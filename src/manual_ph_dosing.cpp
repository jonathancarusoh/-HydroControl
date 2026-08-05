#include "manual_ph_dosing.h"

#include "app_state.h"
#include "config.h"
#include "event_logger.h"
#include "profile_manager.h"
#include "utils.h"

namespace
{
struct ManualDoseRuntime
{
    bool active = false;
    bool outputActive = false;
    ManualPhDirection direction = ManualPhDirection::None;
    uint8_t totalDoses = 0;
    uint8_t completedDoses = 0;
    uint32_t doseDurationMs = 0;
    unsigned long currentDoseStartedAt = 0;
    unsigned long sequenceStartedAt = 0;
};

ManualDoseRuntime runtime;

const char* directionCode(ManualPhDirection direction)
{
    switch (direction)
    {
        case ManualPhDirection::Minus:
            return "minus";

        case ManualPhDirection::Plus:
            return "plus";

        case ManualPhDirection::None:
        default:
            return "none";
    }
}

const char* directionLabel(ManualPhDirection direction)
{
    switch (direction)
    {
        case ManualPhDirection::Minus:
            return "pH-";

        case ManualPhDirection::Plus:
            return "pH+";

        case ManualPhDirection::None:
        default:
            return "pH";
    }
}

ManualPhDirection parseDirection(const String& value)
{
    if (value == "minus")
    {
        return ManualPhDirection::Minus;
    }

    if (value == "plus")
    {
        return ManualPhDirection::Plus;
    }

    return ManualPhDirection::None;
}

// Punto único preparado para conectar el GPIO/MOSFET o relé en el futuro.
// Por ahora solamente mantiene el estado lógico de la salida.
void setManualPumpOutput(
    ManualPhDirection direction,
    bool enabled
)
{
    (void) direction;
    runtime.outputActive = enabled;
}

void beginCurrentDose(unsigned long now)
{
    runtime.currentDoseStartedAt = now;
    setManualPumpOutput(runtime.direction, true);
}

void resetRuntime()
{
    setManualPumpOutput(runtime.direction, false);

    runtime.active = false;
    runtime.direction = ManualPhDirection::None;
    runtime.totalDoses = 0;
    runtime.completedDoses = 0;
    runtime.doseDurationMs = 0;
    runtime.currentDoseStartedAt = 0;
    runtime.sequenceStartedAt = 0;
}

String buildStatusJson()
{
    unsigned long now = millis();
    uint32_t elapsedMs = 0;
    uint32_t remainingMs = 0;
    uint32_t sequenceRemainingMs = 0;
    uint8_t remainingDoses = 0;
    uint8_t currentDoseNumber = 0;
    uint8_t progressPercent = 0;

    if (runtime.active)
    {
        elapsedMs = static_cast<uint32_t>(
            now - runtime.currentDoseStartedAt
        );

        if (elapsedMs > runtime.doseDurationMs)
        {
            elapsedMs = runtime.doseDurationMs;
        }

        remainingMs = runtime.doseDurationMs - elapsedMs;
        remainingDoses = runtime.totalDoses - runtime.completedDoses;
        currentDoseNumber = runtime.completedDoses + 1;

        uint8_t futureDoses =
            runtime.totalDoses - currentDoseNumber;

        sequenceRemainingMs =
            remainingMs +
            static_cast<uint32_t>(futureDoses) *
            runtime.doseDurationMs;

        if (runtime.doseDurationMs > 0)
        {
            progressPercent = static_cast<uint8_t>(
                (static_cast<uint64_t>(elapsedMs) * 100ULL) /
                runtime.doseDurationMs
            );
        }
    }

    String json = "{";
    json += "\"success\":true,";
    json += "\"active\":";
    json += runtime.active ? "true" : "false";
    json += ",\"direction\":\"";
    json += directionCode(runtime.direction);
    json += "\"";
    json += ",\"directionLabel\":\"";
    json += directionLabel(runtime.direction);
    json += "\"";
    json += ",\"outputActive\":";
    json += runtime.outputActive ? "true" : "false";
    json += ",\"totalDoses\":" + String(runtime.totalDoses);
    json += ",\"completedDoses\":" + String(runtime.completedDoses);
    json += ",\"remainingDoses\":" + String(remainingDoses);
    json += ",\"currentDoseNumber\":" + String(currentDoseNumber);
    json += ",\"doseDurationMs\":" + String(runtime.doseDurationMs);
    json += ",\"elapsedMs\":" + String(elapsedMs);
    json += ",\"remainingMs\":" + String(remainingMs);
    json += ",\"sequenceRemainingMs\":" + String(sequenceRemainingMs);
    json += ",\"progressPercent\":" + String(progressPercent);
    json += ",\"automaticMode\":";
    json += config.automaticMode ? "true" : "false";
    json += "}";

    return json;
}

void sendStatus(int statusCode = 200)
{
    server.sendHeader("Cache-Control", "no-store");
    server.send(
        statusCode,
        "application/json; charset=utf-8",
        buildStatusJson()
    );
}
}

