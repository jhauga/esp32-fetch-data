# Changelog

All notable changes to this project are documented in this file. The format is
based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [1.0.0] - 2026-06-20

### Added

- Low-power operation: the device fetches data, displays it for a configured
  window, then enters deep sleep and wakes on a button, sensor, or timer.
- Offline cache: the last successful reading is stored in non-volatile memory and
  shown after a brief notice when a fetch fails. The first run with no cached
  value shows `Fetch memory: <none>`.
- File-based configuration via `config.json` on LittleFS, covering WiFi, data
  source, display, trigger action, timing, and shutdown method. All options have
  built-in defaults.
- Display abstraction with a built-in I²C LCD driver and a serial fallback for
  unaccounted display types, plus an external-handler hook.
- PlatformIO build configuration, Wokwi circuit definition, and installation and
  configuration guides.

### Changed

- The fetch-trigger button moved to an RTC-capable GPIO so it can wake the device
  from deep sleep.
