#include "automatic_ph_dosing.h"

#include "app_state.h"
#include "clock_manager.h"
#include "config.h"
#include "event_logger.h"
#include "manual_ph_dosing.h"
#include "ph_sensor.h"
#include "profile_manager.h"
#include "pump_outputs.h"
#include "utils.h"

#include <Preferences.h>
#include <math.h>

namespace
{
constexpr uint8_t MAX_TRACKED_AUTO_DOSES = 10;
constexpr uint32_t AUTO_WINDOW_SECONDS = 24UL * 60UL * 60UL;
constexpr unsigned long AUTO_BOOT_HOLDOFF_MS = 30000UL;
constexpr unsigned long AUTO_STABLE_HOLDOFF_MS = 15000UL;

Preferences automaticPreferences;

enum class AutomaticDirection : uint8_t
{
    None,
    Minus,
    Plus
};

struct AutomaticRuntime
{
    bool initialized = false;
    bool active = false;
    AutomaticDirection direction = AutomaticDirection::None;
    unsigned long doseStartedAt = 0;
    unsigned long stableSinceAt = 0;
    uint32_t doseEpochs[MAX_TRACKED_AUTO_DOSES] = {};
    uint8_t doseEpochCount = 0;
};

AutomaticRuntime runtime;

const char* directionCode(AutomaticDirection direction)
{
    switch (direction)
    {
        case AutomaticDirection::Minus:
            return "minus";
        case AutomaticDirection::Plus:
            return "plus";
        case AutomaticDirection::None:
        default:
            return "none";
    }
}

const char* directionLabel(AutomaticDirection direction)
{
    switch (direction)
    {
        case AutomaticDirection::Minus:
            return "pH-";
        case AutomaticDirection::Plus:
            return "pH+";
        case AutomaticDirection::None:
        default:
            return "pH";
    }
}

void loadDoseHistory()
{
    automaticPreferences.begin("phauto", true);

    const size_t expectedBytes = sizeof(runtime.doseEpochs);
    const size_t storedBytes = automaticPreferences.getBytesLength("epochs");

    if (storedBytes == expectedBytes)
    {
        automaticPreferences.getBytes(
            "epochs",
            runtime.doseEpochs,
            expectedBytes
        );

        runtime.doseEpochCount = automaticPreferences.getUChar(
            "count",
            0
        );

        if (runtime.doseEpochCount > MAX_TRACKED_AUTO_DOSES)
        {
            runtime.doseEpochCount = 0;
        }
    }

    automaticPreferences.end();
}

void saveDoseHistory()
{
    automaticPreferences.begin("phauto", false);
    automaticPreferences.putBytes(
        "epochs",
        runtime.doseEpochs,
        sizeof(runtime.doseEpochs)
    );
    automaticPreferences.putUChar(
        "count",
        runtime.doseEpochCount
    );
    automaticPreferences.end();
}

void purgeExpiredDoseHistory(uint32_t nowEpoch)
{
    if (nowEpoch == 0 || runtime.doseEpochCount == 0)
    {
        return;
    }

    uint32_t kept[MAX_TRACKED_AUTO_DOSES] = {};
    uint8_t keptCount = 0;

    for (uint8_t i = 0; i < runtime.doseEpochCount; i++)
    {
        const uint32_t epoch = runtime.doseEpochs[i];

        if (epoch == 0)
        {
            continue;
        }

        // Si el reloj fue atrasado manualmente conservamos las marcas futuras
        // para no liberar cuota automática por un cambio de hora.
        if (
            epoch > nowEpoch ||
            nowEpoch - epoch < AUTO_WINDOW_SECONDS
        )
        {
            kept[keptCount++] = epoch;
        }
    }

    bool changed = keptCount != runtime.doseEpochCount;

    for (uint8_t i = 0; i < MAX_TRACKED_AUTO_DOSES; i++)
    {
        const uint32_t value = i < keptCount ? kept[i] : 0;

        if (runtime.doseEpochs[i] != value)
        {
            changed = true;
        }

        runtime.doseEpochs[i] = value;
    }

    runtime.doseEpochCount = keptCount;

    if (changed)
    {
        saveDoseHistory();
    }
}

uint32_t latestDoseEpoch()
{
    if (runtime.doseEpochCount == 0)
    {
        return 0;
    }

    return runtime.doseEpochs[runtime.doseEpochCount - 1];
}

void recordAutomaticDoseEpoch(uint32_t epoch)
{
    if (epoch == 0)
    {
        return;
    }

    purgeExpiredDoseHistory(epoch);

    if (runtime.doseEpochCount < MAX_TRACKED_AUTO_DOSES)
    {
        runtime.doseEpochs[runtime.doseEpochCount++] = epoch;
    }
    else
    {
        for (uint8_t i = 1; i < MAX_TRACKED_AUTO_DOSES; i++)
        {
            runtime.doseEpochs[i - 1] = runtime.doseEpochs[i];
        }

        runtime.doseEpochs[MAX_TRACKED_AUTO_DOSES - 1] = epoch;
    }

    saveDoseHistory();
}

void setAutomaticPumpOutput(AutomaticDirection direction, bool enabled)
{
    setPhPumpOutputs(
        enabled && direction == AutomaticDirection::Minus,
        enabled && direction == AutomaticDirection::Plus
    );
}

void stopActiveDose(const String& reason, bool completed)
{
    if (!runtime.active)
    {
        return;
    }

    const AutomaticDirection direction = runtime.direction;
    setAutomaticPumpOutput(direction, false);

    runtime.active = false;
    runtime.direction = AutomaticDirection::None;
    runtime.doseStartedAt = 0;

    if (completed)
    {
        const uint32_t nowEpoch = getCurrentClockEpoch();
        recordAutomaticDoseEpoch(nowEpoch);

        recordDosageEvent(
            directionLabel(direction),
            config.doseDurationMs,
            true
        );
    }
    else
    {
        String detail = directionLabel(direction);
        detail += " · ";
        detail += reason;

        logEvent(
            "dosage",
            "Dosificación automática cancelada",
            detail
        );
    }
}

AutomaticDirection requiredDirection()
{
    if (!isfinite(currentPh))
    {
        return AutomaticDirection::None;
    }

    if (currentPh > config.targetPh + config.phTolerance)
    {
        return AutomaticDirection::Minus;
    }

    if (currentPh < config.targetPh - config.phTolerance)
    {
        return AutomaticDirection::Plus;
    }

    return AutomaticDirection::None;
}

uint32_t intervalRemainingSeconds(uint32_t nowEpoch)
{
    const uint32_t lastEpoch = latestDoseEpoch();

    if (lastEpoch == 0 || nowEpoch <= lastEpoch)
    {
        return lastEpoch == 0
            ? 0
            : config.doseIntervalMinutes * 60UL;
    }

    const uint32_t requiredSeconds =
        config.doseIntervalMinutes * 60UL;
    const uint32_t elapsed = nowEpoch - lastEpoch;

    return elapsed >= requiredSeconds
        ? 0
        : requiredSeconds - elapsed;
}

String currentBlockReason()
{
    if (!config.automaticMode)
    {
        return "disabled";
    }

    if (isManualPhDosingActive())
    {
        return "manual_active";
    }

    if (!clockConfigured || getCurrentClockEpoch() == 0)
    {
        return "clock_required";
    }

    if (!isPhSensorCalibrated())
    {
        return "sensor_not_calibrated";
    }

    if (isPhSensorSaturated())
    {
        return "sensor_saturated";
    }

    if (!isPhSensorStable() || !isfinite(currentPh))
    {
        return "sensor_unstable";
    }

    if (millis() < AUTO_BOOT_HOLDOFF_MS)
    {
        return "startup_holdoff";
    }

    if (
        runtime.stableSinceAt == 0 ||
        millis() - runtime.stableSinceAt < AUTO_STABLE_HOLDOFF_MS
    )
    {
        return "stability_holdoff";
    }

    const uint32_t nowEpoch = getCurrentClockEpoch();
    purgeExpiredDoseHistory(nowEpoch);

    if (runtime.doseEpochCount >= config.maxDailyDoses)
    {
        return "daily_limit";
    }

    if (intervalRemainingSeconds(nowEpoch) > 0)
    {
        return "interval";
    }

    if (requiredDirection() == AutomaticDirection::None)
    {
        return "within_range";
    }

    return "ready";
}

String blockReasonLabel(const String& reason)
{
    if (reason == "disabled") return "Automático desactivado";
    if (reason == "manual_active") return "Dosificación manual en curso";
    if (reason == "clock_required") return "Configurá el reloj para controlar la ventana de 24 h";
    if (reason == "sensor_not_calibrated") return "Sensor sin calibrar";
    if (reason == "sensor_saturated") return "ADC saturado";
    if (reason == "sensor_unstable") return "Señal de pH inestable";
    if (reason == "startup_holdoff") return "Esperando estabilización después del arranque";
    if (reason == "stability_holdoff") return "Confirmando estabilidad de la lectura";
    if (reason == "daily_limit") return "Máximo automático de 24 h alcanzado";
    if (reason == "interval") return "Esperando el intervalo entre dosis";
    if (reason == "within_range") return "pH dentro del rango objetivo";
    if (reason == "ready") return "Listo para corregir";

    return "Estado desconocido";
}

String buildAutomaticStatusJson()
{
    const uint32_t nowEpoch = getCurrentClockEpoch();

    if (nowEpoch > 0)
    {
        purgeExpiredDoseHistory(nowEpoch);
    }

    const String reason = runtime.active
        ? "dosing"
        : currentBlockReason();

    String json = "{";
    json += "\"success\":true";
    json += ",\"enabled\":";
    json += config.automaticMode ? "true" : "false";
    json += ",\"active\":";
    json += runtime.active ? "true" : "false";
    json += ",\"direction\":\"";
    json += directionCode(runtime.direction);
    json += "\"";
    json += ",\"directionLabel\":\"";
    json += directionLabel(runtime.direction);
    json += "\"";
    json += ",\"blockReason\":\"";
    json += reason;
    json += "\"";
    json += ",\"blockReasonLabel\":\"";
    json += escapeJson(
        runtime.active
            ? String("Dosificando ") + directionLabel(runtime.direction)
            : blockReasonLabel(reason)
    );
    json += "\"";
    json += ",\"clockReady\":";
    json += (clockConfigured && nowEpoch > 0) ? "true" : "false";
    json += ",\"sensorCalibrated\":";
    json += isPhSensorCalibrated() ? "true" : "false";
    json += ",\"sensorStable\":";
    json += isPhSensorStable() ? "true" : "false";
    json += ",\"dailyUsed\":" + String(runtime.doseEpochCount);
    json += ",\"dailyLimit\":" + String(config.maxDailyDoses);
    json += ",\"intervalRemainingSeconds\":";
    json += nowEpoch > 0
        ? String(intervalRemainingSeconds(nowEpoch))
        : "0";
    json += ",\"doseDurationMs\":" + String(config.doseDurationMs);
    json += ",\"minusPin\":" + String(PH_MINUS_PUMP_PIN);
    json += ",\"plusPin\":" + String(PH_PLUS_PUMP_PIN);
    json += "}";

    return json;
}
}

