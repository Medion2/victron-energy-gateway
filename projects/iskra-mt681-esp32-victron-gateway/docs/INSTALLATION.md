# Installation

## Arduino IDE

Install:

- ESP32 board package
- `PubSubClient` by Nick O'Leary

Use the WT32-ETH01 board definition that matches your installed ESP32 Arduino core.

Do **not** copy `PubSubClient.cpp` or `PubSubClient.h` into the sketch folder when the library is
already installed through Arduino Library Manager. Doing both can cause `multiple definition`
linker errors.

## Ethernet addressing

Public release defaults to **DHCP**:

```cpp
static const bool USE_STATIC_ETH_IP = false;
```

To use a static address:

1. set `USE_STATIC_ETH_IP = true`
2. edit `ETH_IP`, `ETH_GATEWAY`, `ETH_SUBNET`, and DNS values
3. make sure the chosen address does not conflict with another device

## First-start Wi-Fi fallback

If Ethernet is unavailable and no saved Wi-Fi configuration exists, the firmware creates:

```text
SSID: Iskra-MT681-Setup
```

Open the setup page and store the Wi-Fi credentials.

## OTA

The web interface provides firmware upload through `/update`.

Use an OTA-capable ESP32 partition scheme. Avoid changing the partition layout through a normal
application-only OTA update.
