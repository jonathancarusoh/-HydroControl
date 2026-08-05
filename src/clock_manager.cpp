#include "clock_manager.h"
#include "app_state.h"
#include "config.h"
#include "event_logger.h"
#include "profile_manager.h"
#include "utils.h"
#include <Preferences.h>
#include <sys/time.h>

namespace { Preferences clockPreferences; const char* CLOCK_TIMEZONE = "ART3"; }

// ======================================================
// RELOJ INTERNO Y PROGRAMACIÓN DE LUZ
// ======================================================

String formatTime(
    uint8_t hour,
    uint8_t minute
)
{
    char buffer[6];

    snprintf(
        buffer,
        sizeof(buffer),
        "%02u:%02u",
        hour,
        minute
    );

    return String(buffer);
}

bool parseTimeValue(
    const String& value,
    uint8_t& hour,
    uint8_t& minute
)
{
    if (
        value.length() != 5 ||
        value.charAt(2) != ':'
    )
    {
        return false;
    }

    for (uint8_t index = 0; index < 5; index++)
    {
        if (index == 2)
        {
            continue;
        }

        if (!isDigit(value.charAt(index)))
        {
            return false;
        }
    }

    int parsedHour = value.substring(0, 2).toInt();
    int parsedMinute = value.substring(3, 5).toInt();

    if (
        parsedHour < 0 ||
        parsedHour > 23 ||
        parsedMinute < 0 ||
        parsedMinute > 59
    )
    {
        return false;
    }

    hour = static_cast<uint8_t>(parsedHour);
    minute = static_cast<uint8_t>(parsedMinute);

    return true;
}


// ======================================================
// RELOJ INTERNO Y PROGRAMACIÓN DE LUZ
// ======================================================

bool isValidClockEpoch(uint32_t epoch)
{
    // 2024-01-01 a 2100-01-01.
    return epoch >= 1704067200UL &&
        epoch <= 4102444800UL;
}

void applyClockEpoch(uint32_t epoch)
{
    struct timeval value;
    value.tv_sec = static_cast<time_t>(epoch);
    value.tv_usec = 0;

    settimeofday(&value, nullptr);
    clockConfigured = true;
}

void initializeClockRuntime()
{
    setenv("TZ", CLOCK_TIMEZONE, 1);
    tzset();

    clockPreferences.begin("clock", false);

    uint32_t storedEpoch =
        clockPreferences.getULong("epoch", 0);

    bool restoreAfterRestart =
        clockPreferences.getBool("restore", false);

    // La restauración solo vale para un reinicio controlado.
    // Un corte de energía no puede medirse sin un RTC físico.
    clockPreferences.putBool("restore", false);
    clockPreferences.end();

    clockConfigured = false;
    clockRestoredAfterSoftwareRestart = false;

    if (
        restoreAfterRestart &&
        isValidClockEpoch(storedEpoch)
    )
    {
        applyClockEpoch(storedEpoch);
        clockRestoredAfterSoftwareRestart = true;
    }
}

void saveManualClockReference(uint32_t epoch)
{
    clockPreferences.begin("clock", false);
    clockPreferences.putULong("epoch", epoch);
    clockPreferences.putBool("restore", false);
    clockPreferences.end();
}

void preserveClockForSoftwareRestart()
{
    clockPreferences.begin("clock", false);

    if (clockConfigured)
    {
        time_t now = time(nullptr);

        if (
            now > 0 &&
            isValidClockEpoch(
                static_cast<uint32_t>(now)
            )
        )
        {
            clockPreferences.putULong(
                "epoch",
                static_cast<uint32_t>(now)
            );
            clockPreferences.putBool("restore", true);
            clockPreferences.end();
            return;
        }
    }

    clockPreferences.putBool("restore", false);
    clockPreferences.end();
}

bool getCurrentLocalTime(struct tm& timeInfo)
{
    if (!clockConfigured)
    {
        return false;
    }

    time_t now = time(nullptr);

    if (now <= 0)
    {
        return false;
    }

    localtime_r(&now, &timeInfo);
    return true;
}

