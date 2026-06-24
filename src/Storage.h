// Storage.h
// Non-volatile persistence in the ESP32 NVS (Preferences) flash region, so the
// values survive deep sleep, resets, full power loss, and -- importantly -- an
// OTA firmware update (which rewrites the app partition but leaves NVS intact).
//
// The device keeps three things here:
//   - the last successfully fetched value (one slot, overwritten each time), for
//     the offline fallback;
//   - the version and install time of the last proxy-mode OTA update;
//   - the WiFi credentials, provisioned once from config.json on the first USB
//     flash. Storing them in NVS lets distributed/OTA binaries carry no secrets:
//     the credentials live only on the device, not in the firmware image.
#pragma once

#include <Arduino.h>
#include <Preferences.h>

namespace storage {

constexpr char kNamespace[] = "fetchdata";
constexpr char kLastDataKey[] = "lastData";
constexpr char kOtaVersionKey[] = "otaVersion";
constexpr char kLastInstallKey[] = "lastInstall";
constexpr char kWifiSsidKey[] = "wifiSsid";
constexpr char kWifiPassKey[] = "wifiPass";

// Returns the stored value, or an empty string if nothing has been saved yet.
inline String loadLastData() {
  Preferences prefs;
  if (!prefs.begin(kNamespace, /*readOnly=*/true)) {
    return String();
  }
  const String value = prefs.getString(kLastDataKey, "");
  prefs.end();
  return value;
}

// Persists the most recent successful fetch. Skips empty/sentinel values so a
// failed fetch never overwrites a good cached reading.
inline void saveLastData(const String& value) {
  if (value.isEmpty() || value.startsWith("<")) {
    return;
  }
  Preferences prefs;
  if (!prefs.begin(kNamespace, /*readOnly=*/false)) {
    return;
  }
  prefs.putString(kLastDataKey, value);
  prefs.end();
}

// Returns the version string of the firmware installed by the last proxy-mode
// OTA update, or an empty string if none has ever been applied.
inline String loadOtaVersion() {
  Preferences prefs;
  if (!prefs.begin(kNamespace, /*readOnly=*/true)) {
    return String();
  }
  const String value = prefs.getString(kOtaVersionKey, "");
  prefs.end();
  return value;
}

// Records the version just flashed by a proxy-mode update so the device does
// not reinstall the same build on the next check.
inline void saveOtaVersion(const String& version) {
  if (version.isEmpty()) {
    return;
  }
  Preferences prefs;
  if (!prefs.begin(kNamespace, /*readOnly=*/false)) {
    return;
  }
  prefs.putString(kOtaVersionKey, version);
  prefs.end();
}

// Returns when the last proxy-mode update was installed (an ISO-8601 timestamp,
// or the build's version string if the clock was unavailable), or empty if no
// update has ever been applied.
inline String loadLastInstall() {
  Preferences prefs;
  if (!prefs.begin(kNamespace, /*readOnly=*/true)) {
    return String();
  }
  const String value = prefs.getString(kLastInstallKey, "");
  prefs.end();
  return value;
}

// Records when a proxy-mode update finished installing.
inline void saveLastInstall(const String& timestamp) {
  if (timestamp.isEmpty()) {
    return;
  }
  Preferences prefs;
  if (!prefs.begin(kNamespace, /*readOnly=*/false)) {
    return;
  }
  prefs.putString(kLastInstallKey, timestamp);
  prefs.end();
}

// Returns the provisioned WiFi credentials, or empty strings if none are stored.
inline String loadWifiSsid() {
  Preferences prefs;
  if (!prefs.begin(kNamespace, /*readOnly=*/true)) {
    return String();
  }
  const String value = prefs.getString(kWifiSsidKey, "");
  prefs.end();
  return value;
}

inline String loadWifiPassword() {
  Preferences prefs;
  if (!prefs.begin(kNamespace, /*readOnly=*/true)) {
    return String();
  }
  const String value = prefs.getString(kWifiPassKey, "");
  prefs.end();
  return value;
}

// Persists WiFi credentials so later (OTA) firmware can connect without the
// network name/password being baked into the image or shipped in config.json.
// Only writes when the SSID actually changes, to avoid needless flash wear on
// every boot. Empty SSIDs are ignored so a credential-free config never clears
// good provisioned values.
inline void saveWifiCredentials(const String& ssid, const String& password) {
  if (ssid.isEmpty()) {
    return;
  }
  Preferences prefs;
  if (!prefs.begin(kNamespace, /*readOnly=*/false)) {
    return;
  }
  if (prefs.getString(kWifiSsidKey, "") != ssid ||
      prefs.getString(kWifiPassKey, "") != password) {
    prefs.putString(kWifiSsidKey, ssid);
    prefs.putString(kWifiPassKey, password);
  }
  prefs.end();
}

}  // namespace storage