void initializeAutomaticPhDosing()
{
    loadDoseHistory();

    // Primera versión que acciona bombas automáticamente: por seguridad,
    // al migrar desde firmware anterior se deja el automático apagado una vez.
    automaticPreferences.begin("phauto", false);
    const bool actuatorMigrationDone =
        automaticPreferences.getBool("actuatorV1", false);

    if (!actuatorMigrationDone)
    {
        automaticPreferences.putBool("actuatorV1", true);
        automaticPreferences.end();

        if (config.automaticMode)
        {
            config.automaticMode = false;
            saveConfig();
            setActiveProfileSlot(-1);
        }

        logEvent(
            "ph",
            "Automático físico preparado",
            "Quedó desactivado por seguridad después de la actualización; activalo manualmente cuando termines las pruebas."
        );
    }
    else
    {
        automaticPreferences.end();
    }

    runtime.initialized = true;
}

void processAutomaticPhDosing()
{
    if (!runtime.initialized)
    {
        return;
    }

    const unsigned long now = millis();

    if (runtime.active)
    {
        if (!config.automaticMode)
        {
            stopActiveDose("modo automático desactivado", false);
            runtime.stableSinceAt = 0;
            return;
        }

        if (
            static_cast<uint32_t>(now - runtime.doseStartedAt) >=
            config.doseDurationMs
        )
        {
            stopActiveDose("", true);
        }

        return;
    }

    // Al activar el automático exigimos nuevamente un período continuo
    // de lectura estable; no acumulamos ese tiempo mientras está apagado.
    if (!config.automaticMode)
    {
        runtime.stableSinceAt = 0;
        return;
    }

    if (isPhSensorStable() && isfinite(currentPh))
    {
        if (runtime.stableSinceAt == 0)
        {
            runtime.stableSinceAt = now;
        }
    }
    else
    {
        runtime.stableSinceAt = 0;
    }

    const String reason = currentBlockReason();

    if (reason != "ready")
    {
        return;
    }

    const AutomaticDirection direction = requiredDirection();

    if (direction == AutomaticDirection::None)
    {
        return;
    }

    runtime.direction = direction;
    runtime.active = true;
    runtime.doseStartedAt = now;

    setAutomaticPumpOutput(direction, true);

    String detail = directionLabel(direction);
    detail += " · pH actual ";
    detail += String(currentPh, 2);
    detail += " · objetivo ";
    detail += String(config.targetPh, 2);
    detail += " · ";
    detail += String(config.doseDurationMs / 1000.0f, 1);
    detail += " s";

    logEvent(
        "dosage",
        "Dosificación automática iniciada",
        detail
    );
}

bool isAutomaticPhDosingActive()
{
    return runtime.active;
}

void handleGetAutomaticPhDoseStatus()
{
    server.sendHeader("Cache-Control", "no-store");
    server.send(
        200,
        "application/json; charset=utf-8",
        buildAutomaticStatusJson()
    );
}
