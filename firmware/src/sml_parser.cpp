#include "sml_parser.h"
#include "logger.h"

static SmlValues values;
static String foundObisJson = "[]";

static bool containsHex(const String &haystack, const String &needle) {
  return haystack.indexOf(needle) >= 0;
}

static String detectManufacturer(const String &hex) {
  // 4C 47 5A = LGZ = Landis+Gyr
  if (containsHex(hex, "4C 47 5A")) return "Landis+Gyr";
  return "Unbekannt";
}

static bool detectObis(const String &hex, const String &obisHex) {
  return containsHex(hex, obisHex);
}

void smlParserReset() {
  values = SmlValues();
  foundObisJson = "[]";
}

void smlParserParseHex(const String &hexTelegram) {
  if (hexTelegram.length() == 0) return;

  String obis = "[";
  bool first = true;

  String manufacturer = detectManufacturer(hexTelegram);
  if (manufacturer != "Unbekannt") {
    values.manufacturer = manufacturer;
    values.hasManufacturer = true;
  }

  // OBIS 1.8.0 = 07 01 00 01 08 00 FF
  if (detectObis(hexTelegram, "07 01 00 01 08 00 FF")) {
    values.hasEnergyImport = true;
    if (!first) obis += ",";
    obis += "\"1.8.0 Bezug\"";
    first = false;
  }

  // OBIS 2.8.0 = 07 01 00 02 08 00 FF
  if (detectObis(hexTelegram, "07 01 00 02 08 00 FF")) {
    values.hasEnergyExport = true;
    if (!first) obis += ",";
    obis += "\"2.8.0 Einspeisung\"";
    first = false;
  }

  // OBIS 16.7.0 = 07 01 00 10 07 00 FF
  if (detectObis(hexTelegram, "07 01 00 10 07 00 FF")) {
    values.hasPower = true;
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
  json += "\"has_energy_export\":" + String(values.hasEnergyExport ? "true" : "false") + ",";
  json += "\"has_power\":" + String(values.hasPower ? "true" : "false");
  json += "}";
  return json;
}

String smlParserObisJson() {
  return foundObisJson;
}
