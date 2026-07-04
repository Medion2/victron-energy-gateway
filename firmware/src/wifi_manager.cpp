#include "wifi_manager.h"
#include "logger.h"

#if __has_include("config.h")
  #include "config.h"
#else
  #include "config.example.h"
#endif

static unsigned long lastReconnectTry = 0;
static unsigned long lastStatusLog = 0;

void wifiSetup() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  logInfo("WLAN Verbindung wird gestartet");
  logInfo(String("SSID: ") + VEG_WIFI_SSID);

  WiFi.begin(VEG_WIFI_SSID, VEG_WIFI_PASSWORD);
}

void wifiLoop() {
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastReconnectTry > 10000) {
      lastReconnectTry = millis();
      logWarn("WLAN nicht verbunden, neuer Verbindungsversuch");
      WiFi.disconnect();
      WiFi.begin(VEG_WIFI_SSID, VEG_WIFI_PASSWORD);
    }
    return;
  }

  if (millis() - lastStatusLog > 60000) {
    lastStatusLog = millis();
    logInfo(String("WLAN OK, IP: ") + WiFi.localIP().toString() + ", RSSI: " + String(WiFi.RSSI()) + " dBm");
  }
}

bool wifiIsConnected() {
  return WiFi.status() == WL_CONNECTED;
}

String wifiIpAddress() {
  if (!wifiIsConnected()) return "0.0.0.0";
  return WiFi.localIP().toString();
}

String wifiStatusJson() {
  String json = "{";
  json += "\"connected\":" + String(wifiIsConnected() ? "true" : "false") + ",";
  json += "\"ssid\":\"" + String(WiFi.SSID()) + "\",";
  json += "\"ip\":\"" + wifiIpAddress() + "\",";
  json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"mac\":\"" + WiFi.macAddress() + "\"";
  json += "}";
  return json;
}
