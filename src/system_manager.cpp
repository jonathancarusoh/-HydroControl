#include "system_manager.h"
#include "app_state.h"
#include "event_logger.h"
#include "utils.h"
#include "wifi_manager.h"
#include <WiFi.h>
#include <LittleFS.h>
#include <esp_timer.h>
#include <esp_system.h>

// ======================================================
// SISTEMA Y DIAGNÓSTICO
// ======================================================

void handleGetSystemStatus()
{
    const bool routerConnected =
        !wifiSetupMode &&
        !localAccessMode &&
        WiFi.status() == WL_CONNECTED;

    String currentSsid;
    String currentIp;
    String wifiState;

    if (wifiSetupMode)
    {
        currentSsid = SETUP_WIFI_NAME;
        currentIp = WiFi.softAPIP().toString();
        wifiState = "Punto de acceso de configuración activo";
    }
    else if (localAccessMode)
    {
        currentSsid = LOCAL_WIFI_NAME;
        currentIp = WiFi.softAPIP().toString();
        wifiState = "Punto de acceso local activo";
    }
    else if (routerConnected)
    {
        currentSsid = WiFi.SSID();
        currentIp = WiFi.localIP().toString();
        wifiState = "Conectado";
    }
    else
    {
        currentSsid = loadWifiSsid();
        currentIp = "";
        wifiState = "Desconectado";
    }

    const size_t littleFsTotal = LittleFS.totalBytes();
    const size_t littleFsUsed = LittleFS.usedBytes();
    const size_t littleFsAvailable =
        littleFsTotal >= littleFsUsed
            ? littleFsTotal - littleFsUsed
            : 0;

    const uint64_t uptimeSeconds =
        static_cast<uint64_t>(esp_timer_get_time()) /
        1000000ULL;

    char uptimeBuffer[24];

    snprintf(
        uptimeBuffer,
        sizeof(uptimeBuffer),
        "%llu",
        static_cast<unsigned long long>(uptimeSeconds)
    );

    String json;
    json.reserve(950);

    json += "{";

    json += "\"uptimeSeconds\":";
    json += uptimeBuffer;

    json += ",\"memory\":{";
    json += "\"freeHeap\":" +
        String(static_cast<unsigned long>(ESP.getFreeHeap()));
    json += ",\"minimumFreeHeap\":" +
        String(static_cast<unsigned long>(ESP.getMinFreeHeap()));
    json += "}";

    json += ",\"flash\":{";
    json += "\"total\":" +
        String(static_cast<unsigned long>(ESP.getFlashChipSize()));
    json += "}";

    json += ",\"littlefs\":{";
    json += "\"total\":" +
        String(static_cast<unsigned long>(littleFsTotal));
    json += ",\"used\":" +
        String(static_cast<unsigned long>(littleFsUsed));
    json += ",\"available\":" +
        String(static_cast<unsigned long>(littleFsAvailable));
    json += "}";

    json += ",\"mode\":{";
    json += "\"code\":\"";
    json += getSystemModeCode();
    json += "\",\"label\":\"";
    json += escapeJson(getSystemModeLabel());
    json += "\"}";

    json += ",\"wifi\":{";
    json += "\"state\":\"";
    json += escapeJson(wifiState);
    json += "\",\"routerConnected\":";
    json += routerConnected ? "true" : "false";
    json += ",\"ssid\":\"";
    json += escapeJson(currentSsid);
    json += "\",\"ip\":\"";
    json += escapeJson(currentIp);
    json += "\",\"rssi\":";

    if (routerConnected)
    {
        json += String(WiFi.RSSI());
    }
    else
    {
        json += "null";
    }

    json += "}";

    json += ",\"mdns\":{";
    json += "\"url\":\"http://hydrocontrol.local\",";
    json += "\"available\":";
    json += routerConnected ? "true" : "false";
    json += "}";

    json += ",\"firmware\":{";
    json += "\"version\":\"";
    json += escapeJson(FIRMWARE_VERSION);
    json += "\",\"compiledAt\":\"";
    json += escapeJson(String(__DATE__) + " " + String(__TIME__));
    json += "\"}";

    json += ",\"reset\":{";
    json += "\"code\":" +
        String(static_cast<int>(esp_reset_reason()));
    json += ",\"reason\":\"";
    json += escapeJson(getResetReasonText());
    json += "\"}";

    json += "}";

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

void handleRestartSystem()
{
    logEvent(
        "system",
        "Reinicio solicitado",
        "El ESP32 se reiniciará sin borrar la configuración."
    );

    server.send(
        200,
        "application/json; charset=utf-8",
        "{\"success\":true,"
        "\"message\":\"HydroControl se reiniciará en unos segundos.\"}"
    );

    restartPending = true;
    restartRequestedAt = millis();
}

// ======================================================
// API DE WIFI
// ======================================================
