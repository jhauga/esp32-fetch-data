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
  "storage": {
    "connectionErrorMs": 5000
  }
}
```

## Options

### `wifi`

| Key        | Type   | Default       | Description                  |
| ---------- | ------ | ------------- | ---------------------------- |
| `ssid`     | string | `Wokwi-GUEST` | WiFi network name.           |
| `password` | string | `""`          | WiFi password (empty if open). |

### `data`

| Key             | Type   | Default               | Description                        |
| --------------- | ------ | --------------------- | ---------------------------------- |
| `url`           | string | built-in sample URL   | HTTP(S) endpoint returning text.   |
| `httpTimeoutMs` | number | `10000`               | Request timeout in milliseconds.   |

### `display`

| Key               | Type    | Default    | Description                                                            |
| ----------------- | ------- | ---------- | --------------------------------------------------------------------- |
| `type`            | string  | `lcd_i2c`  | Display driver. `lcd_i2c` is built in; other values use the fallback.  |
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

### `storage`

| Key                | Type   | Default | Description                                                  |
| ------------------ | ------ | ------- | ------------------------------------------------------------ |
| `connectionErrorMs`| number | `5000`  | How long the `Connection Error` notice shows before the cache. |

## RTC-capable GPIOs

Deep-sleep wake on a button or sensor uses the `ext0` source, which is limited to
RTC-capable pins. On a standard ESP32 these include GPIO 0, 2, 4, 12–15, 25, 26,
27, 32, 33, 34, 35, 36, and 39. Choose `button.pin` / `sensor.pin` from this set
when using deep sleep.
