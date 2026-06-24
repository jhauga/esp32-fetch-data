// Arduino_ota.h
// Optional, battery-friendly Over-The-Air (OTA) firmware updates.
//
// OTA is opt-in from config.json: `uploadMethod = "ota"` makes the pre-build
// script scripts/configure_upload.py define the ARDUINO_OTA build flag (the same
// plug-and-play pattern configure_display.py uses for displays). When that flag
// is absent every function below compiles to an inline no-op, so the default
// USB-upload build pulls in no OTA code at all.
//
// Battery model: a deep-sleeping device is normally unreachable for updates. To
// stay battery-friendly, OTA is offered only in the brief active window right
// after a fetch (WiFi is already connected), then the device sleeps again. Two
// modes pick what happens in that window (config: `ota.mode`):
//
//   "window" - passively listen for a pushed update for `ota.windowMs`, then
//              sleep. Push with espota (or scripts/update-esp32.sh) by device IP.
//   "proxy"  - actively pull: GET a manifest from `ota.proxyUrl`, and if it
//              advertises a newer build, download and flash it, then reboot.
//              No waiting window, so the device is awake for the least time.
//
// The always-on (`none`) power mode instead keeps the listener running every
// loop via begin()/handle(), so updates are available continuously.
#pragma once

#include <Arduino.h>

#include "Config.h"

#ifdef ARDUINO_OTA
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>

#include "Storage.h"
#endif

namespace ota {

#ifdef ARDUINO_OTA

// Hostname the device advertises for OTA (reachable as <hostname>.local).
constexpr char kHostname[] = "esp32-fetch-data";

namespace detail {

// Brings up the ArduinoOTA listener exactly once. Requires an active WiFi
// connection, so call only after WiFi is up. Safe to call repeatedly.
inline void ensureService(const AppConfig& cfg) {
  static bool started = false;
  if (started) {
    return;
  }
  ArduinoOTA.setHostname(kHostname);
  if (!cfg.otaPassword.isEmpty()) {
    ArduinoOTA.setPassword(cfg.otaPassword.c_str());
  }
  ArduinoOTA.onStart([]() { Serial.println("OTA: update starting."); });
  ArduinoOTA.onEnd([]() { Serial.println("\nOTA: update complete."); });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA: error [%u].\n", static_cast<unsigned>(error));
  });
  // Keep the radio awake while listening. With the default WiFi modem sleep the
  // ESP32 naps between beacons and drops espota's UDP invitation, which shows up
  // on the host as "No response from device" even though the IP is reachable.
  WiFi.setSleep(false);
  ArduinoOTA.begin();
  started = true;
  Serial.printf("OTA ready: %s.local (IP %s).\n", kHostname,
                WiFi.localIP().toString().c_str());
}

// Passive listen window: service pushed updates until the window elapses.
inline void offerWindow(const AppConfig& cfg) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("OTA window: WiFi not connected; skipping.");
    return;
  }
  ensureService(cfg);
  // Re-assert no-sleep each window: reconnecting WiFi after a light/deep sleep
  // can restore the default modem sleep, which would again drop OTA invitations.
  WiFi.setSleep(false);
  Serial.printf("OTA window open for %lu ms.\n",
                static_cast<unsigned long>(cfg.otaWindowMs));
  const unsigned long start = millis();
  while (millis() - start < cfg.otaWindowMs) {
    ArduinoOTA.handle();
    delay(10);  // yield to the WiFi/OTA stack; keep the listener responsive
  }
  Serial.println("OTA window closed.");
}

// Records a completed proxy update: the version (so the same build is not
// reinstalled) and the install time. The time is taken from NTP when reachable;
// if the clock cannot be set, the build's own version string is stored instead.
inline void recordInstall(const String& version) {
  storage::saveOtaVersion(version);

  String stamp = version;  // fallback when NTP is unavailable
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  struct tm timeInfo;
  if (getLocalTime(&timeInfo, 5000)) {
    char iso[25];
    strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M:%SZ", &timeInfo);
    stamp = iso;
  }
  storage::saveLastInstall(stamp);
  Serial.printf("OTA proxy: recorded install %s at %s.\n", version.c_str(),
                stamp.c_str());
}

