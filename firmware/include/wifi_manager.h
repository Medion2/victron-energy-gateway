#pragma once
#include <Arduino.h>
#include <WiFi.h>

void wifiSetup();
void wifiLoop();
bool wifiIsConnected();
String wifiIpAddress();
String wifiStatusJson();
