#include "api.h"
#include "app_state.h"
#include "clock_manager.h"
#include "profile_manager.h"
#include "ph_sensor.h"
#include "utils.h"
#include <WiFi.h>
#include <math.h>

// ======================================================
// API GENERAL
// ======================================================

void handleApiStatus()
{
    String json = "{";

    json += "\"online\":true,";
    json += "\"ph\":";
    json += isfinite(currentPh)
        ? String(currentPh, 2)
        : "null";
    json += ",";
    json += "\"ec\":" + String(currentEc, 2) + ",";
    json += "\"waterTemp\":" +
        String(waterTemperature, 1) + ",";

    json += "\"humidity\":" +
        String(airHumidity, 0) + ",";

    json += "\"phSensor\":{";
    json += "\"pin\":" + String(PH_SENSOR_PIN) + ",";
    json += "\"calibrated\":";
    json += isPhSensorCalibrated() ? "true" : "false";
    json += ",\"saturated\":";
    json += isPhSensorSaturated() ? "true" : "false";
    json += ",\"stable\":";
    json += isPhSensorStable() ? "true" : "false";
    json += ",\"raw\":" + String(static_cast<unsigned int>(getPhSensorRaw())) + ",";
    json += "\"millivolts\":" + String(getPhSensorMillivolts()) + ",";
    json += "\"batchSpreadMv\":" + String(getPhSensorBatchSpreadMillivolts()) + ",";
    json += "\"historySpreadMv\":" + String(getPhSensorHistorySpreadMillivolts());
    json += "},";

    json += "\"wifiRssi\":" +
        String(WiFi.RSSI());

    json += ",";
    appendClockAndLightJson(json);

    int8_t activeProfileSlot = getActiveProfileSlot();
    CultivationProfile activeProfile;
    bool hasActiveProfile =
        activeProfileSlot >= 0 &&
        loadProfile(
            static_cast<uint8_t>(activeProfileSlot),
            activeProfile
        );

    json += ",\"activeProfile\":{";
    json += "\"active\":";
    json += hasActiveProfile ? "true" : "false";
    json += ",\"id\":";
    json += hasActiveProfile
        ? String(static_cast<int>(activeProfileSlot))
        : "-1";
    json += ",\"name\":\"";
    json += hasActiveProfile
        ? escapeJson(activeProfile.name)
        : "";
    json += "\"}";

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

// ======================================================
// API DE CONFIGURACIÓN DE PH
// ======================================================

void handleGetRuntimeInfo()
{
    String json = "{";
    json += "\"success\":true,";
    json += "\"firmwareVersion\":\"";
    json += FIRMWARE_VERSION;
    json += "\",";
    json += "\"clockApi\":true,";
    json += "\"eventsApi\":true,";
    json += "\"lightScheduleApi\":true,";
    json += "\"lightManualApi\":true,";
    json += "\"lightPhysicalOutput\":true,";
    json += "\"phSensorApi\":true,";
    json += "\"physicalPhPumps\":true,";
    json += "\"automaticPhDosing\":true";
    json += "}";

    server.sendHeader("Cache-Control", "no-store");
    server.send(
        200,
        "application/json; charset=utf-8",
        json
    );
}

// ======================================================
// REGISTRO DE RUTAS
// ======================================================
