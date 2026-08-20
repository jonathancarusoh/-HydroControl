#include "ph.h"
#include "app_state.h"
#include "clock_manager.h"
#include "config.h"
#include "event_logger.h"
#include "manual_ph_dosing.h"
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

    json += "\"maxDailyDoses\":" +
        String(config.maxDailyDoses) + ",";

    // Alias temporal para interfaces anteriores a la migración de 24 h.
    json += "\"maxDoses\":" +
        String(config.maxDailyDoses) + ",";

    json += "\"automaticMode\":";
    json += config.automaticMode
        ? "true"
        : "false";

    json += ",\"manualDoseSeconds\":" +
        String(config.manualDoseDurationMs / 1000.0f, 1);

    json += ",\"manualMaxDoses\":" +
        String(config.manualMaxDoses);

    json += ",\"manualDosingActive\":";
    json += isManualPhDosingActive()
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

    json += ",\"lightManualOn\":";
    json += config.lightManualOn
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
        (
            !server.hasArg("maxDailyDoses") &&
            !server.hasArg("maxDoses")
        ) ||
        !server.hasArg("automaticMode") ||
        !server.hasArg("manualDoseSeconds") ||
        !server.hasArg("manualMaxDoses")
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

    int maxDailyDoses = server.hasArg("maxDailyDoses")
        ? server.arg("maxDailyDoses").toInt()
        : server.arg("maxDoses").toInt();

    bool automaticMode =
        server.arg("automaticMode") == "true";

    float manualDoseSeconds =
        server.arg("manualDoseSeconds").toFloat();

    int manualMaxDoses =
        server.arg("manualMaxDoses").toInt();

    if (
        automaticMode &&
        isManualPhDosingActive()
    )
    {
        server.send(
            409,
            "application/json",
            "{\"success\":false,\"message\":\"No se puede activar el modo automático durante una secuencia manual.\"}"
        );
        return;
    }

    if (
        targetPh < 4.0f ||
        targetPh > 8.0f ||
        tolerance < 0.01f ||
        tolerance > 1.0f ||
        doseSeconds < 0.1f ||
        doseSeconds > 30.0f ||
        intervalMinutes < 1 ||
        intervalMinutes > 120 ||
        maxDailyDoses < 1 ||
        maxDailyDoses > 10 ||
        manualDoseSeconds < 0.1f ||
        manualDoseSeconds > 30.0f ||
        manualMaxDoses < 1 ||
        manualMaxDoses > 10
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
    uint8_t previousMaxDailyDoses = config.maxDailyDoses;
    bool previousAutomaticMode = config.automaticMode;
    uint32_t previousManualDoseDuration =
        config.manualDoseDurationMs;
    uint8_t previousManualMaxDoses =
        config.manualMaxDoses;

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

    config.maxDailyDoses =
        static_cast<uint8_t>(
            maxDailyDoses
        );

    config.automaticMode = automaticMode;

    config.manualDoseDurationMs =
        static_cast<uint32_t>(manualDoseSeconds * 1000.0f);

    config.manualMaxDoses =
        static_cast<uint8_t>(manualMaxDoses);

    saveConfig();

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

    if (previousMaxDailyDoses != config.maxDailyDoses)
    {
        if (!changes.isEmpty()) changes += " · ";
        changes += "Máximo en 24 h ";
        changes += String(previousMaxDailyDoses);
        changes += " → ";
        changes += String(config.maxDailyDoses);
        changes += " dosis";
    }

    if (previousAutomaticMode != config.automaticMode)
    {
        if (!changes.isEmpty()) changes += " · ";
        changes += config.automaticMode
            ? "Automático activado"
            : "Automático desactivado";
    }

    if (previousManualDoseDuration != config.manualDoseDurationMs)
    {
        if (!changes.isEmpty()) changes += " · ";
        changes += "Dosis manual ";
        changes += String(previousManualDoseDuration / 1000.0f, 1);
        changes += " s → ";
        changes += String(config.manualDoseDurationMs / 1000.0f, 1);
        changes += " s";
    }

    if (previousManualMaxDoses != config.manualMaxDoses)
    {
        if (!changes.isEmpty()) changes += " · ";
        changes += "Límite manual ";
        changes += String(previousManualMaxDoses);
        changes += " → ";
        changes += String(config.manualMaxDoses);
    }

    if (!changes.isEmpty())
    {
        clearActiveProfileIfConfigChanged();

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
