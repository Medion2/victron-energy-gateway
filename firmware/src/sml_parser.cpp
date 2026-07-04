#include "sml_parser.h"
#include "logger.h"
#include <math.h>

static SmlValues values;
static String foundObisJson = "[]";

struct TlInfo { uint8_t type; size_t headerLen; size_t payloadLen; size_t count; bool list; bool optional; };

static bool containsHex(const String &h, const String &n) { return h.indexOf(n) >= 0; }
static String detectManufacturer(const String &hex) { return containsHex(hex, "4C 47 5A") ? "Landis+Gyr" : "Unbekannt"; }
static bool detectObis(const String &hex, const String &obisHex) { return containsHex(hex, obisHex); }

static int hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return 10 + c - 'A';
  if (c >= 'a' && c <= 'f') return 10 + c - 'a';
  return -1;
}

static size_t hexToBytes(const String &hex, uint8_t *out, size_t maxLen) {
  size_t count = 0; int high = -1;
  for (size_t i = 0; i < hex.length(); i++) {
    int n = hexNibble(hex[i]);
    if (n < 0) continue;
    if (high < 0) high = n;
    else { if (count < maxLen) out[count++] = (uint8_t)((high << 4) | n); high = -1; }
  }
  return count;
}

static int findPattern(const uint8_t *data, size_t len, const uint8_t *pattern, size_t patternLen) {
  if (patternLen == 0 || len < patternLen) return -1;
  for (size_t i = 0; i <= len - patternLen; i++) {
    bool ok = true;
    for (size_t j = 0; j < patternLen; j++) if (data[i + j] != pattern[j]) { ok = false; break; }
    if (ok) return (int)i;
  }
  return -1;
}

static bool parseTl(const uint8_t *data, size_t len, size_t pos, TlInfo &tl) {
  if (pos >= len) return false;
  uint8_t b = data[pos];
  tl.type = (b & 0x70) >> 4;
  tl.headerLen = 1;
  tl.list = (tl.type == 7);
  tl.optional = (b == 0x01);
  tl.payloadLen = 0;
  tl.count = 0;
  if (tl.optional) return true;
  size_t lf = b & 0x0F;
  if (tl.list) { tl.count = lf; return true; }
  if (lf == 0 || pos + lf > len) return false;
  tl.payloadLen = lf - 1;
  return true;
}

static bool skipElement(const uint8_t *data, size_t len, size_t &pos, uint8_t depth = 0) {
  if (depth > 12 || pos >= len) return false;
  TlInfo tl;
  if (!parseTl(data, len, pos, tl)) return false;
  if (tl.optional) { pos += 1; return true; }
  if (tl.list) {
    pos += 1;
    for (size_t i = 0; i < tl.count; i++) if (!skipElement(data, len, pos, depth + 1)) return false;
    return true;
  }
  pos += 1 + tl.payloadLen;
  return pos <= len;
}

static bool readNumber(const uint8_t *data, size_t len, size_t &pos, int64_t &rawOut) {
  TlInfo tl;
  if (!parseTl(data, len, pos, tl)) return false;
  if (tl.list || tl.optional || !(tl.type == 5 || tl.type == 6)) return false;
  if (tl.payloadLen == 0 || tl.payloadLen > 8 || pos + 1 + tl.payloadLen > len) return false;
  uint64_t raw = 0;
  for (size_t i = 0; i < tl.payloadLen; i++) raw = (raw << 8) | data[pos + 1 + i];
  if (tl.type == 5) { int shift = (8 - tl.payloadLen) * 8; rawOut = ((int64_t)raw << shift) >> shift; }
  else rawOut = (int64_t)raw;
  pos += 1 + tl.payloadLen;
  return true;
}

static bool findEntryStart(const uint8_t *data, size_t len, const uint8_t *obis, size_t &entryStart) {
  int obisIndex = findPattern(data, len, obis, 7);
  if (obisIndex < 0) return false;
  for (int p = obisIndex - 1; p >= 0 && p >= obisIndex - 12; p--) {
    if (data[p] == 0x77) { entryStart = (size_t)p; return true; }
  }
  return false;
}

