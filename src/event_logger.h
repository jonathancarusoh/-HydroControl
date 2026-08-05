#pragma once

#include <Arduino.h>

void logEvent(
    const String& category,
    const String& title,
    const String& detail = ""
);

void recordDosageEvent(
    const String& channel,
    uint32_t durationMs,
    bool automatic
);

size_t getEventLogTotalBytes();
void handleGetEvents();
void handleClearEvents();
