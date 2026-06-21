# esp32-fetch-data

- `ctrl + click` to see the project on [wokwi.com](https://wokwi.com/projects/467393527818771457)

Firmware for an ESP32 that fetches a text data file over WiFi and prints it to a
character display. It is built for low-power, battery-friendly operation and is
configured entirely from a `config.json` file, so the same firmware can drive
different displays, triggers, and timing without recompiling.

## Features

- **Button-gated fetch** — the firmware never fetches on its own. On boot the display
  stays off; a button press (or other configured trigger) runs one fetch and shows the
  result.
- **Display off when idle** — the display lights up only to show a fetch result. After
  `display.displayTimeMs` it turns off and shows nothing until the next action, so the
  device stays dark (and lower power) between fetches.
- **Optional low-power operation** — enable deep sleep in `config.json` and the
  device fetches, shows the result for a configured window, then powers down and
  wakes on the next button press, as if the power were switched back on. This lets a
  small battery pack last far longer than a continuously powered display. (The
  built-in default is the always-on polling mode, which works everywhere — including
  the Wokwi simulator — without relying on deep-sleep wake.)
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
src/main.cpp           Main program: boot, fetch cycle, sleep/wake logic
src/Config.h           config.json loader (LittleFS + ArduinoJson) with defaults
src/Display.h          Display abstraction, I²C LCD driver, serial fallback
src/Storage.h          Non-volatile cache of the last fetched value (NVS)
data/config.json       Runtime configuration uploaded to the device filesystem
config.example.json    Annotated example configuration
platformio.ini         Build configuration and library dependencies
diagram.json           Wokwi circuit definition
wokwi.toml             Wokwi simulator configuration
docs/                  Installation, configuration, and Wokwi simulation guides
```

## Quick start (PlatformIO)

```bash
# Build the firmware
pio run

# Flash the firmware
pio run -t upload

# Upload data/config.json to the device's LittleFS filesystem
pio run -t uploadfs

# Watch the serial output
pio device monitor
```

Edit `data/config.json` before running `uploadfs` to point the device at your own
network and data URL:

```json
{
  "wifi": { "ssid": "YourNetwork", "password": "your-password" },
  "data": { "url": "https://example.com/data.txt" }
}
```

See [docs/installation.md](docs/installation.md) for the Arduino IDE workflow and
[docs/configuration.md](docs/configuration.md) for every available option.

## Try it in the browser-free simulator (no hardware)

You can run the firmware in a simulated circuit inside VS Code with the Wokwi
extension — build with PlatformIO, then start the simulator and click the button.
Follow the [Wokwi VS Code quickstart](docs/wokwi-vscode.md).

## How it works

A button or sensor trigger is treated as user-initiated: the firmware fetches only
when that action fires, never on its own. A cold power-on leaves the display off and
waits for the action instead of fetching. When the action fires — or when the trigger
is a timer — it runs a single fetch cycle:

1. Light the display and connect to WiFi (progress shown on the display).
2. Fetch the data file and sanitize it to fit the display.
3. On success, store the value and show it; on a failure or non-OK HTTP response,
   show the cached value.
4. Keep the result on screen for `display.displayTimeMs`, then turn the display off
   and idle dark until the next action.

By default (`power.shutdown.method = "none"`) the firmware stays awake and polls the
button, so a press reliably triggers a fetch on any setup — including the Wokwi
simulator. Setting the method to `"deepSleep"` switches to the battery-saving cycle:
after the display window the device enters deep sleep and a button press wakes it
(via the RTC `ext0` source) to fetch again. `"lightSleep"` is a middle ground that
naps between fetches without a full power-down. In every mode the display is dark
except while showing a fresh result.
