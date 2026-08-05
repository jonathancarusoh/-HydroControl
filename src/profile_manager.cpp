#include "profile_manager.h"
#include "app_state.h"
#include "clock_manager.h"
#include "config.h"
#include "event_logger.h"
#include "utils.h"
#include <Preferences.h>

namespace { Preferences profilePreferences; }

// ======================================================
// PERFILES DE CULTIVO
// ======================================================

String profileKey(
    const char* prefix,
    uint8_t slot
)
{
    return String(prefix) + String(slot);
}

int8_t getActiveProfileSlot()
{
    profilePreferences.begin("profiles", true);

    int8_t slot =
        profilePreferences.getChar("active", -1);

    profilePreferences.end();

    if (
        slot < 0 ||
        slot >= MAX_CULTIVATION_PROFILES
    )
    {
        return -1;
    }

    return slot;
}

void setActiveProfileSlot(int8_t slot)
{
    profilePreferences.begin("profiles", false);
    profilePreferences.putChar("active", slot);
    profilePreferences.end();
}

bool loadProfile(
    uint8_t slot,
    CultivationProfile& profile
)
{
    if (slot >= MAX_CULTIVATION_PROFILES)
    {
        return false;
    }

    profilePreferences.begin("profiles", true);

    String usedKey = profileKey("u", slot);

    profile.used = profilePreferences.getBool(
        usedKey.c_str(),
        false
    );

    if (!profile.used)
    {
        profilePreferences.end();
        return false;
    }

    String nameKey = profileKey("n", slot);
    String targetPhKey = profileKey("ph", slot);
    String toleranceKey = profileKey("pt", slot);
    String durationKey = profileKey("dm", slot);
    String intervalKey = profileKey("di", slot);
    String maxDosesKey = profileKey("md", slot);
    String autoModeKey = profileKey("am", slot);
    String targetEcKey = profileKey("ec", slot);
    String lightOnHourKey = profileKey("loh", slot);
    String lightOnMinuteKey = profileKey("lom", slot);
    String lightOffHourKey = profileKey("lfh", slot);
    String lightOffMinuteKey = profileKey("lfm", slot);

    profile.name = profilePreferences.getString(
        nameKey.c_str(),
        "Perfil"
    );

    profile.targetPh = profilePreferences.getFloat(
        targetPhKey.c_str(),
        5.80f
    );

    profile.phTolerance = profilePreferences.getFloat(
        toleranceKey.c_str(),
        0.10f
    );

    profile.doseDurationMs = profilePreferences.getUInt(
        durationKey.c_str(),
        500
    );

    profile.doseIntervalMinutes = profilePreferences.getUInt(
        intervalKey.c_str(),
        4
    );

    profile.maxConsecutiveDoses = profilePreferences.getUChar(
        maxDosesKey.c_str(),
        3
    );

    profile.automaticMode = profilePreferences.getBool(
        autoModeKey.c_str(),
        true
    );

    profile.targetEc = profilePreferences.getFloat(
        targetEcKey.c_str(),
        1.40f
    );

    profile.lightOnHour = profilePreferences.getUChar(
        lightOnHourKey.c_str(),
        6
    );

    profile.lightOnMinute = profilePreferences.getUChar(
        lightOnMinuteKey.c_str(),
        0
    );

    profile.lightOffHour = profilePreferences.getUChar(
        lightOffHourKey.c_str(),
        18
    );

    profile.lightOffMinute = profilePreferences.getUChar(
        lightOffMinuteKey.c_str(),
        0
    );

    profilePreferences.end();

    return true;
}

