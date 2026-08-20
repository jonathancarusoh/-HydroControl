#pragma once

#include <Arduino.h>

// Salida física de iluminación.
// El módulo de relé está conectado al GPIO 23 y el jumper se usa en H:
// HIGH = relé activado, LOW = relé desactivado.
constexpr uint8_t LIGHT_RELAY_PIN = 23;
constexpr bool LIGHT_RELAY_ACTIVE_HIGH = true;

void initializeLightOutput();
void processLightOutput();

bool isLightOutputInitialized();
bool isLightOutputOn();
uint8_t getLightRelayPin();
bool isLightRelayActiveHigh();
