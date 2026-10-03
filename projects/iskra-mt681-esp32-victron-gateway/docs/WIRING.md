# Wiring

## Optical SML reader → WT32-ETH01

| Optical reader | ESP32 |
|---|---|
| TX | GPIO4 (RX) |
| RX | GPIO2 (TX) |
| GND | GND |
| VCC | According to the optical reader specification |

UART: **9600 baud, 8N1**

> GPIO2 is a boot-strapping pin on the classic ESP32. If the board has boot problems with the
> optical reader connected, verify that the reader is not forcing GPIO2 to an invalid level during boot.

## Ethernet PHY configuration

The firmware is configured for the commonly used WT32-ETH01 LAN8720 mapping:

| Function | GPIO / value |
|---|---|
| PHY | LAN8720 |
| PHY address | 1 |
| MDC | GPIO23 |
| MDIO | GPIO18 |
| PHY power | GPIO16 |
| Clock | GPIO0 IN |

Board variants can differ. Verify your hardware before changing PHY settings.