void saveProfile(
    uint8_t slot,
    const CultivationProfile& profile
)
{
    profilePreferences.begin("profiles", false);

    String usedKey = profileKey("u", slot);
    String nameKey = profileKey("n", slot);
    String targetPhKey = profileKey("ph", slot);
    String toleranceKey = profileKey("pt", slot);
    String durationKey = profileKey("dm", slot);
    String intervalKey = profileKey("di", slot);
    String maxDosesKey = profileKey("md", slot);
    String autoModeKey = profileKey("am", slot);
    String targetEcKey = profileKey("ec", slot);
    String lightOnHourKey = profileKey("loh", slot);
    String lightOnMinuteKey = profileKey("lom", slot);
    String lightOffHourKey = profileKey("lfh", slot);
    String lightOffMinuteKey = profileKey("lfm", slot);

    profilePreferences.putBool(
        usedKey.c_str(),
        true
    );

    profilePreferences.putString(
        nameKey.c_str(),
        profile.name
    );

    profilePreferences.putFloat(
        targetPhKey.c_str(),
        profile.targetPh
    );

    profilePreferences.putFloat(
        toleranceKey.c_str(),
        profile.phTolerance
    );

    profilePreferences.putUInt(
        durationKey.c_str(),
        profile.doseDurationMs
    );

    profilePreferences.putUInt(
        intervalKey.c_str(),
        profile.doseIntervalMinutes
    );

    profilePreferences.putUChar(
        maxDosesKey.c_str(),
        profile.maxConsecutiveDoses
    );

    profilePreferences.putBool(
        autoModeKey.c_str(),
        profile.automaticMode
    );

    profilePreferences.putFloat(
        targetEcKey.c_str(),
        profile.targetEc
    );

    profilePreferences.putUChar(
        lightOnHourKey.c_str(),
        profile.lightOnHour
    );

    profilePreferences.putUChar(
        lightOnMinuteKey.c_str(),
        profile.lightOnMinute
    );

    profilePreferences.putUChar(
        lightOffHourKey.c_str(),
        profile.lightOffHour
    );

    profilePreferences.putUChar(
        lightOffMinuteKey.c_str(),
        profile.lightOffMinute
    );

    profilePreferences.end();
}

void deleteProfile(uint8_t slot)
{
    profilePreferences.begin("profiles", false);

    const char* prefixes[] = {
        "u", "n", "ph", "pt", "dm", "di",
        "md", "am", "ec", "loh", "lom",
        "lfh", "lfm"
    };

    for (const char* prefix : prefixes)
    {
        String key = profileKey(prefix, slot);
        profilePreferences.remove(key.c_str());
    }

    profilePreferences.end();

    if (getActiveProfileSlot() == slot)
    {
        setActiveProfileSlot(-1);
    }
}

int8_t findFreeProfileSlot()
{
    for (
        uint8_t slot = 0;
        slot < MAX_CULTIVATION_PROFILES;
        slot++
    )
    {
        CultivationProfile profile;

        if (!loadProfile(slot, profile))
        {
            return slot;
        }
    }

    return -1;
}

void applyProfileToConfig(
    const CultivationProfile& profile
)
{
    config.targetPh = profile.targetPh;
    config.phTolerance = profile.phTolerance;
    config.doseDurationMs = profile.doseDurationMs;
    config.doseIntervalMinutes = profile.doseIntervalMinutes;
    config.maxConsecutiveDoses = profile.maxConsecutiveDoses;
    config.automaticMode = profile.automaticMode;

    config.targetEc = profile.targetEc;
    config.lightOnHour = profile.lightOnHour;
    config.lightOnMinute = profile.lightOnMinute;
    config.lightOffHour = profile.lightOffHour;
    config.lightOffMinute = profile.lightOffMinute;

    saveConfig();
}

void appendProfileJson(
    String& json,
    uint8_t slot,
    const CultivationProfile& profile,
    int8_t activeSlot
)
{
    json += "{";
    json += "\"id\":" + String(slot) + ",";
    json += "\"name\":\"";
    json += escapeJson(profile.name);
    json += "\",";
    json += "\"active\":";
    json += slot == activeSlot ? "true" : "false";
    json += ",\"targetPh\":" + String(profile.targetPh, 2);
    json += ",\"tolerance\":" + String(profile.phTolerance, 2);
    json += ",\"doseSeconds\":" +
        String(profile.doseDurationMs / 1000.0f, 2);
    json += ",\"intervalMinutes\":" +
        String(profile.doseIntervalMinutes);
    json += ",\"maxDoses\":" +
        String(profile.maxConsecutiveDoses);
    json += ",\"automaticMode\":";
    json += profile.automaticMode ? "true" : "false";
    json += ",\"targetEc\":" + String(profile.targetEc, 2);
    json += ",\"lightOn\":\"";
    json += formatTime(
        profile.lightOnHour,
        profile.lightOnMinute
    );
    json += "\"";
    json += ",\"lightOff\":\"";
    json += formatTime(
        profile.lightOffHour,
        profile.lightOffMinute
    );
    json += "\"";
    json += "}";
}

void handleGetProfiles()
{
    int8_t activeSlot = getActiveProfileSlot();

    String json;
    json.reserve(6500);

    json += "{";
    json += "\"success\":true,";
    json += "\"activeId\":" +
        String(static_cast<int>(activeSlot)) + ",";
    json += "\"maxProfiles\":" +
        String(MAX_CULTIVATION_PROFILES) + ",";
    json += "\"profiles\":[";

    bool firstProfile = true;

    for (
        uint8_t slot = 0;
        slot < MAX_CULTIVATION_PROFILES;
        slot++
    )
    {
        CultivationProfile profile;

        if (!loadProfile(slot, profile))
        {
            continue;
        }

        if (!firstProfile)
        {
            json += ",";
        }

        appendProfileJson(
            json,
            slot,
            profile,
            activeSlot
        );

        firstProfile = false;
    }

    json += "]}";

    server.sendHeader(
        "Cache-Control",
        "no-store"
    );

    server.send(
        200,
        "application/json; charset=utf-8",
        json
    );
}

