# Configuration

All runtime behavior is controlled by `config.json`, stored on the ESP32 LittleFS
filesystem at `/config.json`. Every key is optional - any value left out falls back
to a built-in default, and a missing or malformed file leaves the firmware running
on defaults. Upload the file with `pio run -t uploadfs` after editing the
root `config.json`.

A complete annotated example lives in
[config.example.json](../config.example.json).

## Full schema

```json
{
  "wifi": {
    "ssid": "YourNetwork",
    "password": "your-password"
  },
  "uploadMethod": "usb",
  "data": {
    "url": "https://example.com/data.txt",
    "httpTimeoutMs": 10000
  },
  "display": {
    "type": "lcd_i2c",
    "externalHandler": "",
    "i2cAddress": "0x27",
    "columns": 16,
    "rows": 2,
    "i2c": { "sda": 21, "scl": 22 },
    "preserveNewlines": false,
    "displayTimeMs": 10000
  },
  "action": {
    "type": "button",
    "button": { "pin": 33, "activeLow": true },
    "sensor": { "pin": 27, "holdMs": 1500 },
    "timer": { "intervalMs": 900000 },
    "refreshOnAction": true
  },
  "power": {
    "shutdown": {
      "method": "deepSleep"
    }
  },
  "ota": {
    "mode": "window",
    "windowMs": 60000,
    "proxyUrl": "",
    "password": ""
  },
  "storage": {
    "connectionErrorMs": 5000
  }
}
```

## Options

### `wifi`

