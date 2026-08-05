#include "api.h"
#include "app_state.h"
#include "clock_manager.h"
#include "utils.h"
#include <WiFi.h>

// ======================================================
// API GENERAL
// ======================================================

void handleApiStatus()
{
    String json = "{";

    json += "\"online\":true,";
    json += "\"ph\":" + String(currentPh, 2) + ",";
    json += "\"ec\":" + String(currentEc, 2) + ",";
    json += "\"waterTemp\":" +
        String(waterTemperature, 1) + ",";

    json += "\"humidity\":" +
        String(airHumidity, 0) + ",";

    json += "\"wifiRssi\":" +
        String(WiFi.RSSI());

    json += ",";
    appendClockAndLightJson(json);

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
    json += "\"lightScheduleApi\":true";
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
