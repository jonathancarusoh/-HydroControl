#include "light_output.h"

#include "clock_manager.h"
#include "event_logger.h"

namespace
{
    bool lightOutputInitialized = false;
    bool lightOutputOn = false;

    uint8_t relayLevelForState(bool on)
    {
        bool highLevel = LIGHT_RELAY_ACTIVE_HIGH
            ? on
            : !on;

        return highLevel ? HIGH : LOW;
    }

    void writeLightOutput(bool on)
    {
        digitalWrite(
            LIGHT_RELAY_PIN,
            relayLevelForState(on)
        );

        lightOutputOn = on;
    }

    String buildLightOutputDetail(bool on)
    {
        String detail = "GPIO ";
        detail += String(LIGHT_RELAY_PIN);
        detail += " · relé ";
        detail += LIGHT_RELAY_ACTIVE_HIGH
            ? "activo en HIGH"
            : "activo en LOW";
        detail += " · modo ";
        detail += getLightControlMode();
        detail += " · salida ";
        detail += on ? "ON" : "OFF";

        return detail;
    }
}

void initializeLightOutput()
{
    // Cargamos primero el nivel inactivo en el latch del GPIO y después
    // lo convertimos en salida. Con el jumper H, LOW mantiene el relé apagado.
    digitalWrite(
        LIGHT_RELAY_PIN,
        relayLevelForState(false)
    );

    pinMode(LIGHT_RELAY_PIN, OUTPUT);

    writeLightOutput(false);
    lightOutputInitialized = true;

    Serial.printf(
        "Salida de luz lista: GPIO %u · relé activo en %s · estado inicial OFF\n",
        LIGHT_RELAY_PIN,
        LIGHT_RELAY_ACTIVE_HIGH ? "HIGH" : "LOW"
    );
}

void processLightOutput()
{
    if (!lightOutputInitialized)
    {
        return;
    }

    bool requestedOn = isLightEffectivelyOn();

    if (requestedOn == lightOutputOn)
    {
        return;
    }

    writeLightOutput(requestedOn);

    logEvent(
        "light",
        requestedOn
            ? "Salida física de luz encendida"
            : "Salida física de luz apagada",
        buildLightOutputDetail(requestedOn)
    );

    Serial.printf(
        "[LUZ] GPIO %u -> %s\n",
        LIGHT_RELAY_PIN,
        requestedOn ? "ON" : "OFF"
    );
}

bool isLightOutputInitialized()
{
    return lightOutputInitialized;
}

bool isLightOutputOn()
{
    return lightOutputInitialized && lightOutputOn;
}

uint8_t getLightRelayPin()
{
    return LIGHT_RELAY_PIN;
}

bool isLightRelayActiveHigh()
{
    return LIGHT_RELAY_ACTIVE_HIGH;
}