// Active pull: check the proxy manifest and flash a newer build if offered.
// On a successful update the device reboots and does not return from here.
inline void checkProxy(const AppConfig& cfg) {
  if (cfg.otaProxyUrl.isEmpty()) {
    Serial.println("OTA proxy: no proxyUrl configured; skipping.");
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("OTA proxy: WiFi not connected; skipping.");
    return;
  }

  // Public proxies (e.g. GitHub) are HTTPS. Use an insecure TLS client so no CA
  // bundle has to be shipped; plain HTTP proxies use the basic client.
  const bool secure = cfg.otaProxyUrl.startsWith("https");
  WiFiClientSecure secureClient;
  WiFiClient plainClient;
  if (secure) {
    secureClient.setInsecure();
  }
  WiFiClient* client =
      secure ? static_cast<WiFiClient*>(&secureClient) : &plainClient;

  // 1) Fetch the manifest describing the latest available build.
  HTTPClient http;
  http.setTimeout(cfg.httpTimeoutMs);
  if (!http.begin(*client, cfg.otaProxyUrl)) {
    Serial.println("OTA proxy: manifest request setup failed.");
    return;
  }
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("OTA proxy: manifest HTTP %d.\n", code);
    http.end();
    return;
  }
  JsonDocument doc;
  const DeserializationError parseError = deserializeJson(doc, http.getString());
  http.end();
  if (parseError) {
    Serial.printf("OTA proxy: manifest parse error: %s\n", parseError.c_str());
    return;
  }

  const String latest = doc["version"] | "";
  const String binUrl = doc["url"] | "";
  if (latest.isEmpty() || binUrl.isEmpty()) {
    Serial.println("OTA proxy: manifest missing \"version\" or \"url\".");
    return;
  }

  // 2) Update only when nothing is recorded yet or the proxy build is newer.
  // Versions are compared as strings, so ISO-8601 UTC timestamps (e.g.
  // 2026-06-24T15:00:00Z) sort chronologically and an older proxy is ignored.
  const String installed = storage::loadOtaVersion();
  const bool newer = installed.isEmpty() || latest > installed;
  if (!newer) {
    Serial.printf("OTA proxy: up to date (installed %s, proxy %s).\n",
                  installed.c_str(), latest.c_str());
    return;
  }

  // 3) Download and flash the raw firmware image. rebootOnUpdate(false) lets us
  // record the new version before restarting so the device does not loop.
  Serial.printf("OTA proxy: updating %s -> %s\n",
                installed.isEmpty() ? "<none>" : installed.c_str(),
                latest.c_str());
  httpUpdate.rebootOnUpdate(false);
  const t_httpUpdate_return result = httpUpdate.update(*client, binUrl);
  switch (result) {
    case HTTP_UPDATE_OK:
      recordInstall(latest);
      Serial.println("OTA proxy: update applied; rebooting.");
      delay(100);
      ESP.restart();
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("OTA proxy: server reported no update.");
      break;
    case HTTP_UPDATE_FAILED:
    default:
      Serial.printf("OTA proxy: update failed (%d): %s\n",
                    httpUpdate.getLastError(),
                    httpUpdate.getLastErrorString().c_str());
      break;
  }
}

}  // namespace detail

// Starts the continuous OTA listener, for the always-on (`none`) power mode that
// services updates every loop via handle(). Call once after WiFi is up.
inline void begin(const AppConfig& cfg) { detail::ensureService(cfg); }

// Services a pending pushed update. Call frequently from loop() (always-on mode).
inline void handle() { ArduinoOTA.handle(); }

// Battery-mode OTA opportunity to run once after a fetch, before sleeping.
// Returns true if it held the device awake for its own window (window mode), so
// the caller can skip the usual on-screen hold; false otherwise (proxy mode or
// a skipped attempt), letting the caller show the fetched data normally.
inline bool run(const AppConfig& cfg) {
  switch (cfg.otaMode) {
    case OtaMode::Window:
      detail::offerWindow(cfg);
      return true;
    case OtaMode::Proxy:
    case OtaMode::Periodic:
      // Proxy checks per fetch. Periodic normally polls from loop() on steady
      // power, but in a sleep mode (where loop() does not run freely) it falls
      // back to the same per-fetch pull so updates still arrive.
      detail::checkProxy(cfg);  // reboots on success and never returns
      return false;
  }
  return false;
}

// Periodic auto-refresh for steady-power (always-on) deployments. Call every
// loop iteration; runs a proxy check on power-up and then every otaRefreshMs.
// A no-op in any other mode. Keeps the firmware current without a fetch trigger.
inline void poll(const AppConfig& cfg) {
  if (cfg.otaMode != OtaMode::Periodic) {
    return;
  }
  static unsigned long lastCheck = 0;
  static bool primed = false;
  const unsigned long now = millis();
  if (!primed || now - lastCheck >= cfg.otaRefreshMs) {
    primed = true;
    lastCheck = now;
    detail::checkProxy(cfg);  // reboots on success and never returns
  }
}

// True when OTA is compiled in (uploadMethod = "ota").
inline constexpr bool enabled() { return true; }

#else  // ARDUINO_OTA not defined: OTA disabled, everything is an inline no-op.

inline void begin(const AppConfig&) {}
inline void handle() {}
inline bool run(const AppConfig&) { return false; }
inline void poll(const AppConfig&) {}
inline constexpr bool enabled() { return false; }

#endif  // ARDUINO_OTA

}  // namespace ota