bool readProfileFromRequest(
    CultivationProfile& profile,
    String& errorMessage
)
{
    const char* requiredArguments[] = {
        "name",
        "targetPh",
        "tolerance",
        "doseSeconds",
        "intervalMinutes",
        "maxDoses",
        "automaticMode",
        "targetEc",
        "lightOn",
        "lightOff"
    };

    for (const char* argument : requiredArguments)
    {
        if (!server.hasArg(argument))
        {
            errorMessage =
                "Faltan datos para guardar el perfil.";

            return false;
        }
    }

    profile.name = server.arg("name");
    profile.name.trim();

    if (
        profile.name.isEmpty() ||
        profile.name.length() > 32
    )
    {
        errorMessage =
            "El nombre debe tener entre 1 y 32 caracteres.";

        return false;
    }

    profile.targetPh =
        server.arg("targetPh").toFloat();

    profile.phTolerance =
        server.arg("tolerance").toFloat();

    float doseSeconds =
        server.arg("doseSeconds").toFloat();

    int intervalMinutes =
        server.arg("intervalMinutes").toInt();

    int maxDoses =
        server.arg("maxDoses").toInt();

    profile.automaticMode =
        server.arg("automaticMode") == "true";

    profile.targetEc =
        server.arg("targetEc").toFloat();

    String lightOn = server.arg("lightOn");
    String lightOff = server.arg("lightOff");

    if (
        profile.targetPh < 4.0f ||
        profile.targetPh > 8.0f ||
        profile.phTolerance < 0.01f ||
        profile.phTolerance > 1.0f ||
        doseSeconds < 0.1f ||
        doseSeconds > 30.0f ||
        intervalMinutes < 1 ||
        intervalMinutes > 120 ||
        maxDoses < 1 ||
        maxDoses > 10 ||
        profile.targetEc < 0.10f ||
        profile.targetEc > 5.00f
    )
    {
        errorMessage =
            "Hay valores fuera del rango permitido.";

        return false;
    }

    if (
        !parseTimeValue(
            lightOn,
            profile.lightOnHour,
            profile.lightOnMinute
        ) ||
        !parseTimeValue(
            lightOff,
            profile.lightOffHour,
            profile.lightOffMinute
        )
    )
    {
        errorMessage =
            "Los horarios de luz no son válidos.";

        return false;
    }

    profile.doseDurationMs =
        static_cast<uint32_t>(doseSeconds * 1000.0f);

    profile.doseIntervalMinutes =
        static_cast<uint32_t>(intervalMinutes);

    profile.maxConsecutiveDoses =
        static_cast<uint8_t>(maxDoses);

    profile.used = true;

    return true;
}

void handleSaveProfile()
{
    CultivationProfile profile;
    String errorMessage;

    if (!readProfileFromRequest(profile, errorMessage))
    {
        String json = "{";
        json += "\"success\":false,";
        json += "\"message\":\"";
        json += escapeJson(errorMessage);
        json += "\"}";

        server.send(
            400,
            "application/json; charset=utf-8",
            json
        );

        return;
    }

    int requestedSlot = -1;

    if (server.hasArg("id"))
    {
        requestedSlot = server.arg("id").toInt();
    }

    if (
        server.hasArg("id") &&
        requestedSlot != -1 &&
        (
            requestedSlot < 0 ||
            requestedSlot >= MAX_CULTIVATION_PROFILES
        )
    )
    {
        server.send(
            400,
            "application/json; charset=utf-8",
            "{\"success\":false,"
            "\"message\":\"El identificador del perfil no es válido.\"}"
        );

        return;
    }

    bool updating =
        requestedSlot >= 0 &&
        requestedSlot < MAX_CULTIVATION_PROFILES;

    int8_t slot = updating
        ? static_cast<int8_t>(requestedSlot)
        : findFreeProfileSlot();

    if (slot < 0)
    {
        server.send(
            409,
            "application/json; charset=utf-8",
            "{\"success\":false,"
            "\"message\":\"Ya alcanzaste el máximo de 10 perfiles.\"}"
        );

        return;
    }

    if (updating)
    {
        CultivationProfile existingProfile;

        if (!loadProfile(slot, existingProfile))
        {
            server.send(
                404,
                "application/json; charset=utf-8",
                "{\"success\":false,"
                "\"message\":\"El perfil que querés editar no existe.\"}"
            );

            return;
        }
    }

    saveProfile(slot, profile);

    // Si se edita el perfil activo, los nuevos valores pasan
    // a ser la configuración activa inmediatamente.
    if (getActiveProfileSlot() == slot)
    {
        applyProfileToConfig(profile);
    }

    String profileEventDetail = "pH ";
    profileEventDetail += String(profile.targetPh, 2);
    profileEventDetail += " · EC ";
    profileEventDetail += String(profile.targetEc, 2);
    profileEventDetail += " mS/cm · Luz ";
    profileEventDetail += formatTime(
        profile.lightOnHour,
        profile.lightOnMinute
    );
    profileEventDetail += "–";
    profileEventDetail += formatTime(
        profile.lightOffHour,
        profile.lightOffMinute
    );

    logEvent(
        "profile",
        updating
            ? "Perfil actualizado"
            : "Perfil creado",
        profile.name + " · " + profileEventDetail
    );

    String json = "{";
    json += "\"success\":true,";
    json += "\"id\":" + String(slot) + ",";
    json += "\"message\":\"";
    json += updating
        ? "Perfil actualizado correctamente."
        : "Perfil guardado correctamente.";
    json += "\"}";

    server.send(
        200,
        "application/json; charset=utf-8",
        json
    );
}

