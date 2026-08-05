#include "ph.h"
#include "app_state.h"
#include "clock_manager.h"
#include "config.h"
#include "event_logger.h"
#include "profile_manager.h"
#include "utils.h"

// ======================================================
// API DE CONFIGURACIÓN DE PH
// ======================================================

void handleGetConfig()
{
    String json = "{";

    json += "\"targetPh\":" +
        String(config.targetPh, 2) + ",";

    json += "\"tolerance\":" +
        String(config.phTolerance, 2) + ",";

    json += "\"doseSeconds\":" +
        String(
            config.doseDurationMs / 1000.0f,
            1
        ) + ",";

    json += "\"intervalMinutes\":" +
        String(config.doseIntervalMinutes) + ",";

    json += "\"maxDoses\":" +
        String(config.maxConsecutiveDoses) + ",";

    json += "\"automaticMode\":";
    json += config.automaticMode
        ? "true"
        : "false";

    json += ",\"targetEc\":" +
        String(config.targetEc, 2);

    json += ",\"lightOn\":\"";
    json += formatTime(
        config.lightOnHour,
        config.lightOnMinute
    );
    json += "\"";

    json += ",\"lightOff\":\"";
    json += formatTime(
        config.lightOffHour,
        config.lightOffMinute
    );
    json += "\"";

    json += ",\"lightScheduleEnabled\":";
    json += config.lightScheduleEnabled
        ? "true"
        : "false";

    json += ",\"activeProfileId\":" +
        String(static_cast<int>(getActiveProfileSlot()));

    json += "}";

    server.sendHeader(
        "Cache-Control",
        "no-store"
    );

    server.send(
        200,
        "application/json",
        json
    );
}

void handleSaveConfig()
{
    if (
        !server.hasArg("targetPh") ||
        !server.hasArg("tolerance") ||
        !server.hasArg("doseSeconds") ||
        !server.hasArg("intervalMinutes") ||
        !server.hasArg("maxDoses") ||
        !server.hasArg("automaticMode")
    )
    {
        server.send(
            400,
            "application/json",
            "{\"success\":false,"
            "\"message\":\"Faltan datos\"}"
        );

        return;
    }

    float targetPh =
        server.arg("targetPh").toFloat();

    float tolerance =
        server.arg("tolerance").toFloat();

    float doseSeconds =
        server.arg("doseSeconds").toFloat();

    int intervalMinutes =
        server.arg("intervalMinutes").toInt();

    int maxDoses =
        server.arg("maxDoses").toInt();

    bool automaticMode =
        server.arg("automaticMode") == "true";

    if (
        targetPh < 4.0f ||
        targetPh > 8.0f ||
        tolerance < 0.01f ||
        tolerance > 1.0f ||
        doseSeconds < 0.1f ||
        doseSeconds > 30.0f ||
        intervalMinutes < 1 ||
        intervalMinutes > 120 ||
        maxDoses < 1 ||
        maxDoses > 10
    )
    {
        server.send(
            400,
            "application/json",
            "{\"success\":false,"
            "\"message\":\"Valores fuera de rango\"}"
        );

        return;
    }

    float previousTargetPh = config.targetPh;
    float previousTolerance = config.phTolerance;
    uint32_t previousDoseDuration = config.doseDurationMs;
    uint32_t previousDoseInterval = config.doseIntervalMinutes;
    uint8_t previousMaxDoses = config.maxConsecutiveDoses;
    bool previousAutomaticMode = config.automaticMode;

    config.targetPh = targetPh;
    config.phTolerance = tolerance;

    config.doseDurationMs =
        static_cast<uint32_t>(
            doseSeconds * 1000.0f
        );

    config.doseIntervalMinutes =
        static_cast<uint32_t>(
            intervalMinutes
        );

    config.maxConsecutiveDoses =
        static_cast<uint8_t>(
            maxDoses
        );

    config.automaticMode = automaticMode;

    saveConfig();

    // Una modificación manual deja de coincidir con el perfil
    // que estaba aplicado anteriormente.
    setActiveProfileSlot(-1);

    String changes;

    if (previousTargetPh != config.targetPh)
    {
        changes += "Objetivo ";
        changes += String(previousTargetPh, 2);
        changes += " → ";
        changes += String(config.targetPh, 2);
    }

    if (previousTolerance != config.phTolerance)
    {
        if (!changes.isEmpty()) changes += " · ";
        changes += "Tolerancia ";
        changes += String(previousTolerance, 2);
        changes += " → ";
        changes += String(config.phTolerance, 2);
    }

    if (previousDoseDuration != config.doseDurationMs)
    {
        if (!changes.isEmpty()) changes += " · ";
        changes += "Dosis ";
        changes += String(previousDoseDuration / 1000.0f, 2);
        changes += " s → ";
        changes += String(config.doseDurationMs / 1000.0f, 2);
        changes += " s";
    }

    if (previousDoseInterval != config.doseIntervalMinutes)
    {
        if (!changes.isEmpty()) changes += " · ";
        changes += "Intervalo ";
        changes += String(previousDoseInterval);
        changes += " → ";
        changes += String(config.doseIntervalMinutes);
        changes += " min";
    }

    if (previousMaxDoses != config.maxConsecutiveDoses)
    {
        if (!changes.isEmpty()) changes += " · ";
        changes += "Máximo ";
        changes += String(previousMaxDoses);
        changes += " → ";
        changes += String(config.maxConsecutiveDoses);
    }

    if (previousAutomaticMode != config.automaticMode)
    {
        if (!changes.isEmpty()) changes += " · ";
        changes += config.automaticMode
            ? "Automático activado"
            : "Automático desactivado";
    }

    if (!changes.isEmpty())
    {
        logEvent(
            "ph",
            "Configuración de pH actualizada",
            changes
        );
    }


    server.send(
        200,
        "application/json",
        "{\"success\":true,"
        "\"message\":\"Configuración guardada\"}"
    );
}

// ======================================================
// API DE SISTEMA Y DIAGNÓSTICO
// ======================================================
