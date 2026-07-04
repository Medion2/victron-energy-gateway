#include <Arduino.h>
#include "version.h"
#include "logger.h"
#include "wifi_manager.h"
#include "webserver.h"
#include "sml_reader.h"

static unsigned long lastHeartbeat = 0;

static void printBootHeader() {
  Serial.println();
  Serial.println("========================================");
  Serial.println(VEG_NAME);
  Serial.print("Version   : ");
  Serial.println(VEG_VERSION);
  Serial.print("Codename  : ");
  Serial.println(VEG_CODENAME);
  Serial.println("Hardware  : ESP32-S3 DevKitC-1 N16R8");
  Serial.println("========================================");
  Serial.println();
}

static void printSystemInfo() {
  logInfo(String("Chip Model: ") + ESP.getChipModel());
  logInfo(String("CPU MHz: ") + String(ESP.getCpuFreqMHz()));
  logInfo(String("Flash Size: ") + String(ESP.getFlashChipSize() / 1024 / 1024) + " MB");
  logInfo(String("Heap Free: ") + String(ESP.getFreeHeap()) + " bytes");

  #if defined(BOARD_HAS_PSRAM)
    logInfo(String("PSRAM Free: ") + String(ESP.getFreePsram()) + " bytes");
  #endif
}

void setup() {
  Serial.begin(115200);
  delay(1200);

  printBootHeader();
  logInfo("Firmware startet");
  printSystemInfo();

  wifiSetup();
  webserverSetup();
  smlSetup();
}

void loop() {
  wifiLoop();
  webserverLoop();
  smlLoop();

  if (millis() - lastHeartbeat > 30000) {
    lastHeartbeat = millis();
    logInfo(String("Heartbeat, Uptime: ") + String(millis() / 1000) + " s, IP: " + wifiIpAddress());
  }

  delay(10);
}
