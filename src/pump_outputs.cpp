#include "pump_outputs.h"

namespace
{
bool minusPumpOn = false;
bool plusPumpOn = false;

uint8_t levelFor(bool on)
{
    return (on == PH_PUMPS_ACTIVE_HIGH) ? HIGH : LOW;
}

void writeOutputs(bool minusOn, bool plusOn)
{
    digitalWrite(PH_MINUS_PUMP_PIN, levelFor(minusOn));
    digitalWrite(PH_PLUS_PUMP_PIN, levelFor(plusOn));

    minusPumpOn = minusOn;
    plusPumpOn = plusOn;
}
}

void initializePumpOutputs()
{
    // Preparamos el latch de salida antes de convertir los pines en OUTPUT.
    // Así las bombas arrancan siempre apagadas durante el boot.
    digitalWrite(PH_MINUS_PUMP_PIN, levelFor(false));
    digitalWrite(PH_PLUS_PUMP_PIN, levelFor(false));

    pinMode(PH_MINUS_PUMP_PIN, OUTPUT);
    pinMode(PH_PLUS_PUMP_PIN, OUTPUT);

    stopPhPumpOutputs();
}

void setPhPumpOutputs(bool minusOn, bool plusOn)
{
    // Interbloqueo físico por software: nunca se permiten ambas bombas juntas.
    if (minusOn && plusOn)
    {
        minusOn = false;
        plusOn = false;
    }

    // Primero apagamos ambas y recién después activamos la solicitada.
    // Evita solapamientos breves al cambiar de dirección.
    writeOutputs(false, false);

    if (minusOn)
    {
        writeOutputs(true, false);
    }
    else if (plusOn)
    {
        writeOutputs(false, true);
    }
}

void stopPhPumpOutputs()
{
    writeOutputs(false, false);
}

bool isPhMinusPumpOn()
{
    return minusPumpOn;
}

bool isPhPlusPumpOn()
{
    return plusPumpOn;
}
