# Architektur ab Version 0.4

## Ziel

Das Victron Energy Gateway soll Energiequellen auslesen und die Werte an Home Assistant sowie später an Victron Cerbo GX weitergeben.

## Zielarchitektur

```text
IR-Lesekopf / UART
        │
        ▼
SML Core
        │
        ▼
OBIS Decoder
        │
        ▼
Energy Model
        │
        ├── Web Dashboard
        ├── MQTT
        ├── Home Assistant Discovery
        └── Victron Integration
```

## Module

### Core

- Logger
- WLAN
- Webserver
- OTA
- MQTT
- Konfiguration

### Drivers

- SML
- Landis+Gyr E320
- Hager EHZ
- EMH
- EasyMeter
- DZG
- ABB Aurora
- Shelly

### Victron

- Virtueller PV-Wechselrichter
- D-Bus-Konzept
- Device Instance
- VRM-Anzeige

### UI

- Dashboard
- SML Debug
- MQTT Status
- Systeminformationen
- Firmware Update

## SML Debug-Ausgabe

Jeder erkannte OBIS-Wert soll später so angezeigt werden:

```text
OBIS: 1.8.0
Unit: Wh
Scaler: -1
Raw: 13542613
Value: 13542.613 kWh
```

Dadurch lassen sich Zähler schnell prüfen und neue Modelle leichter integrieren.
