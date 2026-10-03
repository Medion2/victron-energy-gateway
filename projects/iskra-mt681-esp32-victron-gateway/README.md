# Iskra MT681 ESP32 Victron Gateway

Open-source gateway for reading **SML telegrams from an Iskra MT681 electricity meter** with a
**WT32-ETH01 / ESP32-ETH01** and publishing selected values to a **Victron Cerbo GX / Venus OS**
as a `pvinverter` via MQTT and `dbus-mqtt-devices`.

## What it does

- Reads SML at 9600 baud, 8N1
- Decodes:
  - `1.8.0` imported energy
  - `2.8.0` exported energy
  - `16.7.0` total active power
  - `36.7.0` L1 active power
  - `56.7.0` L2 active power
  - `76.7.0` L3 active power
- Ethernet via LAN8720 on WT32-ETH01
- Wi-Fi fallback / first-start setup AP
- Browser dashboard
- Web OTA firmware update
- MQTT registration as a Victron `pvinverter`
- Automatic re-registration after reboot/OTA
- Fixed nominal 230 V per phase for display
- Calculated phase current: `I = P / 230 V`
- Calculated overall display current: `I = P_total / (3 × 230 V)`

## Important limitation

The project measures at the **grid meter**. Therefore the value shown as PV power represents
the meter's **export power**, not necessarily the PV inverter's true gross AC production.

Example:

- PV inverter produces 7 kW
- house consumes 1 kW at the same time
- grid meter measures about 6 kW export
- Cerbo shows about 6 kW through this gateway

This distinction is important for VRM statistics and energy interpretation.

## Tested architecture

```text
PV inverter
    │
    ▼
House / AC grid
    │
    ▼
Iskra MT681
    │ optical SML
    ▼
WT32-ETH01 / ESP32-ETH01
    │ Ethernet / MQTT
    ▼
Victron Cerbo GX / Venus OS
    │
    ▼
dbus-mqtt-devices → pvinverter
```

## Privacy / public release

This repository contains **no private IP addresses, Wi-Fi credentials, portal IDs, user names,
or site-specific identifiers** from the original installation.

The public firmware uses **DHCP by default**. Example static IP values use the generic
`192.168.1.0/24` private range and must be changed for your own network.

## Requirements

- Iskra MT681 with accessible SML output
- compatible optical SML reader
- WT32-ETH01 / ESP32-ETH01 with LAN8720
- Arduino IDE with ESP32 board support
- `PubSubClient` by Nick O'Leary
- Victron Cerbo GX / Venus OS
- `dbus-mqtt-devices` installed on the GX device

## Quick start

1. Open `src/Iskra_MT681_ESP32_Victron_Gateway.ino`.
2. Select the correct WT32-ETH01 board definition.
3. Install `PubSubClient`.
4. Flash the ESP32.
5. Open the device web interface.
6. Enter the Cerbo GX IP/hostname under **Cerbo GX**.
7. Make sure MQTT and `dbus-mqtt-devices` are running on Venus OS.
8. Verify that the web UI reports a valid `PV Instance`.

See the `docs/` folder for wiring and installation details.

## Safety

The optical reader side is intended to be electrically isolated from mains conductors.
Do not open, modify, or wire directly to utility-meter mains terminals unless you are qualified
and authorized to do so.

## License

MIT License. See `LICENSE`.