uint32_t getCurrentClockEpoch()
{
    if (!clockConfigured)
    {
        return 0;
    }

    time_t now = time(nullptr);

    if (now <= 0)
    {
        return 0;
    }

    return static_cast<uint32_t>(now);
}

uint16_t getScheduleDurationMinutes()
{
    int onMinutes =
        config.lightOnHour * 60 +
        config.lightOnMinute;

    int offMinutes =
        config.lightOffHour * 60 +
        config.lightOffMinute;

    int duration = offMinutes - onMinutes;

    if (duration <= 0)
    {
        duration += 24 * 60;
    }

    return static_cast<uint16_t>(duration);
}

bool isLightScheduledOnNow()
{
    struct tm timeInfo;

    if (
        !config.lightScheduleEnabled ||
        !getCurrentLocalTime(timeInfo)
    )
    {
        return false;
    }

    int currentMinutes =
        timeInfo.tm_hour * 60 +
        timeInfo.tm_min;

    int onMinutes =
        config.lightOnHour * 60 +
        config.lightOnMinute;

    int offMinutes =
        config.lightOffHour * 60 +
        config.lightOffMinute;

    if (onMinutes < offMinutes)
    {
        return currentMinutes >= onMinutes &&
            currentMinutes < offMinutes;
    }

    return currentMinutes >= onMinutes ||
        currentMinutes < offMinutes;
}

bool isLightEffectivelyOn()
{
    if (config.lightScheduleEnabled)
    {
        return isLightScheduledOnNow();
    }

    return config.lightManualOn;
}

String getLightControlMode()
{
    return config.lightScheduleEnabled
        ? "automatic"
        : "manual";
}

String getLightStateCode()
{
    if (config.lightScheduleEnabled && !clockConfigured)
    {
        return "unknown";
    }

    if (config.lightScheduleEnabled)
    {
        return isLightScheduledOnNow()
            ? "on"
            : "off";
    }

    return config.lightManualOn
        ? "manual-on"
        : "manual-off";
}

String getLightStateLabel()
{
    String state = getLightStateCode();

    if (state == "unknown")
    {
        return "Reloj sin configurar";
    }

    return isLightEffectivelyOn()
        ? "Lámpara encendida"
        : "Lámpara apagada";
}

String formatRemainingMinutes(int totalMinutes)
{
    int hours = totalMinutes / 60;
    int minutes = totalMinutes % 60;

    String result;

    if (hours > 0)
    {
        result += String(hours) + " h";
    }

    if (minutes > 0 || hours == 0)
    {
        if (!result.isEmpty())
        {
            result += " ";
        }

        result += String(minutes) + " min";
    }

    return result;
}

String getLightNextChangeLabel()
{
    if (!config.lightScheduleEnabled)
    {
        return config.lightManualOn
            ? "Control manual encendido"
            : "Control manual apagado";
    }

    struct tm timeInfo;

    if (!getCurrentLocalTime(timeInfo))
    {
        return "Configurá el reloj";
    }

    int currentMinutes =
        timeInfo.tm_hour * 60 +
        timeInfo.tm_min;

    bool currentlyOn = isLightScheduledOnNow();

    int targetMinutes = currentlyOn
        ? config.lightOffHour * 60 +
            config.lightOffMinute
        : config.lightOnHour * 60 +
            config.lightOnMinute;

    int difference = targetMinutes - currentMinutes;

    if (difference <= 0)
    {
        difference += 24 * 60;
    }

    return String(currentlyOn ? "Apaga en " : "Enciende en ") +
        formatRemainingMinutes(difference);
}

// ======================================================
// REGISTRO PERSISTENTE DE EVENTOS
// ======================================================

