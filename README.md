# esp32-fetch-data

Firmware for an ESP32 that fetches a text data file over WiFi and prints it to a
character display. It is built for low-power, battery-friendly operation and is
configured entirely from a `config.json` file, so the same firmware can drive
different displays, triggers, and timing without recompiling.

## Features

- **Low-power operation** — the device wakes, fetches data, shows the result for
  a configured window, then powers down with deep sleep. A button press (or other
  configured trigger) wakes it again, as if the power were switched back on. This
  lets a small battery pack last far longer than a continuously powered display.
- **Offline cache** — the last successful reading is stored in non-volatile
  memory. If a fetch fails, the device shows a brief `Connection Error` notice and
  then falls back to the cached value (`Fetch memory: <value>`). On the very first
  run with nothing cached, it shows `Fetch memory: <none>`.
- **File-based configuration** — WiFi, data URL, display type and size, the
  trigger action, timing, and the shutdown method are all set in `config.json`.
  Every option has a safe built-in default, so the firmware runs even when no
  config file is present.

## Hardware

The reference circuit uses:

- An ESP32 development board
- A 16×2 character LCD with an I²C backpack (default address `0x27`)
- A momentary pushbutton as the fetch trigger

| Signal      | ESP32 pin | Notes                                            |
| ----------- | --------- | ------------------------------------------------ |
| LCD SDA     | GPIO 21   | I²C data                                         |
| LCD SCL     | GPIO 22   | I²C clock                                         |
| LCD VCC/GND | 5V / GND  | Backpack power                                   |
| Button      | GPIO 33   | RTC-capable pin (required to wake from deep sleep) |

> **Why GPIO 33 for the button?** Deep-sleep wake-on-press uses the `ext0`
> source, which only works on RTC-capable GPIOs. Pins such as GPIO 5 cannot wake
> the chip, so the button defaults to an RTC pin. You can change it in
> `config.json` to any other RTC-capable GPIO.

## Repository layout

```
esp32-fetch-data.ino   Main sketch: boot, fetch cycle, sleep/wake logic
Config.h               config.json loader (LittleFS + ArduinoJson) with defaults
Display.h              Display abstraction, I²C LCD driver, serial fallback
Storage.h              Non-volatile cache of the last fetched value (NVS)
config.json            Runtime configuration uploaded to the device filesystem
config.example.json    Annotated example configuration
platformio.ini         Build configuration and library dependencies
extra_script.py        Stages config.json into the LittleFS image at build time
diagram.json           Wokwi circuit definition
docs/                  Installation and configuration guides
```

## Quick start (PlatformIO)

```bash
# Build the firmware
pio run

# Flash the firmware
pio run -t upload

# Upload config.json to the device's LittleFS filesystem
pio run -t uploadfs

# Watch the serial output
pio device monitor
```

Edit `config.json` before running `uploadfs` to point the device at your own
network and data URL:

```json
{
  "wifi": { "ssid": "YourNetwork", "password": "your-password" },
  "data": { "url": "https://example.com/data.txt" }
}
```

See [docs/installation.md](docs/installation.md) for the Arduino IDE workflow and
[docs/configuration.md](docs/configuration.md) for every available option.

## How it works

On each boot or wake the firmware runs a single fetch cycle:

1. Connect to WiFi (progress shown on the display).
2. Fetch the data file and sanitize it to fit the display.
3. On success, store the value and show it; on failure, show the cached value.
4. Keep the result on screen for the configured window.
5. Configure the wake source and enter deep sleep.

When `power.shutdown.method` is set to `"none"`, the firmware instead stays awake
and polls the button (and refreshes on the timer interval) like a classic
always-on display.
