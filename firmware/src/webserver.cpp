#include "webserver.h"
#include "version.h"
#include "logger.h"
#include "wifi_manager.h"
#include "sml_reader.h"
#include <WebServer.h>
#include <Update.h>

static WebServer server(80);
static bool rebootAfterUpdate = false;

static String htmlPage() {
  String html;
  html += "<!doctype html><html lang='de'><head>";
  html += "<meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>Landis+Gyr E320 Reader</title>";
  html += "<style>body{font-family:Arial;background:#111;color:#eee;margin:20px}.card{background:#1f1f1f;padding:16px;border-radius:12px;margin-bottom:14px}.ok{color:#66ff99}.warn{color:#ffcc66}a{color:#7ec8ff}button,input[type=submit]{padding:10px 14px;border:0;border-radius:8px;margin-top:8px}pre{white-space:pre-wrap;background:#000;padding:12px;border-radius:8px}</style>";
  html += "</head><body>";
  html += "<h1>Landis+Gyr E320 Reader</h1>";
  html += "<div class='card'><b>Version:</b> "; html += VEG_VERSION; html += "<br>";
  html += "<b>IP:</b> "; html += wifiIpAddress(); html += "</div>";
  html += "<div class='card'><h2>Status</h2><p>WLAN: ";
  html += wifiIsConnected() ? "<span class='ok'>verbunden</span>" : "<span class='warn'>nicht verbunden</span>";
  html += "</p><p>SML: ";
  html += smlHasData() ? "<span class='ok'>Daten empfangen</span>" : "<span class='warn'>noch keine Daten</span>";
  html += "</p><p><a href='/api/status'>JSON Status</a> | <a href='/api/sml'>SML Status</a> | <a href='/sml/raw'>SML Rohdaten</a> | <a href='/log'>Log</a></p></div>";
  html += "<div class='card'><h2>Firmware Update über WLAN</h2>";
  html += "<p>Hier die von PlatformIO erzeugte <b>firmware.bin</b> auswählen.</p>";
  html += "<form method='POST' action='/update' enctype='multipart/form-data'>";
  html += "<input type='file' name='firmware' accept='.bin' required><br>";
  html += "<input type='submit' value='Firmware installieren'>";
  html += "</form></div>";
  html += "<div class='card'><h2>System</h2><form method='POST' action='/reboot'><button type='submit'>ESP32 neu starten</button></form></div>";
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

static void handleReboot() {
  server.send(200, "text/html", "<html><body><h2>Neustart...</h2><p>Bitte in einigen Sekunden neu laden.</p></body></html>");
  delay(250);
  ESP.restart();
}

static void handleUpdateFinished() {
  const bool ok = !Update.hasError();
  server.sendHeader("Connection", "close");
  server.send(200, "text/html",
    ok
      ? "<html><body><h2>Update erfolgreich</h2><p>Der ESP32 startet neu.</p></body></html>"
      : "<html><body><h2>Update fehlgeschlagen</h2><p>Bitte Log und Firmware-Datei prüfen.</p></body></html>");
  if (ok) rebootAfterUpdate = true;
}

static void handleUpdateUpload() {
  HTTPUpload &upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    logInfo("OTA Update gestartet: " + upload.filename);
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
      logError("OTA Update.begin fehlgeschlagen");
    }
  }
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
      logError("OTA Schreibfehler");
    }
  }
  else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      logInfo("OTA erfolgreich, Bytes: " + String(upload.totalSize));
    } else {
      Update.printError(Serial);
      logError("OTA Update.end fehlgeschlagen");
    }
  }
  else if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    logError("OTA Upload abgebrochen");
  }
}

void webserverSetup() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/sml", HTTP_GET, handleSml);
  server.on("/sml/raw", HTTP_GET, handleSmlRaw);
  server.on("/log", HTTP_GET, handleLog);
  server.on("/reboot", HTTP_POST, handleReboot);
  server.on("/update", HTTP_POST, handleUpdateFinished, handleUpdateUpload);
  server.begin();
  logInfo("Webserver gestartet auf Port 80");
}

void webserverLoop() {
  server.handleClient();
  if (rebootAfterUpdate) {
    delay(750);
    ESP.restart();
  }
}
