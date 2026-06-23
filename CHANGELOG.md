# Changelog

All notable changes to this project are documented in this file. The format is
based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [0.1.0] - 2026-06-22

### Added

- Display drivers for the SSD1306 OLED (`oled_ssd1306`), Grove SH1107 OLED
  (`oled_sh1107`), parallel character LCD (`lcd`, 16x2 / 20x4), ILI9341 TFT
  (`tft_ili9341`), ILI9341 touch screen (`tft_ili9341_touch`), and MAX7219 LED
  dot-matrix (`matrix_max7219`). Each is gated behind a build flag so the default
  firmware still compiles with no extra libraries.
- Config-driven, plug-and-play builds: the pre-build script
  `scripts/configure_display.py` reads the `display` block from
  `data/config.json` and automatically selects the driver build flag, installs
  the matching library, and bakes in the display defaults. Selecting a display is
  now just editing `config.json` and rebuilding, with no `platformio.ini` or
  `Config.h` edits.
- The configured display now also works in the Wokwi simulator, which does not
  load `config.json` at runtime: the script injects the display settings as
  compile-time defaults so the firmware drives the configured display in
  simulation.
- Optional display-wiring keys in `config.json` for the non-I²C drivers:
  `display.parallel` (parallel LCD pins), `display.spi` (SPI cs/dc/rst pins),
  `display.rotation` (ILI9341 orientation), and `display.matrixDevices`
  (chained MAX7219 modules).

### Changed

- The built-in display defaults in `Config.h` are now derived from compile-time
  macros injected by the build script, falling back to the reference `lcd_i2c`
  circuit when the script does not run.

## [0.0.0]

### Changed

- Adopted the standard PlatformIO project layout: program sources moved into
  `src/`, fixing the "Nothing to build" error. `config.json` now lives in the
  standard `data/` folder and is written to LittleFS by `uploadfs`, replacing the
  root-staging build script.
- Button and sensor triggers are now strictly action-gated: a cold power-on leaves
  the display off and fetches only when the action fires, instead of fetching
  automatically on boot.
- The display is now lit only while showing a fetch result. After
  `display.displayTimeMs` it turns off and the device idles dark until the next
  action, in every power mode. (Previously the last result stayed on screen.)
- Consolidated the display-window timing onto the single `display.displayTimeMs`
  key and removed the redundant `power.shutdown.delayMs`.

### Fixed

- Corrected the built-in default data URL, which pointed at a branch that returned
  HTTP 404 in simulation (where `config.json` is not loaded).
- A non-OK HTTP response (e.g. 404) is now treated as a failed fetch and falls
  back to the cached value, instead of displaying the error-page body as data.
- The built-in default shutdown method is now `none` (always-on button polling)
  so a button press reliably triggers a fetch on any setup, including the Wokwi
  simulator, where deep-sleep `ext0` wake did not resume the device. Deep sleep
  remains available as the battery-saving mode via `config.json`.

## [0.0.0-local] - 2026-06-20

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