bool isManualPhDosingActive()
{
    return runtime.active;
}

void processManualPhDosing()
{
    if (!runtime.active)
    {
        return;
    }

    unsigned long now = millis();

    if (
        static_cast<uint32_t>(
            now - runtime.currentDoseStartedAt
        ) < runtime.doseDurationMs
    )
    {
        return;
    }

    setManualPumpOutput(runtime.direction, false);
    runtime.completedDoses++;

    recordDosageEvent(
        directionLabel(runtime.direction),
        runtime.doseDurationMs,
        false
    );

    if (runtime.completedDoses < runtime.totalDoses)
    {
        beginCurrentDose(now);
        return;
    }

    String detail = directionLabel(runtime.direction);
    detail += " · ";
    detail += String(runtime.totalDoses);
    detail += runtime.totalDoses == 1
        ? " dosis completada"
        : " dosis completadas";
    detail += " · ";
    detail += String(
        (static_cast<uint64_t>(runtime.totalDoses) *
        runtime.doseDurationMs) / 1000.0f,
        1
    );
    detail += " s totales";

    logEvent(
        "dosage",
        "Secuencia manual finalizada",
        detail
    );

    resetRuntime();
}

void handleGetManualPhDoseStatus()
{
    sendStatus();
}

void handleStartManualPhDose()
{
    if (runtime.active)
    {
        server.send(
            409,
            "application/json; charset=utf-8",
            "{\"success\":false,\"message\":\"Ya existe una secuencia manual en curso.\"}"
        );
        return;
    }

    if (
        !server.hasArg("direction") ||
        !server.hasArg("doses")
    )
    {
        server.send(
            400,
            "application/json; charset=utf-8",
            "{\"success\":false,\"message\":\"Faltan los datos de la dosificación.\"}"
        );
        return;
    }

    ManualPhDirection direction =
        parseDirection(server.arg("direction"));

    int requestedDoses = server.arg("doses").toInt();

    if (direction == ManualPhDirection::None)
    {
        server.send(
            400,
            "application/json; charset=utf-8",
            "{\"success\":false,\"message\":\"Dirección de dosificación inválida.\"}"
        );
        return;
    }

    if (
        requestedDoses < 1 ||
        requestedDoses > config.manualMaxDoses
    )
    {
        String message = "La cantidad debe estar entre 1 y ";
        message += String(config.manualMaxDoses);
        message += ".";

        String json = "{";
        json += "\"success\":false,";
        json += "\"message\":\"";
        json += escapeJson(message);
        json += "\"}";

        server.send(
            400,
            "application/json; charset=utf-8",
            json
        );
        return;
    }

    if (
        config.manualDoseDurationMs < 100 ||
        config.manualDoseDurationMs > 30000
    )
    {
        server.send(
            400,
            "application/json; charset=utf-8",
            "{\"success\":false,\"message\":\"La duración manual guardada no es válida.\"}"
        );
        return;
    }

    bool automaticWasEnabled = config.automaticMode;

    if (automaticWasEnabled)
    {
        config.automaticMode = false;
        saveConfig();
        setActiveProfileSlot(-1);

        logEvent(
            "ph",
            "Modo automático pausado",
            "Se inició una dosificación manual"
        );
    }

    runtime.active = true;
    runtime.direction = direction;
    runtime.totalDoses = static_cast<uint8_t>(requestedDoses);
    runtime.completedDoses = 0;
    runtime.doseDurationMs = config.manualDoseDurationMs;
    runtime.sequenceStartedAt = millis();

    beginCurrentDose(runtime.sequenceStartedAt);

    String detail = directionLabel(direction);
    detail += " · ";
    detail += String(requestedDoses);
    detail += requestedDoses == 1 ? " dosis" : " dosis";
    detail += " de ";
    detail += String(config.manualDoseDurationMs / 1000.0f, 1);
    detail += " s";

    logEvent(
        "dosage",
        "Secuencia manual iniciada",
        detail
    );

    sendStatus(202);
}

void handleCancelManualPhDose()
{
    if (!runtime.active)
    {
        server.send(
            200,
            "application/json; charset=utf-8",
            "{\"success\":true,\"message\":\"No había una secuencia activa.\"}"
        );
        return;
    }

    ManualPhDirection cancelledDirection = runtime.direction;
    uint8_t completed = runtime.completedDoses;
    uint8_t total = runtime.totalDoses;

    resetRuntime();

    String detail = directionLabel(cancelledDirection);
    detail += " · ";
    detail += String(completed);
    detail += " de ";
    detail += String(total);
    detail += " dosis completadas";

    logEvent(
        "dosage",
        "Secuencia manual cancelada",
        detail
    );

    server.send(
        200,
        "application/json; charset=utf-8",
        "{\"success\":true,\"message\":\"Secuencia cancelada.\"}"
    );
}
