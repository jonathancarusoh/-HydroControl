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

    config.maxConsecutiveDoses =
        hydroPreferences.getUChar("maxDoses", 3);

    config.automaticMode =
        hydroPreferences.getBool("autoMode", true);

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
        "maxDoses",
        config.maxConsecutiveDoses
    );

    hydroPreferences.putBool(
        "autoMode",
        config.automaticMode
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

    hydroPreferences.end();
}

// ======================================================
// CREDENCIALES WIFI
// ======================================================