void handleApplyProfile()
{
    if (!server.hasArg("id"))
    {
        server.send(
            400,
            "application/json; charset=utf-8",
            "{\"success\":false,"
            "\"message\":\"Falta seleccionar un perfil.\"}"
        );

        return;
    }

    int slot = server.arg("id").toInt();

    if (
        slot < 0 ||
        slot >= MAX_CULTIVATION_PROFILES
    )
    {
        server.send(
            400,
            "application/json; charset=utf-8",
            "{\"success\":false,"
            "\"message\":\"El perfil seleccionado no es válido.\"}"
        );

        return;
    }

    CultivationProfile profile;

    if (!loadProfile(slot, profile))
    {
        server.send(
            404,
            "application/json; charset=utf-8",
            "{\"success\":false,"
            "\"message\":\"El perfil seleccionado no existe.\"}"
        );

        return;
    }

    applyProfileToConfig(profile);
    setActiveProfileSlot(static_cast<int8_t>(slot));

    String appliedDetail = "pH ";
    appliedDetail += String(profile.targetPh, 2);
    appliedDetail += " · EC ";
    appliedDetail += String(profile.targetEc, 2);
    appliedDetail += " mS/cm · Luz ";
    appliedDetail += formatTime(
        profile.lightOnHour,
        profile.lightOnMinute
    );
    appliedDetail += "–";
    appliedDetail += formatTime(
        profile.lightOffHour,
        profile.lightOffMinute
    );

    logEvent(
        "profile",
        "Perfil aplicado",
        profile.name + " · " + appliedDetail
    );

    String json = "{";
    json += "\"success\":true,";
    json += "\"activeId\":" + String(slot) + ",";
    json += "\"message\":\"Perfil aplicado: ";
    json += escapeJson(profile.name);
    json += ".\"}";

    server.send(
        200,
        "application/json; charset=utf-8",
        json
    );
}

void handleDeleteProfile()
{
    if (!server.hasArg("id"))
    {
        server.send(
            400,
            "application/json; charset=utf-8",
            "{\"success\":false,"
            "\"message\":\"Falta seleccionar un perfil.\"}"
        );

        return;
    }

    int slot = server.arg("id").toInt();

    if (
        slot < 0 ||
        slot >= MAX_CULTIVATION_PROFILES
    )
    {
        server.send(
            400,
            "application/json; charset=utf-8",
            "{\"success\":false,"
            "\"message\":\"El perfil seleccionado no es válido.\"}"
        );

        return;
    }

    CultivationProfile profile;

    if (!loadProfile(slot, profile))
    {
        server.send(
            404,
            "application/json; charset=utf-8",
            "{\"success\":false,"
            "\"message\":\"El perfil seleccionado no existe.\"}"
        );

        return;
    }

    String deletedName = profile.name;
    deleteProfile(static_cast<uint8_t>(slot));

    logEvent(
        "profile",
        "Perfil eliminado",
        deletedName
    );

    String json = "{";
    json += "\"success\":true,";
    json += "\"message\":\"Perfil eliminado: ";
    json += escapeJson(deletedName);
    json += ".\"}";

    server.send(
        200,
        "application/json; charset=utf-8",
        json
    );
}

// ======================================================
// API DE ESTADO GENERAL
// ======================================================