void appendClockAndLightJson(String& json)
{
    uint32_t epoch = getCurrentClockEpoch();

    json += "\"clock\":{";
    json += "\"configured\":";
    json += clockConfigured ? "true" : "false";
    json += ",\"epoch\":";

    if (clockConfigured)
    {
        json += String(epoch);
    }
    else
    {
        json += "null";
    }

    json += ",\"restoredAfterRestart\":";
    json += clockRestoredAfterSoftwareRestart
        ? "true"
        : "false";
    json += ",\"timezone\":\"UTC-03:00\"";
    json += "},";

    json += "\"light\":{";
    json += "\"enabled\":";
    json += config.lightScheduleEnabled
        ? "true"
        : "false";
    json += ",\"mode\":\"";
    json += getLightControlMode();
    json += "\"";
    json += ",\"automaticEnabled\":";
    json += config.lightScheduleEnabled
        ? "true"
        : "false";
    json += ",\"manualOn\":";
    json += config.lightManualOn
        ? "true"
        : "false";
    json += ",\"effectiveOn\":";
    json += isLightEffectivelyOn()
        ? "true"
        : "false";
    json += ",\"on\":\"";
    json += formatTime(
        config.lightOnHour,
        config.lightOnMinute
    );
    json += "\",\"off\":\"";
    json += formatTime(
        config.lightOffHour,
        config.lightOffMinute
    );
    json += "\",\"photoperiodMinutes\":" +
        String(getScheduleDurationMinutes());
    json += ",\"stateCode\":\"";
    json += getLightStateCode();
    json += "\",\"stateLabel\":\"";
    json += escapeJson(getLightStateLabel());
    json += "\",\"nextChangeLabel\":\"";
    json += escapeJson(getLightNextChangeLabel());
    json += "\",\"outputAvailable\":false";
    json += "}";
}

void handleGetClockStatus()
{
    String json;
    json.reserve(700);

    json += "{\"success\":true,";
    appendClockAndLightJson(json);
    json += "}";

    server.sendHeader("Cache-Control", "no-store");
    server.send(
        200,
        "application/json; charset=utf-8",
        json
    );
}

void handleSetClock()
{
    if (!server.hasArg("epoch"))
    {
        server.send(
            400,
            "application/json; charset=utf-8",
            "{\"success\":false,"
            "\"message\":\"Falta la fecha y hora.\"}"
        );
        return;
    }

    String epochText = server.arg("epoch");
    char* endPointer = nullptr;

    unsigned long parsedEpoch = strtoul(
        epochText.c_str(),
        &endPointer,
        10
    );

    if (
        endPointer == epochText.c_str() ||
        *endPointer != '\0' ||
        !isValidClockEpoch(
            static_cast<uint32_t>(parsedEpoch)
        )
    )
    {
        server.send(
            400,
            "application/json; charset=utf-8",
            "{\"success\":false,"
            "\"message\":\"La fecha y hora no son válidas.\"}"
        );
        return;
    }

    applyClockEpoch(
        static_cast<uint32_t>(parsedEpoch)
    );

    clockRestoredAfterSoftwareRestart = false;

    saveManualClockReference(
        static_cast<uint32_t>(parsedEpoch)
    );

    struct tm timeInfo;
    char dateTimeBuffer[32] = "hora configurada";

    if (getCurrentLocalTime(timeInfo))
    {
        strftime(
            dateTimeBuffer,
            sizeof(dateTimeBuffer),
            "%d/%m/%Y %H:%M:%S",
            &timeInfo
        );
    }

    logEvent(
        "clock",
        "Reloj configurado",
        String("Hora establecida manualmente: ") +
            dateTimeBuffer
    );

    server.send(
        200,
        "application/json; charset=utf-8",
        "{\"success\":true,"
        "\"message\":\"Hora guardada correctamente.\"}"
    );
}

