# Cyrus BT Controller

ESPHome firmware + Home Assistant package that set the volume of a
**Cyrus ONE** (2016) or **Cyrus ONE HD** amplifier a few seconds after it is
powered on.

Both amplifiers have a mechanical power switch and **do not remember the last
volume setting** — after every power-on they come back at an unpredictable
level. This project restores a fixed, safe volume automatically and
deterministically.

## Why not just the Home Assistant BLE integration?

The [cyrus-one-hass](https://github.com/vitkuv/cyrus-one-hass) integration
works well for day-to-day control, but its BLE connection time after the
amplifier boots is random (optimistically ~7-8 s, worst case ~40 s). That
makes the volume moment after power-on audible at the wrong level.

This project uses a dedicated ESP32 (M5Stack Atom S3U) sitting next to the
amplifier. It talks raw NimBLE directly to the amp, so the volume is written
in a stable ~8 s after power-on, after which the connection is released and
the HA integration is free to connect — no race.

## How it works

The BLE module inside the amplifier has a two-stage boot: right after power
it advertises under one random MAC, and ~4 s later it re-advertises under a
second, stable MAC (the first one disappears). Both the MAC **and** the
advertised name suffix change on every power cycle; the only stable
identifier is the `ONE-` name prefix (this mirrors the `local_name: "ONE-*"`
matcher used by the [HA integration](https://github.com/vitkuv/cyrus-one-hass)).

The controller therefore:

1. waits for any advertisement whose name starts with `ONE-` (first MAC),
2. waits for a second, different MAC under the same name (~4 s),
3. connects to the second MAC and writes the absolute volume (0-90) over GATT
   (service `bc2f4cc6-aaef-4351-9034-d66268e328f0`, characteristic
   `06d1e5e7-79ad-4a71-8faa-373789f7d93c` — the same one the official Cyrus
   app and the HA integration use, so this works for both ONE and ONE HD),
4. disconnects and pulses the `Cyrus Volume Set` binary sensor.

The power trigger is deliberately **not** part of the firmware: any HA switch
entity (Shelly Plug, TP-Link, Zigbee plug, ...) drives it — see
`home-assistant/cyrus_amp.yaml`.

## Repository layout

- `esphome/` — ESPHome firmware for the M5Stack Atom S3U
  - `cyrus-bt-controller.yaml` — device configuration
  - `components/cyrus_ble/` — custom ESPHome component (NimBLE scanner,
    connection and volume write, status LED state machine)
- `home-assistant/cyrus_amp.yaml` — HA package: template switch, automation
  and script; adjust `switch.amplifier_plug` to your plug entity

## Setup

1. ESPHome: copy `esphome/secrets.yaml.example` to `esphome/secrets.yaml`,
   fill in Wi-Fi and generated keys, flash the firmware.
2. Home Assistant: include `home-assistant/cyrus_amp.yaml` as a package and
   replace `switch.amplifier_plug` with your smart plug entity.
3. Set the volume in `home-assistant/cyrus_amp.yaml`
   (`esphome.cyrus_bt_over_wifi_controller_set_cyrus_volume`, absolute 0-90;
   46 is the default). You can also call the service from any other automation
   or script.

## Status LED (M5Stack Atom S3U)

- cyan pulse — boot / connecting to Wi-Fi / HA
- green pulse — waiting for the first MAC, then the second MAC
- green steady — BLE connected, volume being written
- white pulse — volume written successfully, then off
- red pulse — error / timeout
