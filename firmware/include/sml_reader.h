#pragma once
#include <Arduino.h>

void smlSetup();
void smlLoop();
String smlStatusJson();
String smlLastTelegramHex();
bool smlHasData();
