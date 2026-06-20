# Installation

The firmware targets an ESP32 with a 16×2 I²C character LCD and a momentary
pushbutton. It can be built with PlatformIO (recommended) or the Arduino IDE.

## Wiring

| Signal      | ESP32 pin | Notes                                              |
| ----------- | --------- | -------------------------------------------------- |
| LCD SDA     | GPIO 21   | I²C data                                           |
| LCD SCL     | GPIO 22   | I²C clock                                           |
| LCD VCC     | 5V        |                                                    |
| LCD GND     | GND       |                                                    |
| Button      | GPIO 33   | One side to the pin, the other to GND (active-low) |

GPIO 33 is RTC-capable, which is required for waking from deep sleep on a button
press. To use a different button pin, pick another RTC-capable GPIO and set it in
`config.json`.

## Option A: PlatformIO

PlatformIO resolves the library dependencies automatically from `platformio.ini`.

```bash
# Build
pio run

# Flash the firmware over USB
pio run -t upload

# Upload config.json to the LittleFS filesystem
pio run -t uploadfs

# Open the serial monitor at 115200 baud
pio device monitor
```

Run `uploadfs` again whenever you change `config.json`. The root `config.json` is
staged into the filesystem image automatically by `extra_script.py`.

## Option B: Arduino IDE

1. Install the ESP32 board support package (Boards Manager → "esp32").
2. Install these libraries through the Library Manager:
   - **ArduinoJson** by Benoit Blanchon
   - **LiquidCrystal_I2C** by Frank de Brabander / Marco Schwartz
3. Open `esp32-fetch-data.ino`. The `.h` files in the same folder are compiled
   automatically.
4. Select your ESP32 board and port, then upload.
5. Upload `config.json` to the device filesystem with the
   [Arduino LittleFS upload plugin](https://github.com/lorol/arduino-esp32fs-plugin).
   This plugin uploads from a `data/` folder, so copy the root `config.json` into
   a `data/` folder next to the sketch before uploading. (PlatformIO stages this
   automatically; the Arduino IDE plugin does not.)

Without a `config.json` on the filesystem, the firmware runs on its built-in
defaults — useful for a first smoke test.

## Simulation (Wokwi)

A Wokwi circuit definition is included in `diagram.json`. After building with
PlatformIO, start the Wokwi simulator from this folder. In simulation the
filesystem is typically empty, so the firmware runs on its built-in defaults.

## Verifying

Open the serial monitor at 115200 baud and trigger a fetch. A healthy run prints
the connection status and the fetched value, then reports the configured sleep
source, for example:

```
Loaded configuration from config.json.
Boot (wake cause: 0).
Connected. IP: 192.168.0.10
Fetched data: Hello from the data file
Deep sleep: wake on button (GPIO 33).
```
