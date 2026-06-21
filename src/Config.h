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

// Action that triggers a data fetch.
enum class ActionType { Button, Sensor, Timer, None };

// Method used to power down between fetch cycles.
enum class ShutdownMethod { DeepSleep, LightSleep, None };

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
  // `displayType` selects the driver. "lcd_i2c" is built in. Any other value
  // routes through the external handler hook so unaccounted displays can be
  // supported without modifying core code.
  String displayType = "lcd_i2c";
  String externalDisplayHandler = "";  // optional hook id for custom displays
  uint8_t lcdAddress = 0x27;
  uint8_t lcdColumns = 16;
  uint8_t lcdRows = 2;
  int i2cSda = 21;
  int i2cScl = 22;
  bool preserveNewlines = false;  // some displays can render embedded newlines
  // How long fetched data stays on screen before the display turns off and the
  // device idles dark until the next action.
  uint32_t displayTimeMs = 10000UL;

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

  Serial.println("Loaded configuration from config.json.");
  return true;
}
