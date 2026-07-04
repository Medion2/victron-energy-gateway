#pragma once
#include <Arduino.h>

struct SmlValues {
  bool hasManufacturer = false;
  bool hasEnergyImport = false;
  bool hasEnergyExport = false;
  bool hasPower = false;
  String manufacturer = "";
  double energyImportKwh = 0.0;
  double energyExportKwh = 0.0;
  double powerW = 0.0;
};

void smlParserReset();
void smlParserParseHex(const String &hexTelegram);
SmlValues smlParserGetValues();
String smlParserStatusJson();
String smlParserObisJson();
