#include "webserver.h"
#include "version.h"
#include "logger.h"
#include "wifi_manager.h"
#include "sml_reader.h"
#include <WebServer.h>

static WebServer server(80);

static String htmlPage() {
  String html;
  html += "<!doctype html><html lang='de'><head>";
  html += "<meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>Victron Energy Gateway</title>";
  html += "<style>body{font-family:Arial;background:#111;color:#eee;margin:20px}.card{background:#1f1f1f;padding:16px;border-radius:12px;margin-bottom:14px}.ok{color:#66ff99}.warn{color:#ffcc66}pre{white-space:pre-wrap;background:#000;padding:12px;border-radius:8px}</style>";
  html += "</head><body>";
  html += "<h1>Victron Energy Gateway</h1>";
  html += "<div class='card'><b>Version:</b> "; html += VEG_VERSION; html += "<br>";
  html += "<b>Codename:</b> "; html += VEG_CODENAME; html += "<br>";
  html += "<b>IP:</b> "; html += wifiIpAddress(); html += "</div>";
  html += "<div class='card'><h2>Status</h2><p>WLAN: ";
  html += wifiIsConnected() ? "<span class='ok'>verbunden</span>" : "<span class='warn'>nicht verbunden</span>";
  html += "</p><p>SML: ";
  html += smlHasData() ? "<span class='ok'>Daten empfangen</span>" : "<span class='warn'>noch keine Daten</span>";
  html += "</p><p><a href='/api/status'>JSON Status</a> | <a href='/api/sml'>SML Status</a> | <a href='/sml/raw'>SML Rohdaten</a> | <a href='/log'>Log</a></p></div>";
  html += "</body></html>";
  return html;
}

static void handleRoot() {
  server.send(200, "text/html", htmlPage());
}

static void handleStatus() {
  String json = "{";
  json += "\"name\":\"" + String(VEG_NAME) + "\",";
  json += "\"version\":\"" + String(VEG_VERSION) + "\",";
  json += "\"codename\":\"" + String(VEG_CODENAME) + "\",";
  json += "\"uptime\":" + String(millis() / 1000) + ",";
  json += "\"heap_free\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"wifi\":" + wifiStatusJson() + ",";
  json += "\"sml\":" + smlStatusJson();
  json += "}";
  server.send(200, "application/json", json);
}

static void handleSml() {
  server.send(200, "application/json", smlStatusJson());
}

static void handleSmlRaw() {
  server.send(200, "text/plain", smlLastTelegramHex());
}

static void handleLog() {
  server.send(200, "text/plain", getLogBuffer());
}

void webserverSetup() {
  server.on("/", handleRoot);
  server.on("/api/status", handleStatus);
  server.on("/api/sml", handleSml);
  server.on("/sml/raw", handleSmlRaw);
  server.on("/log", handleLog);
  server.begin();
  logInfo("Webserver gestartet auf Port 80");
}

void webserverLoop() {
  server.handleClient();
}
