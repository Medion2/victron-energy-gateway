#include <WiFi.h>
#include <WebServer.h>

// Pins bei Bedarf anpassen.
static const int IR_RX_PIN = 16;   // Ausgang des Lesekopfs -> ESP32 RX
static const int IR_TX_PIN = 17;   // ESP32 TX -> Eingang des Lesekopfs
static const uint32_t IR_BAUD = 9600;

const char* AP_SSID = "IR-Tester";
const char* AP_PASS = "12345678";

HardwareSerial irSerial(1);
WebServer server(80);

volatile uint32_t rxBytes = 0;
volatile uint32_t rxChanges = 0;
uint8_t lastRxLevel = HIGH;
uint32_t txPackets = 0;
uint32_t lastSendMs = 0;
bool autoSend = false;
String lastHex = "";

String htmlPage() {
  String p;
  p += F("<!DOCTYPE html><html><head><meta charset='utf-8'>");
  p += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
  p += F("<title>ESP32-S3 IR Tester</title><style>");
  p += F("body{font-family:Arial;background:#111;color:#eee;margin:20px} .card{background:#1d1d1d;padding:16px;border-radius:14px;margin-bottom:14px} button{padding:12px 18px;margin:6px;border:0;border-radius:10px;font-size:16px} .ok{color:#6f6}.bad{color:#f66} code{word-break:break-all}</style></head><body>");
  p += F("<h2>ESP32-S3 IR-Lesekopf Tester</h2>");
  p += F("<div class='card'><b>RX Pegel:</b> <span id='rxlevel'>-</span><br><b>RX Bytes:</b> <span id='rxbytes'>0</span><br><b>RX Pegelwechsel:</b> <span id='changes'>0</span><br><b>TX Testpakete:</b> <span id='txpackets'>0</span></div>");
  p += F("<div class='card'><b>Letzte empfangene Bytes:</b><br><code id='lasthex'>-</code></div>");
  p += F("<div class='card'><button onclick=\"fetch('/send')\">Testpaket senden</button><button onclick=\"fetch('/auto/on')\">Dauertest EIN</button><button onclick=\"fetch('/auto/off')\">Dauertest AUS</button><button onclick=\"fetch('/reset')\">Zaehler reset</button></div>");
  p += F("<script>async function u(){let r=await fetch('/status');let j=await r.json();document.getElementById('rxlevel').textContent=j.rx_level;document.getElementById('rxbytes').textContent=j.rx_bytes;document.getElementById('changes').textContent=j.rx_changes;document.getElementById('txpackets').textContent=j.tx_packets;document.getElementById('lasthex').textContent=j.last_hex||'-';}setInterval(u,500);u();</script>");
  p += F("</body></html>");
  return p;
}

void sendPattern() {
  static const uint8_t pattern[] = {0x1B,0x1B,0x1B,0x1B,0x01,0x01,0x01,0x01,0x55,0xAA,0x00,0xFF};
  irSerial.write(pattern, sizeof(pattern));
  irSerial.flush();
  txPackets++;
}

void setupWeb() {
  server.on("/", HTTP_GET, [](){ server.send(200, "text/html", htmlPage()); });

  server.on("/status", HTTP_GET, [](){
    String json = "{";
    json += "\"rx_level\":" + String(digitalRead(IR_RX_PIN));
    json += ",\"rx_bytes\":" + String(rxBytes);
    json += ",\"rx_changes\":" + String(rxChanges);
    json += ",\"tx_packets\":" + String(txPackets);
    json += ",\"auto_send\":" + String(autoSend ? "true" : "false");
    json += ",\"last_hex\":\"" + lastHex + "\"}";
    server.send(200, "application/json", json);
  });

  server.on("/send", HTTP_GET, [](){ sendPattern(); server.send(200, "text/plain", "OK"); });
  server.on("/auto/on", HTTP_GET, [](){ autoSend = true; server.send(200, "text/plain", "ON"); });
  server.on("/auto/off", HTTP_GET, [](){ autoSend = false; server.send(200, "text/plain", "OFF"); });
  server.on("/reset", HTTP_GET, [](){ rxBytes = 0; rxChanges = 0; txPackets = 0; lastHex = ""; server.send(200, "text/plain", "RESET"); });
  server.begin();
}

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(IR_RX_PIN, INPUT_PULLUP);
  lastRxLevel = digitalRead(IR_RX_PIN);
  irSerial.begin(IR_BAUD, SERIAL_8N1, IR_RX_PIN, IR_TX_PIN);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  setupWeb();

  Serial.println();
  Serial.println("ESP32-S3 IR Tester gestartet");
  Serial.print("WLAN: "); Serial.println(AP_SSID);
  Serial.print("IP: "); Serial.println(WiFi.softAPIP());
}

void loop() {
  server.handleClient();

  uint8_t lvl = digitalRead(IR_RX_PIN);
  if (lvl != lastRxLevel) {
    rxChanges++;
    lastRxLevel = lvl;
  }

  while (irSerial.available()) {
    uint8_t b = irSerial.read();
    rxBytes++;
    char h[4];
    snprintf(h, sizeof(h), "%02X ", b);
    lastHex += h;
    if (lastHex.length() > 300) lastHex.remove(0, 100);
  }

  if (autoSend && millis() - lastSendMs >= 1000) {
    lastSendMs = millis();
    sendPattern();
  }
}
