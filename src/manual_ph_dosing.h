#pragma once

#include <Arduino.h>

enum class ManualPhDirection : int8_t
{
    None = 0,
    Minus = -1,
    Plus = 1
};

void processManualPhDosing();
bool isManualPhDosingActive();

void handleGetManualPhDoseStatus();
void handleStartManualPhDose();
void handleCancelManualPhDose();
