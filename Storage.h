// Storage.h
// Non-volatile persistence of the last successfully fetched value.
//
// Uses the ESP32 NVS (Preferences) flash region so the value survives deep
// sleep, resets, and full power loss. When a fetch fails, the firmware reads
// this value back and shows it as an offline fallback.
#pragma once

#include <Arduino.h>
#include <Preferences.h>

namespace storage {

constexpr char kNamespace[] = "fetchdata";
constexpr char kLastDataKey[] = "lastData";

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

}  // namespace storage
