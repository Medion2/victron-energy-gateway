# Entwicklung

Dieses Projekt wird ab Version 0.4 mit zwei Hauptzweigen entwickelt.

## Branches

- `main` enthält stabile Versionen.
- `develop` enthält neue Funktionen und Teststände.

Neue Funktionen werden zuerst in `develop` entwickelt und getestet. Erst wenn sie stabil sind, werden sie in `main` übernommen.

## Versionierung

Geplantes Schema:

- `v0.4.x` SML-Core und Decoder
- `v0.5.x` Victron-Integration
- `v0.6.x` Weboberfläche, OTA und Einstellungen
- `v1.0.0` stabile 24/7-Version

## OTA-Regel

Da der ESP32 später schwer zugänglich ist, dürfen nur getestete Builds per OTA installiert werden.

Vor jedem OTA-Test:

1. Projekt aktualisieren.
2. `firmware/include/config.h` prüfen.
3. PlatformIO Build ausführen.
4. `firmware.bin` sichern.
5. OTA durchführen.
6. Weboberfläche und SML-Daten prüfen.

## Aktueller Fokus

Version 0.4 konzentriert sich auf einen sauberen SML-Core:

- SML-Telegramme stabil empfangen
- OBIS-Werte korrekt dekodieren
- Unit, Scaler und Raw-Wert sichtbar machen
- Debug-Ausgabe im Browser vorbereiten
