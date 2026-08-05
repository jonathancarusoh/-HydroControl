#pragma once

#include <Arduino.h>
#include <time.h>

String formatTime(uint8_t hour, uint8_t minute);
bool parseTimeValue(const String& value, uint8_t& hour, uint8_t& minute);
bool isValidClockEpoch(uint32_t epoch);
void initializeClockRuntime();
void preserveClockForSoftwareRestart();
bool getCurrentLocalTime(struct tm& timeInfo);
uint32_t getCurrentClockEpoch();
uint16_t getScheduleDurationMinutes();
bool isLightScheduledOnNow();
bool isLightEffectivelyOn();
String getLightControlMode();
String getLightStateCode();
String getLightStateLabel();
String getLightNextChangeLabel();
void appendClockAndLightJson(String& json);
void handleGetClockStatus();
void handleSetClock();
void handleSaveLightSchedule();
void handleSetManualLight();