void handleSaveLightSchedule()
{
    if (
        !server.hasArg("enabled") ||
        !server.hasArg("lightOn") ||
        !server.hasArg("lightOff")
    )
    {
        server.send(
            400,
            "application/json; charset=utf-8",
            "{\"success\":false,"
            "\"message\":\"Faltan datos del horario de luz.\"}"
        );
        return;
    }

    uint8_t onHour;
    uint8_t onMinute;
    uint8_t offHour;
    uint8_t offMinute;

    String lightOn = server.arg("lightOn");
    String lightOff = server.arg("lightOff");

    if (
        !parseTimeValue(lightOn, onHour, onMinute) ||
        !parseTimeValue(lightOff, offHour, offMinute) ||
        lightOn == lightOff
    )
    {
        server.send(
            400,
            "application/json; charset=utf-8",
            "{\"success\":false,"
            "\"message\":\"Los horarios de luz no son válidos.\"}"
        );
        return;
    }

    bool enabled = server.arg("enabled") == "true";

    bool changed =
        config.lightScheduleEnabled != enabled ||
        config.lightOnHour != onHour ||
        config.lightOnMinute != onMinute ||
        config.lightOffHour != offHour ||
        config.lightOffMinute != offMinute;

    config.lightScheduleEnabled = enabled;
    config.lightOnHour = onHour;
    config.lightOnMinute = onMinute;
    config.lightOffHour = offHour;
    config.lightOffMinute = offMinute;

    saveConfig();

    // Un cambio manual del horario deja de coincidir con
    // el perfil que estaba aplicado.
    setActiveProfileSlot(-1);

    if (changed)
    {
        String detail = enabled
            ? "Horario activo · "
            : "Horario desactivado · ";

        detail += lightOn;
        detail += " a ";
        detail += lightOff;
        detail += " · ";
        detail += String(getScheduleDurationMinutes() / 60);
        detail += " h ";
        detail += String(getScheduleDurationMinutes() % 60);
        detail += " min";

        logEvent(
            "light",
            "Programación de luz actualizada",
            detail
        );
    }

    server.send(
        200,
        "application/json; charset=utf-8",
        "{\"success\":true,"
        "\"message\":\"Programación de luz guardada.\"}"
    );
}

void handleSetManualLight()
{
    if (!server.hasArg("state"))
    {
        server.send(
            400,
            "application/json; charset=utf-8",
            "{\"success\":false,"
            "\"message\":\"Falta indicar el estado manual.\"}"
        );
        return;
    }

    String stateText = server.arg("state");

    if (
        stateText != "true" &&
        stateText != "false" &&
        stateText != "1" &&
        stateText != "0"
    )
    {
        server.send(
            400,
            "application/json; charset=utf-8",
            "{\"success\":false,"
            "\"message\":\"El estado manual no es válido.\"}"
        );
        return;
    }

    bool requestedOn =
        stateText == "true" ||
        stateText == "1";

    bool automaticWasEnabled =
        config.lightScheduleEnabled;

    bool changed =
        automaticWasEnabled ||
        config.lightManualOn != requestedOn;

    config.lightScheduleEnabled = false;
    config.lightManualOn = requestedOn;

    saveConfig();
    setActiveProfileSlot(-1);

    if (changed)
    {
        String detail = requestedOn
            ? "Control manual encendido"
            : "Control manual apagado";

        if (automaticWasEnabled)
        {
            detail += " · Programación automática desactivada";
        }

        logEvent(
            "light",
            requestedOn
                ? "Lámpara encendida manualmente"
                : "Lámpara apagada manualmente",
            detail
        );
    }

    String json = "{";
    json += "\"success\":true,";
    json += "\"message\":\"";
    json += requestedOn
        ? "Control manual encendido."
        : "Control manual apagado.";
    json += "\",";
    json += "\"automaticDisabled\":";
    json += automaticWasEnabled
        ? "true"
        : "false";
    json += ",\"manualOn\":";
    json += requestedOn
        ? "true"
        : "false";
    json += "}";

    server.sendHeader("Cache-Control", "no-store");
    server.send(
        200,
        "application/json; charset=utf-8",
        json
    );
}

