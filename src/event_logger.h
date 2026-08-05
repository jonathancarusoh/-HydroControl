#pragma once

#include <Arduino.h>

void logEvent(
    const String& category,
    const String& title,
    const String& detail = ""
);

void recordDosageEvent(
    const String& product,
    float durationSeconds,
    const String& reason
);

size_t getEventLogTotalBytes();
void handleGetEvents();
void handleClearEvents();
