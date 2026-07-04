# Victron Energy Gateway (VEG)

ESP32-S3 Gateway zum Auslesen von SML-Stromzählern wie Landis+Gyr E320 und zur Weitergabe der Werte an Home Assistant und Victron/Cerbo GX.

## Ziel

VEG soll ältere oder externe Energiequellen über einen optischen IR-Lesekopf erfassen und später in Victron/VRM als PV-Wechselrichter darstellen.

## Aktueller Stand

### Version 0.3 – Kommunikations-Engine

- ESP32-S3 DevKitC-1 / WROOM-1-N16R8
- Landis+Gyr E320 über Hichi IR-Lesekopf
- SML-Rohdatenempfang
- MQTT-Grundlage
- Home Assistant Discovery geplant
- Victron-Integration ab Version 0.4 geplant

## Hardware

- ARCELI ESP32-S3 WROOM-1-N16R8 ESP32-S3-DevKitC-1
- Hichi IR-Lesekopf TTL
- Landis+Gyr E320
- Victron Cerbo GX mit Venus OS Large

## Projektstruktur

```text
firmware/     PlatformIO-Firmware
docs/         Dokumentation
hardware/     Anschlusspläne und Bilder
examples/     Beispielkonfigurationen
```

## Roadmap

- v0.3: MQTT, Home Assistant Discovery, Landis+Gyr Diagnose
- v0.4: Victron PV-Wechselrichter / Cerbo GX
- v1.0: stabiles Dashboard, OTA, Diagnose, Konfiguration über Browser