static bool extractValue(const uint8_t *data, size_t len, const uint8_t *obis, double &valueOut) {
  size_t pos = 0;
  if (!findEntryStart(data, len, obis, pos)) return false;
  TlInfo list;
  if (!parseTl(data, len, pos, list) || !list.list || list.count < 6) return false;
  pos += 1;
  if (!skipElement(data, len, pos)) return false; // objName
  if (!skipElement(data, len, pos)) return false; // status
  if (!skipElement(data, len, pos)) return false; // valTime
  int64_t unit = 0; size_t pUnit = pos;
  if (readNumber(data, len, pUnit, unit)) pos = pUnit; else if (!skipElement(data, len, pos)) return false;
  int64_t scalerRaw = 0; int scaler = 0; size_t pScaler = pos;
  if (readNumber(data, len, pScaler, scalerRaw)) { scaler = (int)scalerRaw; pos = pScaler; }
  else if (!skipElement(data, len, pos)) return false;
  int64_t rawValue = 0;
  if (!readNumber(data, len, pos, rawValue)) return false;
  valueOut = (double)rawValue * pow(10.0, scaler);
  if (unit == 30) valueOut /= 1000.0;
  return true;
}

void smlParserReset() { values = SmlValues(); foundObisJson = "[]"; }

void smlParserParseHex(const String &hexTelegram) {
  if (hexTelegram.length() == 0) return;
  uint8_t bytes[1024];
  size_t len = hexToBytes(hexTelegram, bytes, sizeof(bytes));
  String obis = "["; bool first = true;
  String manufacturer = detectManufacturer(hexTelegram);
  if (manufacturer != "Unbekannt") { values.manufacturer = manufacturer; values.hasManufacturer = true; }
  const uint8_t obisImport[7] = {0x07,0x01,0x00,0x01,0x08,0x00,0xFF};
  const uint8_t obisExport[7] = {0x07,0x01,0x00,0x02,0x08,0x00,0xFF};
  const uint8_t obisPower[7]  = {0x07,0x01,0x00,0x10,0x07,0x00,0xFF};
  if (detectObis(hexTelegram, "07 01 00 01 08 00 FF")) { values.hasEnergyImport = true; double v = 0; if (extractValue(bytes, len, obisImport, v)) values.energyImportKwh = v; if (!first) obis += ","; obis += "\"1.8.0 Bezug\""; first = false; }
  if (detectObis(hexTelegram, "07 01 00 02 08 00 FF")) { values.hasEnergyExport = true; double v = 0; if (extractValue(bytes, len, obisExport, v)) values.energyExportKwh = v; if (!first) obis += ","; obis += "\"2.8.0 Einspeisung\""; first = false; }
  if (detectObis(hexTelegram, "07 01 00 10 07 00 FF")) { values.hasPower = true; double v = 0; if (extractValue(bytes, len, obisPower, v)) values.powerW = v; if (!first) obis += ","; obis += "\"16.7.0 Leistung\""; first = false; }
  obis += "]"; foundObisJson = obis;
  logInfo(String("SML Parser: ") + values.manufacturer + ", OBIS=" + foundObisJson + ", 1.8.0=" + String(values.energyImportKwh, 3) + ", 2.8.0=" + String(values.energyExportKwh, 3));
}

SmlValues smlParserGetValues() { return values; }

String smlParserStatusJson() {
  String json = "{";
  json += "\"manufacturer\":\"" + values.manufacturer + "\",";
  json += "\"has_manufacturer\":" + String(values.hasManufacturer ? "true" : "false") + ",";
  json += "\"has_energy_import\":" + String(values.hasEnergyImport ? "true" : "false") + ",";
  json += "\"energy_import_kwh\":" + String(values.energyImportKwh, 3) + ",";
  json += "\"has_energy_export\":" + String(values.hasEnergyExport ? "true" : "false") + ",";
  json += "\"energy_export_kwh\":" + String(values.energyExportKwh, 3) + ",";
  json += "\"has_power\":" + String(values.hasPower ? "true" : "false") + ",";
  json += "\"power_w\":" + String(values.powerW, 3);
  json += "}";
  return json;
}

String smlParserObisJson() { return foundObisJson; }
