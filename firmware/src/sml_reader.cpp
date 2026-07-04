#include "sml_reader.h"
#include "sml_parser.h"
#include "logger.h"

#if __has_include("config.h")
  #include "config.h"
#else
  #include "config.example.h"
#endif

static HardwareSerial SmlSerial(1);
static String lastTelegramHex;
static unsigned long lastByteTime = 0;
static unsigned long lastTelegramTime = 0;
static bool hasDataFlag = false;
static uint32_t telegramCount = 0;
static uint8_t buffer[1024];
static size_t bufferLen = 0;

static String byteToHex(uint8_t b) {
  char out[4];
  snprintf(out, sizeof(out), "%02X", b);
  return String(out);
}

static void resetBuffer() {
  bufferLen = 0;
}

static void finishTelegram() {
  if (bufferLen == 0) return;

  String hex;
  for (size_t i = 0; i < bufferLen; i++) {
    if (i > 0) hex += " ";
    hex += byteToHex(buffer[i]);
  }

  lastTelegramHex = hex;
  lastTelegramTime = millis();
  hasDataFlag = true;
  telegramCount++;

  logInfo(String("SML Telegramm empfangen, Bytes: ") + String(bufferLen));
  smlParserParseHex(lastTelegramHex);
  resetBuffer();
}

void smlSetup() {
  SmlSerial.begin(VEG_SML_BAUD, SERIAL_8N1, VEG_IR_RX_PIN, VEG_IR_TX_PIN);
  smlParserReset();
  logInfo(String("SML Reader gestartet, Baudrate: ") + String(VEG_SML_BAUD));
  logInfo(String("IR RX Pin: ") + String(VEG_IR_RX_PIN));
}

void smlLoop() {
  while (SmlSerial.available()) {
    uint8_t b = SmlSerial.read();
    lastByteTime = millis();

    if (bufferLen < sizeof(buffer)) {
      buffer[bufferLen++] = b;
    } else {
      logWarn("SML Puffer voll, Telegramm wird verworfen");
      resetBuffer();
    }
  }

  if (bufferLen > 0 && millis() - lastByteTime > 200) {
    finishTelegram();
  }
}

String smlStatusJson() {
  String json = "{";
  json += "\"has_data\":" + String(hasDataFlag ? "true" : "false") + ",";
  json += "\"telegram_count\":" + String(telegramCount) + ",";
  json += "\"last_age_ms\":" + String(lastTelegramTime == 0 ? 0 : millis() - lastTelegramTime) + ",";
  json += "\"baud\":" + String(VEG_SML_BAUD) + ",";
  json += "\"rx_pin\":" + String(VEG_IR_RX_PIN) + ",";
  json += "\"parser\":" + smlParserStatusJson() + ",";
  json += "\"obis\":" + smlParserObisJson();
  json += "}";
  return json;
}

String smlLastTelegramHex() {
  return lastTelegramHex;
}

bool smlHasData() {
  return hasDataFlag;
}
