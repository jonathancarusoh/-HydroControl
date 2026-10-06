#pragma once

#include <Arduino.h>

// PH-4502C: salida analógica PO conectada al ADC1 del ESP32.
// GPIO34 es solo entrada y puede usarse con WiFi activo.
constexpr uint8_t PH_SENSOR_PIN = 34;

void initializePhSensor();
void processPhSensor();
void forcePhSensorSample();

bool isPhSensorInitialized();
bool isPhSensorCalibrated();
bool isPhSensorSaturated();

uint16_t getPhSensorRaw();
uint32_t getPhSensorMillivolts();
float getPhSensorVoltage();
float getPhCalibration7Millivolts();
float getPhCalibration4Millivolts();

void handleGetPhSensorStatus();
void handleCapturePhCalibration();
void handleResetPhCalibration();
