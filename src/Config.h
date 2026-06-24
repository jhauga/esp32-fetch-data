// Config.h
// Runtime configuration for esp32-fetch-data.
//
// Every setting has a compiled-in default that matches the known-good
// proof-of-concept behavior. A `config.json` stored on the LittleFS
// filesystem may override any subset of these defaults. If the file is
// missing or malformed, the firmware silently falls back to the defaults,
// so the program always runs with a working configuration.
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

// Compile-time display defaults. scripts/configure_display.py injects these
// from data/config.json so the firmware selects the configured display even
// where config.json is not loaded (the Wokwi simulator). When the script does
// not run, these fall back to the reference lcd_i2c circuit.
#ifndef DISPLAY_TYPE_DEFAULT
#define DISPLAY_TYPE_DEFAULT "lcd_i2c"
#endif
#ifndef DISPLAY_ADDR_DEFAULT
#define DISPLAY_ADDR_DEFAULT 0x27
#endif
#ifndef DISPLAY_COLS_DEFAULT
#define DISPLAY_COLS_DEFAULT 16
#endif
#ifndef DISPLAY_ROWS_DEFAULT
#define DISPLAY_ROWS_DEFAULT 2
#endif
#ifndef DISPLAY_SDA_DEFAULT
#define DISPLAY_SDA_DEFAULT 21
#endif
#ifndef DISPLAY_SCL_DEFAULT
#define DISPLAY_SCL_DEFAULT 22
#endif

// Action that triggers a data fetch.
enum class ActionType { Button, Sensor, Timer, None };

// Method used to power down between fetch cycles.
enum class ShutdownMethod { DeepSleep, LightSleep, None };

// How a built-with-OTA device looks for an update.
//   Window   - passively listen for a pushed update for a short window after a
//              fetch, then sleep (push with espota / scripts/update-esp32.sh).
//              Battery-friendly.
//   Proxy    - actively pull after a fetch: check a manifest URL for a newer
//              build and, if one exists, download and flash it. Battery-friendly.
//   Periodic - actively pull on a recurring timer, independent of fetches. For
//              steady (wall) power: the firmware refreshes itself so the device
//              always reflects the latest source. Needs the always-on (`none`)
//              power mode; in a sleep mode it degrades to per-fetch Proxy checks.
enum class OtaMode { Window, Proxy, Periodic };

struct AppConfig {
  // --- WiFi ---------------------------------------------------------------
  String wifiSsid = "Wokwi-GUEST";
  String wifiPassword = "";

  // --- Data source --------------------------------------------------------
  String dataUrl =
      "https://raw.githubusercontent.com/jhauga/support-repo/"
      "refs/heads/esp32-fetch-text/file.txt";
  uint32_t httpTimeoutMs = 10000UL;

  // --- Display ------------------------------------------------------------
  // `displayType` selects the driver. "lcd_i2c" is built in. The optional
  // drivers in Display.h ("oled_ssd1306", "oled_sh1107", "lcd",
  // "tft_ili9341", "tft_ili9341_touch", "matrix_max7219") compile only when
  // their build flag is set. Any unrecognized value routes through the
  // external handler hook / serial fallback so the firmware always boots.
  String displayType = DISPLAY_TYPE_DEFAULT;
  String externalDisplayHandler = "";  // optional hook id for custom displays
  uint8_t lcdAddress = DISPLAY_ADDR_DEFAULT;
  uint8_t lcdColumns = DISPLAY_COLS_DEFAULT;
  uint8_t lcdRows = DISPLAY_ROWS_DEFAULT;
  int i2cSda = DISPLAY_SDA_DEFAULT;
  int i2cScl = DISPLAY_SCL_DEFAULT;
  bool preserveNewlines = false;  // some displays can render embedded newlines
  // How long fetched data stays on screen before the display turns off and the
  // device idles dark until the next action.
  uint32_t displayTimeMs = 10000UL;

