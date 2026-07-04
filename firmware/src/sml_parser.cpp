#include "sml_parser.h"
#include "logger.h"
#include <math.h>

static SmlValues values;
static String foundObisJson = "[]";

static bool containsHex(const String &haystack, const String &needle) {
  return haystack.indexOf(needle) >= 0;
}

static String detectManufacturer(const String &hex) {
  if (containsHex(hex, "4C 47 5A")) return "Landis+Gyr";
  return "Unbekannt";
}

static bool detectObis(const String &hex, const String &obisHex) {
  return containsHex(hex, obisHex);
}

static int hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return 10 + c - 'A';
  if (c >= 'a' && c <= 'f') return 10 + c - 'a';
  return -1;
}

static size_t hexToBytes(const String &hex, uint8_t *out, size_t maxLen) {
  size_t count = 0;
  int high = -1;
  for (size_t i = 0; i < hex.length(); i++) {
    int n = hexNibble(hex[i]);
    if (n < 0) continue;
    if (high < 0) {
      high = n;
    } else {
      if (count < maxLen) out[count++] = (uint8_t)((high << 4) | n);
      high = -1;
    }
  }
  return count;
}

static int findPattern(const uint8_t *data, size_t len, const uint8_t *pattern, size_t patternLen) {
  if (patternLen == 0 || len < patternLen) return -1;
  for (size_t i = 0; i <= len - patternLen; i++) {
    bool ok = true;
    for (size_t j = 0; j < patternLen; j++) {
      if (data[i + j] != pattern[j]) {
        ok = false;
        break;
      }
    }
    if (ok) return (int)i;
  }
  return -1;
}

static bool readSignedInteger(const uint8_t *data, size_t len, size_t pos, int64_t &raw, size_t &nextPos) {
  if (pos >= len) return false;
  uint8_t type = data[pos];
  size_t bytes = 0;

  if (type == 0x62) bytes = 1;
  else if (type == 0x63) bytes = 2;
  else if (type == 0x65) bytes = 4;
  else if (type == 0x69) bytes = 8;
  else return false;

  if (pos + 1 + bytes > len) return false;

  raw = 0;
  for (size_t i = 0; i < bytes; i++) {
    raw = (raw << 8) | data[pos + 1 + i];
  }

  int shift = (8 - bytes) * 8;
  raw = (raw << shift) >> shift;
  nextPos = pos + 1 + bytes;
  return true;
}

static bool extractScaler(const uint8_t *data, size_t len, size_t start, size_t end, int &scalerOut) {
  for (size_t p = start; p + 1 < end && p + 1 < len; p++) {
    if (data[p] == 0x52) {
      scalerOut = (int8_t)data[p + 1];
      return true;
    }
  }
  return false;
}

static bool extractFirstIntegerAfter(const uint8_t *data, size_t len, size_t start, size_t end, int64_t &rawOut) {
  for (size_t p = start; p < end && p < len; p++) {
    int64_t raw = 0;
    size_t next = p;
    if (readSignedInteger(data, len, p, raw, next)) {
      rawOut = raw;
      return true;
    }
  }
  return false;
}

static bool extractValueNearObis(const uint8_t *data, size_t len, const uint8_t *obis, double &valueOut) {
  int idx = findPattern(data, len, obis, 7);
  if (idx < 0) return false;

  size_t start = (size_t)idx + 7;
  size_t end = min(len, (size_t)idx + 96);

  int scaler = 0;
  bool hasScaler = extractScaler(data, len, start, end, scaler);

  // Nach dem Scaler kommt in vielen SML ListEntries der eigentliche Wert.
  // Dadurch vermeiden wir, Status/Zeit als Messwert zu lesen.
  size_t valueStart = start;
  if (hasScaler) {
    for (size_t p = start; p + 1 < end && p + 1 < len; p++) {
      if (data[p] == 0x52) {
        valueStart = p + 2;
        break;
      }
    }
  }

  int64_t raw = 0;
  if (!extractFirstIntegerAfter(data, len, valueStart, end, raw)) return false;

  valueOut = (double)raw;
  if (hasScaler) valueOut = valueOut * pow(10.0, scaler);
  return true;
}

void smlParserReset() {
  values = SmlValues();
  foundObisJson = "[]";
}

void smlParserParseHex(const String &hexTelegram) {
  if (hexTelegram.length() == 0) return;

  uint8_t bytes[1024];
  size_t len = hexToBytes(hexTelegram, bytes, sizeof(bytes));

  String obis = "[";
  bool first = true;

  String manufacturer = detectManufacturer(hexTelegram);
  if (manufacturer != "Unbekannt") {
    values.manufacturer = manufacturer;
    values.hasManufacturer = true;
  }

  const uint8_t obisImport[7] = {0x07,0x01,0x00,0x01,0x08,0x00,0xFF};
  const uint8_t obisExport[7] = {0x07,0x01,0x00,0x02,0x08,0x00,0xFF};
  const uint8_t obisPower[7]  = {0x07,0x01,0x00,0x10,0x07,0x00,0xFF};

  if (detectObis(hexTelegram, "07 01 00 01 08 00 FF")) {
    values.hasEnergyImport = true;
    double v = 0;
    if (extractValueNearObis(bytes, len, obisImport, v)) values.energyImportKwh = v;
    if (!first) obis += ",";
    obis += "\"1.8.0 Bezug\"";
    first = false;
  }

  if (detectObis(hexTelegram, "07 01 00 02 08 00 FF")) {
    values.hasEnergyExport = true;
    double v = 0;
    if (extractValueNearObis(bytes, len, obisExport, v)) values.energyExportKwh = v;
    if (!first) obis += ",";
    obis += "\"2.8.0 Einspeisung\"";
    first = false;
  }

  if (detectObis(hexTelegram, "07 01 00 10 07 00 FF")) {
    values.hasPower = true;
    double v = 0;
    if (extractValueNearObis(bytes, len, obisPower, v)) values.powerW = v;
    if (!first) obis += ",";
    obis += "\"16.7.0 Leistung\"";
    first = false;
  }

  obis += "]";
  foundObisJson = obis;

  logInfo(String("SML Parser: Hersteller=") + (values.hasManufacturer ? values.manufacturer : "unbekannt") + ", OBIS=" + foundObisJson);
}

SmlValues smlParserGetValues() {
  return values;
}

String smlParserStatusJson() {
  String json = "{";
  json += "\"manufacturer\":\"" + values.manufacturer + "\",";
  json += "\"has_manufacturer\":" + String(values.hasManufacturer ? "true" : "false") + ",";
  json += "\"has_energy_import\":" + String(values.hasEnergyImport ? "true" : "false") + ",";
  json += "\"energy_import_raw\":" + String(values.energyImportKwh, 3) + ",";
  json += "\"has_energy_export\":" + String(values.hasEnergyExport ? "true" : "false") + ",";
  json += "\"energy_export_raw\":" + String(values.energyExportKwh, 3) + ",";
  json += "\"has_power\":" + String(values.hasPower ? "true" : "false") + ",";
  json += "\"power_raw\":" + String(values.powerW, 3);
  json += "}";
  return json;
}

String smlParserObisJson() {
  return foundObisJson;
}
