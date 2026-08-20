#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>

#include "app_state.h"
#include "clock_manager.h"
#include "config.h"
#include "event_logger.h"
#include "manual_ph_dosing.h"
#include "light_output.h"
#include "profile_manager.h"
#include "utils.h"
#include "web_server.h"
#include "wifi_manager.h"

namespace
{
void printStartupBanner()
{
    Serial.println();
    Serial.println("=================================");
    Serial.println("       HYDRO CONTROL PRO");
    Serial.println("=================================");
}

void printLoadedConfiguration()
{
    Serial.println("Configuración cargada:");
    Serial.printf("Objetivo pH: %.2f\n", config.targetPh);
    Serial.printf("Tolerancia: %.2f\n", config.phTolerance);
    Serial.printf(
        "Duración dosis: %lu ms\n",
        static_cast<unsigned long>(config.doseDurationMs)
    );
    Serial.printf(
        "Intervalo: %lu minutos\n",
        static_cast<unsigned long>(config.doseIntervalMinutes)
    );
    Serial.printf("Máximo automático por 24 h: %u\n", config.maxDailyDoses);
}

void printAccessInformation()
{
    Serial.println("Servidor web iniciado.");

    if (wifiSetupMode)
    {
        Serial.println("Conectate a HydroControl-Setup");
        Serial.println("Abrí: http://192.168.4.1");
        return;
    }

    if (localAccessMode)
    {
        Serial.println("Conectate a la red HydroControl");
        Serial.println("Contraseña: hydrocontrol");
        Serial.println("Abrí: http://192.168.4.1");
        return;
    }

    startMdns();
    Serial.print("IP actual: http://");
    Serial.println(WiFi.localIP());
    Serial.println("Nombre permanente: http://hydrocontrol.local");
}
}

void setup()
{
    Serial.begin(115200);
    delay(500);

    printStartupBanner();

    loadConfig();
    clearActiveProfileIfConfigChanged();
    printLoadedConfiguration();

    if (!LittleFS.begin())
    {
        Serial.println("Error al iniciar LittleFS.");

        while (true)
        {
            delay(1000);
        }
    }

    Serial.println("LittleFS iniciado correctamente.");
    littleFsReady = true;

    initializeClockRuntime();
    initializeLightOutput();
    processLightOutput();

    String startupDetail = "Firmware ";
    startupDetail += FIRMWARE_VERSION;
    startupDetail += " · ";
    startupDetail += getResetReasonText();

    logEvent("system", "ESP32 iniciado", startupDetail);

    registerServerRoutes();

    if (loadLocalAccessMode())
    {
        startLocalAccessMode();
    }
    else if (!connectToSavedWifi())
    {
        startWifiSetupMode();
    }

    server.begin();
    printAccessInformation();
}

void loop()
{
    if (wifiSetupMode || localAccessMode)
    {
        dnsServer.processNextRequest();
    }

    server.handleClient();
    processManualPhDosing();
    processLightOutput();
    monitorRouterWifiState();

    if (
        restartPending &&
        millis() - restartRequestedAt >= RESTART_DELAY_MS
    )
    {
        preserveClockForSoftwareRestart();
        ESP.restart();
    }

    delay(2);
}