| Key        | Type   | Default       | Description                  |
| ---------- | ------ | ------------- | ---------------------------- |
| `ssid`     | string | `Wokwi-GUEST` | WiFi network name. Leave empty to use the credentials provisioned in NVS on the first USB flash (see [WiFi credentials and OTA security](installation.md#wifi-credentials-and-ota-security)). |
| `password` | string | `""`          | WiFi password (empty if open). Saved to NVS alongside the SSID on the first flash. |

### `uploadMethod`

| Value   | Default | Description                                                          |
| ------- | ------- | -------------------------------------------------------------------- |
| `usb`   | yes     | Flash over USB with `pio run -t upload`.                             |
| `ota`   |         | Compile in the over-the-air update service (`ARDUINO_OTA`) so later builds can be pushed over WiFi. The first OTA-capable build must still be flashed once over USB. Use a power mode that stays awake (`none` or `lightSleep`). |

This is a build-time setting: the pre-build script `scripts/configure_upload.py`
reads it and defines the `ARDUINO_OTA` flag when it is `ota`. See
[Uploading firmware](installation.md#uploading-firmware) for the commands.

### `data`

| Key             | Type   | Default               | Description                        |
| --------------- | ------ | --------------------- | ---------------------------------- |
| `url`           | string | built-in sample URL   | HTTP(S) endpoint returning text.   |
| `httpTimeoutMs` | number | `10000`               | Request timeout in milliseconds.   |

### `display`

| Key               | Type    | Default    | Description                                                            |
| ----------------- | ------- | ---------- | --------------------------------------------------------------------- |
| `type`            | string  | `lcd_i2c`  | Display driver. `lcd_i2c` is built in; the optional drivers below are routable only when their build flag is set. Unknown values use the fallback. |
| `externalHandler` | string  | `""`       | Identifier for a custom display handler when `type` is unaccounted.   |
| `i2cAddress`      | string  | `0x27`     | I²C address; accepts hex strings (`"0x27"`) or numbers.               |
| `columns`         | number  | `16`       | Display width in characters.                                          |
| `rows`            | number  | `2`        | Display height in rows.                                               |
| `i2c.sda`         | number  | `21`       | I²C SDA pin.                                                          |
| `i2c.scl`         | number  | `22`       | I²C SCL pin.                                                          |
| `preserveNewlines`| boolean | `false`    | Keep embedded newlines instead of collapsing them to spaces.         |
| `displayTimeMs`   | number  | `10000`    | How long fetched data stays on screen, after which the display turns off until the next action. |

**Unaccounted displays.** If `type` is not a built-in driver, the firmware logs a
warning and switches to a serial fallback so the device keeps running. Set
`externalHandler` to the name of your own handler to integrate a display the
firmware does not natively support.

#### Supported display types

Set `type` in `config.json` and rebuild - that is all. The pre-build script
`scripts/configure_display.py` reads `config.json`, then compiles the matching
driver, installs its library, and applies the display defaults automatically
(plug and play). The build flags below are what the script sets for you; you do
not edit `platformio.ini` or `Config.h` by hand.

| `type`              | Hardware                                   | Build flag          |
| ------------------- | ------------------------------------------ | ------------------- |
| `lcd_i2c`           | 16x2 / 20x4 character LCD over I²C         | (always built)      |
| `lcd`               | 16x2 / 20x4 character LCD, parallel wiring | `USE_LCD_PARALLEL`  |
| `oled_ssd1306`      | SSD1306 OLED over I²C                      | `USE_SSD1306`       |
| `oled_sh1107`       | Grove SH1107 OLED over I²C                 | `USE_SH1107`        |
| `tft_ili9341`       | ILI9341 2.8" TFT-LCD over SPI             | `USE_ILI9341`       |
| `tft_ili9341_touch` | ILI9341 2.8" touch screen (same output)   | `USE_ILI9341_TOUCH` |
| `matrix_max7219`    | MAX7219 LED dot-matrix chain over SPI      | `USE_MAX7219`       |

Both the I²C and parallel LCD entries cover the 16x2 and 20x4 sizes; set
`columns` and `rows` to match the panel. The touch screen renders identically to
`tft_ili9341` (touch input is read by the action subsystem, not the display).

#### Optional driver wiring

These keys are read only by the optional drivers and ignored by `lcd_i2c`.

| Key             | Type   | Default | Description                                         |
| --------------- | ------ | ------- | --------------------------------------------------- |
| `parallel.rs`   | number | `19`    | Parallel LCD register-select pin.                   |
| `parallel.en`   | number | `23`    | Parallel LCD enable pin.                            |
| `parallel.d4`–`d7` | number | `18/17/16/15` | Parallel LCD 4-bit data pins.              |
| `spi.cs`        | number | `5`     | SPI chip-select (ILI9341 TFT / MAX7219 matrix).     |
| `spi.dc`        | number | `2`     | ILI9341 data/command pin.                           |
| `spi.rst`       | number | `4`     | ILI9341 reset pin.                                  |
| `rotation`      | number | `1`     | ILI9341 orientation, `0`–`3`.                       |
| `matrixDevices` | number | `4`     | Number of chained 8x8 MAX7219 modules.              |

SPI displays use the board's hardware SPI bus (VSPI: SCK 18, MOSI 23) for clock
and data.

### `action`

Selects what triggers a fetch.

| Key                  | Type    | Default  | Description                                                       |
| -------------------- | ------- | -------- | ---------------------------------------------------------------- |
| `type`               | string  | `button` | One of `button`, `sensor`, `timer`, `none`.                      |
| `button.pin`         | number  | `33`     | Button GPIO. Must be RTC-capable to wake from deep sleep.        |
| `button.activeLow`   | boolean | `true`   | `true` when the button reads LOW while pressed.                  |
| `sensor.pin`         | number  | `27`     | Presence-sensor GPIO (RTC-capable).                             |
| `sensor.holdMs`      | number  | `1500`   | Dwell time before a sensor reading counts as a trigger.          |
| `timer.intervalMs`   | number  | `900000` | Refresh interval for `timer`/`none` actions (default 15 min).    |
| `refreshOnAction`    | boolean | `true`   | Re-fetch on each trigger rather than re-showing the cached value. |

### `power.shutdown`

| Key       | Type   | Default     | Description                                                          |
| --------- | ------ | ----------- | ------------------------------------------------------------------- |
| `method`  | string | `none`      | `none` (always-on polling loop), `deepSleep`, or `lightSleep`. Deep sleep is the battery-saving mode for hardware; the default `none` polls the button and works everywhere, including the Wokwi simulator. |

### `ota`

Read only by the OTA build (`uploadMethod = "ota"`); ignored otherwise. After
each fetch, while WiFi is still up, the device offers one update opportunity and
then sleeps, so OTA stays compatible with deep sleep. See
[Uploading firmware](installation.md#uploading-firmware) for the workflow.

| Key         | Type   | Default   | Description                                                              |
| ----------- | ------ | --------- | ------------------------------------------------------------------------ |
| `mode`      | string | `window`  | `window` (listen for a pushed update after a fetch), `proxy` (pull a newer build from a manifest after a fetch), or `periodic` (pull on a recurring timer for steady-power devices). |
| `windowMs`  | number | `60000`   | Window-mode listen duration after each fetch, in milliseconds.           |
| `proxyUrl`  | string | `""`      | Manifest URL returning `{ "version": ..., "url": ... }`, used by `proxy` and `periodic`. |
| `password`  | string | `""`      | Optional password the push must supply during the listen window (window mode). |
| `refreshMs` | number | `3600000` | Periodic-mode poll interval, in milliseconds (default 1 hour).           |

The `window` and `proxy` opportunities are what make OTA battery-friendly: they
run only in the brief post-fetch window in the `deepSleep` and `lightSleep`
modes. `periodic` is for steady (wall) power and requires the always-on (`none`)
power mode - it polls the manifest on its own timer so the firmware stays current
without waiting for a fetch (see
[Mode: periodic](installation.md#mode-periodic-steady-power)). In `none` mode the
push listener also runs continuously, so an update can be pushed at any time.

### `storage`

| Key                | Type   | Default | Description                                                  |
| ------------------ | ------ | ------- | ------------------------------------------------------------ |
| `connectionErrorMs`| number | `5000`  | How long the `Connection Error` notice shows before the cache. |

## RTC-capable GPIOs

Deep-sleep wake on a button or sensor uses the `ext0` source, which is limited to
RTC-capable pins. On a standard ESP32 these include GPIO 0, 2, 4, 12–15, 25, 26,
27, 32, 33, 34, 35, 36, and 39. Choose `button.pin` / `sensor.pin` from this set
when using deep sleep.
