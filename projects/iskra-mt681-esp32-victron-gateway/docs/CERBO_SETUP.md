# Cerbo GX / Venus OS setup

This project uses MQTT together with `dbus-mqtt-devices` to expose the ESP32 data as a Victron
`pvinverter`.

## Required service

Install `dbus-mqtt-devices` on Venus OS and verify that the service is running:

```sh
svstat /service/dbus-mqtt-devices
```

Expected state:

```text
/service/dbus-mqtt-devices: up ...
```

## Firmware configuration

Open the ESP32 web interface and select **Cerbo GX**. Enter the Cerbo GX IP address or hostname.

The ESP32 publishes a registration message for service type:

```text
pvinverter
```

The firmware automatically repeats registration while no Victron device instance has been assigned.
After successful registration, the dashboard should show a non-negative `PV Instance`, for example `1`.

## Values published

- `Ac/Power`
- `Ac/L1/Power`
- `Ac/L2/Power`
- `Ac/L3/Power`
- `Ac/L1/Voltage`
- `Ac/L2/Voltage`
- `Ac/L3/Voltage`
- `Ac/L1/Current`
- `Ac/L2/Current`
- `Ac/L3/Current`
- `Ac/Current`
- `Ac/Energy/Forward`
- `Ac/MaxPower`
- `ErrorCode`
- `StatusCode`
- `Position`

The three voltage values are nominal fixed values of 230 V and are **not meter measurements**.
The current values are calculated from active power and the fixed voltage.
