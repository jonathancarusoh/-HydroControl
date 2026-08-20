#include "config.h"
#include "app_state.h"
#include <Preferences.h>

namespace { Preferences hydroPreferences; }

// ======================================================
// CONFIGURACIÓN PERSISTENTE
// ======================================================

void loadConfig()
{
    hydroPreferences.begin("hydrocontrol", true);

    config.targetPh =
        hydroPreferences.getFloat("targetPh", 5.80f);

    config.phTolerance =
        hydroPreferences.getFloat("tolerance", 0.10f);

    config.doseDurationMs =
        hydroPreferences.getUInt("doseMs", 500);

    config.doseIntervalMinutes =
        hydroPreferences.getUInt("intervalMin", 4);

    config.maxDailyDoses = hydroPreferences.isKey("maxDaily")
        ? hydroPreferences.getUChar("maxDaily", 3)
        : hydroPreferences.getUChar("maxDoses", 3);

    config.automaticMode =
        hydroPreferences.getBool("autoMode", true);

    config.manualDoseDurationMs =
        hydroPreferences.getUInt("manualDoseMs", 1000);

    config.manualMaxDoses =
        hydroPreferences.getUChar("manualMax", 3);

    config.targetEc =
        hydroPreferences.getFloat("targetEc", 1.40f);

    config.lightOnHour =
        hydroPreferences.getUChar("lightOnH", 6);

    config.lightOnMinute =
        hydroPreferences.getUChar("lightOnM", 0);

    config.lightOffHour =
        hydroPreferences.getUChar("lightOffH", 18);

    config.lightOffMinute =
        hydroPreferences.getUChar("lightOffM", 0);

    config.lightScheduleEnabled =
        hydroPreferences.getBool("lightEnabled", false);

    config.lightManualOn =
        hydroPreferences.getBool("lightManual", false);

    hydroPreferences.end();
}

void saveConfig()
{
    hydroPreferences.begin("hydrocontrol", false);

    hydroPreferences.putFloat(
        "targetPh",
        config.targetPh
    );

    hydroPreferences.putFloat(
        "tolerance",
        config.phTolerance
    );

    hydroPreferences.putUInt(
        "doseMs",
        config.doseDurationMs
    );

    hydroPreferences.putUInt(
        "intervalMin",
        config.doseIntervalMinutes
    );

    hydroPreferences.putUChar(
        "maxDaily",
        config.maxDailyDoses
    );

    // Compatibilidad con firmware anterior a la migración del nombre.
    hydroPreferences.putUChar(
        "maxDoses",
        config.maxDailyDoses
    );

    hydroPreferences.putBool(
        "autoMode",
        config.automaticMode
    );

    hydroPreferences.putUInt(
        "manualDoseMs",
        config.manualDoseDurationMs
    );

    hydroPreferences.putUChar(
        "manualMax",
        config.manualMaxDoses
    );

    hydroPreferences.putFloat(
        "targetEc",
        config.targetEc
    );

    hydroPreferences.putUChar(
        "lightOnH",
        config.lightOnHour
    );

    hydroPreferences.putUChar(
        "lightOnM",
        config.lightOnMinute
    );

    hydroPreferences.putUChar(
        "lightOffH",
        config.lightOffHour
    );

    hydroPreferences.putUChar(
        "lightOffM",
        config.lightOffMinute
    );

    hydroPreferences.putBool(
        "lightEnabled",
        config.lightScheduleEnabled
    );

    hydroPreferences.putBool(
        "lightManual",
        config.lightManualOn
    );

    hydroPreferences.end();
}

// ======================================================
// CREDENCIALES WIFI
// ======================================================