  // Optional-driver wiring. These are consumed only by the #ifdef-gated drivers
  // in Display.h; the default lcd_i2c build ignores them entirely.
  // Parallel character LCD (LiquidCrystal): register-select, enable and the
  // four 4-bit data pins.
  int lcdRsPin = 19;
  int lcdEnPin = 23;
  int lcdD4Pin = 18;
  int lcdD5Pin = 17;
  int lcdD6Pin = 16;
  int lcdD7Pin = 15;
  // SPI display control pins: ILI9341 TFT chip-select/data-command/reset and
  // the MAX7219 matrix chip-select. Clock and data ride the board's hardware
  // SPI bus (VSPI: SCK 18, MOSI 23).
  int spiCsPin = 5;
  int spiDcPin = 2;
  int spiRstPin = 4;
  int tftRotation = 1;    // ILI9341 orientation, 0-3 (landscape by default)
  int matrixDevices = 4;  // number of chained 8x8 MAX7219 modules

  // --- Action / trigger ---------------------------------------------------
  ActionType actionType = ActionType::Button;
  int buttonPin = 33;  // RTC-capable GPIO so it can wake from deep sleep
  bool buttonActiveLow = true;
  int sensorPin = 27;             // RTC-capable GPIO for presence sensor
  uint32_t sensorHoldMs = 1500UL;  // dwell time before a sensor hit counts
  uint32_t timerIntervalMs = 900000UL;  // 15 min refresh when action == timer
  bool refreshOnAction = true;

  // --- Power management ---------------------------------------------------
  // Default to the always-on polling mode: the button is read directly in the
  // loop, so a press reliably triggers a fetch everywhere (including the Wokwi
  // simulator, which runs on these defaults). Deep sleep is the battery-saving
  // mode for real hardware -- enable it in config.json with
  // power.shutdown.method = "deepSleep" (button wake via ext0 works on silicon).
  ShutdownMethod shutdownMethod = ShutdownMethod::None;

  // --- Storage / offline fallback ----------------------------------------
  uint32_t connectionErrorMs = 5000UL;  // "Connection Error" notice duration

  // --- OTA updates --------------------------------------------------------
  // Used only when the firmware is built with uploadMethod = "ota"
  // (ARDUINO_OTA). After each fetch the device opens a brief update window so it
  // can keep deep-sleeping between updates instead of staying awake to listen.
  OtaMode otaMode = OtaMode::Window;
  uint32_t otaWindowMs = 60000UL;  // window-mode listen duration after a fetch
  String otaProxyUrl = "";         // proxy/periodic manifest URL (newer-build check)
  String otaPassword = "";         // optional auth for the listen window
  uint32_t otaRefreshMs = 3600000UL;  // periodic-mode poll interval (steady power)
};

namespace config_detail {

inline ActionType parseAction(const String& value) {
  String v = value;
  v.toLowerCase();
  if (v == "sensor") return ActionType::Sensor;
  if (v == "timer") return ActionType::Timer;
  if (v == "none") return ActionType::None;
  return ActionType::Button;
}

inline ShutdownMethod parseShutdown(const String& value) {
  String v = value;
  v.toLowerCase();
  if (v == "lightsleep" || v == "light_sleep") return ShutdownMethod::LightSleep;
  if (v == "none") return ShutdownMethod::None;
  return ShutdownMethod::DeepSleep;
}

inline OtaMode parseOtaMode(const String& value) {
  String v = value;
  v.toLowerCase();
  if (v == "proxy") return OtaMode::Proxy;
  if (v == "periodic") return OtaMode::Periodic;
  return OtaMode::Window;
}

// Accept both numeric (39) and hex-string ("0x27") I2C addresses.
inline uint8_t parseAddress(JsonVariantConst value, uint8_t fallback) {
  if (value.is<const char*>()) {
    return static_cast<uint8_t>(strtoul(value.as<const char*>(), nullptr, 0));
  }
  if (value.is<int>()) {
    return static_cast<uint8_t>(value.as<int>());
  }
  return fallback;
}

}  // namespace config_detail

