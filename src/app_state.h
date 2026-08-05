#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>

struct HydroConfig
{
    float targetPh = 5.80f;
    float phTolerance = 0.10f;
    uint32_t doseDurationMs = 500;
    uint32_t doseIntervalMinutes = 4;
    uint8_t maxConsecutiveDoses = 3;
    bool automaticMode = true;

    float targetEc = 1.40f;
    uint8_t lightOnHour = 6;
    uint8_t lightOnMinute = 0;
    uint8_t lightOffHour = 18;
    uint8_t lightOffMinute = 0;
    bool lightScheduleEnabled = false;
};

extern WebServer server;
extern DNSServer dnsServer;
extern HydroConfig config;

extern const char* SETUP_WIFI_NAME;
extern const char* SETUP_WIFI_PASSWORD;
extern const char* LOCAL_WIFI_NAME;
extern const char* LOCAL_WIFI_PASSWORD;
extern const char* MDNS_HOSTNAME;
extern const char* FIRMWARE_VERSION;
extern const byte DNS_PORT;
extern const unsigned long RESTART_DELAY_MS;

extern bool wifiSetupMode;
extern bool localAccessMode;
extern bool restartPending;
extern unsigned long restartRequestedAt;
extern bool littleFsReady;
extern bool clockConfigured;
extern bool clockRestoredAfterSoftwareRestart;

// Lecturas provisionales hasta conectar los sensores reales.
extern float currentPh;
extern float currentEc;
extern float waterTemperature;
extern float airHumidity;
