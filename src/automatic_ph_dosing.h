#pragma once

#include <Arduino.h>

void initializeAutomaticPhDosing();
void processAutomaticPhDosing();

bool isAutomaticPhDosingActive();
void handleGetAutomaticPhDoseStatus();
