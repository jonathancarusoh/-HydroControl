#pragma once

#include <Arduino.h>

// Señales de control de los MOSFET de las bombas de corrección de pH.
constexpr uint8_t PH_MINUS_PUMP_PIN = 26;
constexpr uint8_t PH_PLUS_PUMP_PIN = 27;

// Los módulos usados en HydroControl se disparan con nivel HIGH en TRIG.
// Si se reemplazan por módulos activos en LOW, cambiar este valor a false.
constexpr bool PH_PUMPS_ACTIVE_HIGH = true;

void initializePumpOutputs();
void setPhPumpOutputs(bool minusOn, bool plusOn);
void stopPhPumpOutputs();

bool isPhMinusPumpOn();
bool isPhPlusPumpOn();
