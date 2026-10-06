#pragma once

#include <Arduino.h>

// Salida física de iluminación.
// El relé estable quedó en GPIO25. El GPIO23 se evita por el fallo
// observado en hardware. Jumper H: HIGH = ON, LOW = OFF.
constexpr uint8_t LIGHT_RELAY_PIN = 25;
constexpr bool LIGHT_RELAY_ACTIVE_HIGH = true;

void initializeLightOutput();
void processLightOutput();

bool isLightOutputInitialized();
bool isLightOutputOn();
uint8_t getLightRelayPin();
bool isLightRelayActiveHigh();
