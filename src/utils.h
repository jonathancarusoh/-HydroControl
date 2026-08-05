#pragma once

#include <Arduino.h>

String escapeJson(const String& text);
String getResetReasonText();
String getSystemModeCode();
String getSystemModeLabel();
