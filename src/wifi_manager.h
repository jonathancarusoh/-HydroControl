#pragma once

#include <Arduino.h>

String loadWifiSsid();
String loadWifiPassword();
void saveWifiCredentials(const String& ssid, const String& password);
void clearWifiCredentials();
void clearWifiPassword();
bool loadLocalAccessMode();
void saveLocalAccessMode(bool enabled);
bool connectToSavedWifi();
void startLocalAccessMode();
void startWifiSetupMode();
void monitorRouterWifiState();
bool startMdns();

void handleSaveWifiForm();
void handleUseLocalMode();
void handleGetWifiStatus();
void handleScanWifi();
void handleSaveWifiApi();
void handleResetWifi();
void handleDisconnectWifi();
void handleConnectSavedWifi();
void handleDeleteSavedWifiPassword();