// Loads configuration from LittleFS, overriding defaults where keys exist.
// Returns true if a config file was read, false if defaults were kept.
inline bool loadConfig(AppConfig& cfg, const char* path = "/config.json") {
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed; using built-in defaults.");
    return false;
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.println("config.json not found; using built-in defaults.");
    return false;
  }

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.print("config.json parse error: ");
    Serial.println(error.c_str());
    Serial.println("Using built-in defaults.");
    return false;
  }

  // WiFi
  JsonObjectConst wifi = doc["wifi"];
  cfg.wifiSsid = wifi["ssid"] | cfg.wifiSsid;
  cfg.wifiPassword = wifi["password"] | cfg.wifiPassword;

  // Data source
  JsonObjectConst data = doc["data"];
  cfg.dataUrl = data["url"] | cfg.dataUrl;
  cfg.httpTimeoutMs = data["httpTimeoutMs"] | cfg.httpTimeoutMs;

  // Display
  JsonObjectConst display = doc["display"];
  cfg.displayType = display["type"] | cfg.displayType;
  cfg.externalDisplayHandler =
      display["externalHandler"] | cfg.externalDisplayHandler;
  cfg.lcdAddress = config_detail::parseAddress(display["i2cAddress"], cfg.lcdAddress);
  cfg.lcdColumns = display["columns"] | cfg.lcdColumns;
  cfg.lcdRows = display["rows"] | cfg.lcdRows;
  cfg.i2cSda = display["i2c"]["sda"] | cfg.i2cSda;
  cfg.i2cScl = display["i2c"]["scl"] | cfg.i2cScl;
  cfg.preserveNewlines = display["preserveNewlines"] | cfg.preserveNewlines;
  cfg.displayTimeMs = display["displayTimeMs"] | cfg.displayTimeMs;
  // Optional-driver wiring (parallel LCD pins, SPI control pins, matrix size).
  JsonObjectConst parallel = display["parallel"];
  cfg.lcdRsPin = parallel["rs"] | cfg.lcdRsPin;
  cfg.lcdEnPin = parallel["en"] | cfg.lcdEnPin;
  cfg.lcdD4Pin = parallel["d4"] | cfg.lcdD4Pin;
  cfg.lcdD5Pin = parallel["d5"] | cfg.lcdD5Pin;
  cfg.lcdD6Pin = parallel["d6"] | cfg.lcdD6Pin;
  cfg.lcdD7Pin = parallel["d7"] | cfg.lcdD7Pin;
  JsonObjectConst spi = display["spi"];
  cfg.spiCsPin = spi["cs"] | cfg.spiCsPin;
  cfg.spiDcPin = spi["dc"] | cfg.spiDcPin;
  cfg.spiRstPin = spi["rst"] | cfg.spiRstPin;
  cfg.tftRotation = display["rotation"] | cfg.tftRotation;
  cfg.matrixDevices = display["matrixDevices"] | cfg.matrixDevices;

  // Action / trigger
  JsonObjectConst action = doc["action"];
  if (!action["type"].isNull()) {
    cfg.actionType = config_detail::parseAction(action["type"].as<String>());
  }
  cfg.buttonPin = action["button"]["pin"] | cfg.buttonPin;
  cfg.buttonActiveLow = action["button"]["activeLow"] | cfg.buttonActiveLow;
  cfg.sensorPin = action["sensor"]["pin"] | cfg.sensorPin;
  cfg.sensorHoldMs = action["sensor"]["holdMs"] | cfg.sensorHoldMs;
  cfg.timerIntervalMs = action["timer"]["intervalMs"] | cfg.timerIntervalMs;
  cfg.refreshOnAction = action["refreshOnAction"] | cfg.refreshOnAction;

  // Power
  JsonObjectConst power = doc["power"];
  if (!power["shutdown"]["method"].isNull()) {
    cfg.shutdownMethod =
        config_detail::parseShutdown(power["shutdown"]["method"].as<String>());
  }

  // Storage
  JsonObjectConst storage = doc["storage"];
  cfg.connectionErrorMs = storage["connectionErrorMs"] | cfg.connectionErrorMs;

  // OTA (consumed only by the ARDUINO_OTA build; harmless to parse otherwise).
  JsonObjectConst ota = doc["ota"];
  if (!ota["mode"].isNull()) {
    cfg.otaMode = config_detail::parseOtaMode(ota["mode"].as<String>());
  }
  cfg.otaWindowMs = ota["windowMs"] | cfg.otaWindowMs;
  cfg.otaProxyUrl = ota["proxyUrl"] | cfg.otaProxyUrl;
  cfg.otaPassword = ota["password"] | cfg.otaPassword;
  cfg.otaRefreshMs = ota["refreshMs"] | cfg.otaRefreshMs;

  Serial.println("Loaded configuration from config.json.");
  return true;
}
