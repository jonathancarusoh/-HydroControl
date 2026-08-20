#include "app_state.h"

WebServer server(80);
DNSServer dnsServer;
HydroConfig config;

const char* SETUP_WIFI_NAME = "HydroControl-Setup";
const char* SETUP_WIFI_PASSWORD = "hydrocontrol";
const char* LOCAL_WIFI_NAME = "HydroControl";
const char* LOCAL_WIFI_PASSWORD = "hydrocontrol";
const char* MDNS_HOSTNAME = "hydrocontrol";
const char* FIRMWARE_VERSION = "0.4.0";
const byte DNS_PORT = 53;
const unsigned long RESTART_DELAY_MS = 3000;

bool wifiSetupMode = false;
bool localAccessMode = false;
bool restartPending = false;
unsigned long restartRequestedAt = 0;
bool littleFsReady = false;
bool clockConfigured = false;
bool clockRestoredAfterSoftwareRestart = false;

float currentPh = 5.82f;
float currentEc = 1.45f;
float waterTemperature = 18.5f;
float airHumidity = 61.0f;
