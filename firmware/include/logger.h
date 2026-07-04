#pragma once
#include <Arduino.h>

void logInfo(const String &message);
void logWarn(const String &message);
void logError(const String &message);
String getLogBuffer();
