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
press. You can set any valid input pin in `config.json`; a non-RTC pin (such as
GPIO 5) still works, but the firmware falls back to light sleep for it because it
cannot wake from deep sleep. For the lowest power, keep the button on an
RTC-capable GPIO (see
[RTC-capable GPIOs](configuration.md#rtc-capable-gpios-and-pin-selection)).

## Installing PlatformIO Core (`pio`)

PlatformIO Core provides the `pio` command used throughout this guide. It needs
Python 3.6+ and pip. Install for your OS, then confirm with `pio --version`.

### Windows

```powershell
python -m pip install --upgrade platformio
```

If `python` is missing, install it with `winget install Python.Python.3` or from
[python.org](https://www.python.org/downloads/) (check "Add Python to PATH").

### macOS

```bash
python3 -m pip install --upgrade platformio
```

Homebrew users can run `brew install platformio` instead.

### Linux

```bash
python3 -m pip install --upgrade platformio
```

On Debian/Ubuntu, install pip first with `sudo apt install python3-pip`. To flash
over USB without root, add yourself to the serial group and re-login:

```bash
sudo usermod -aG dialout $USER
```

<details>
<summary>Alternative: arduino-cli</summary>

You can build and flash with
[arduino-cli](https://arduino.github.io/arduino-cli/) instead of PlatformIO.

Install the CLI:

- **Windows:** `winget install ArduinoSA.CLI`
- **macOS:** `brew install arduino-cli`
- **Linux:** `curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh`

Add the ESP32 core and the project's libraries:

```bash
arduino-cli config init
arduino-cli config add board_manager.additional_urls \
  https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install "ArduinoJson" "LiquidCrystal_I2C"
```

arduino-cli compiles the sketch form described under **Option B** (an
`esp32-fetch-data.ino` with the `.h` files beside it). The pre-build scripts that
auto-select the display driver and the OTA flag are PlatformIO-only, so pass any
build flag yourself, for example
`--build-property "build.extra_flags=-DARDUINO_OTA"`. PlatformIO remains the
recommended path because it resolves `platformio.ini` and runs those scripts for
you.

</details>

## Option A: PlatformIO

PlatformIO resolves the library dependencies automatically from `platformio.ini`.

```bash
# Build
pio run

# Flash the firmware over USB
pio run -t upload

# Upload data/config.json to the LittleFS filesystem
pio run -t uploadfs

# Open the serial monitor at 115200 baud
pio device monitor
```

Run `uploadfs` again whenever you change `data/config.json`. PlatformIO builds the
filesystem image from the `data/` folder and writes it to the LittleFS partition.

## Option B: Arduino IDE

1. Install the ESP32 board support package (Boards Manager → "esp32").
2. Install these libraries through the Library Manager:
   - **ArduinoJson** by Benoit Blanchon
   - **LiquidCrystal_I2C** by Frank de Brabander / Marco Schwartz
3. The Arduino IDE expects a sketch folder whose name matches the `.ino` file.
   Create a folder named `esp32-fetch-data`, then copy `src/main.cpp` into it
   renamed to `esp32-fetch-data.ino` alongside the three `.h` files from `src/`.
   Open that `.ino`; the `.h` files in the same folder are compiled automatically.
4. Select your ESP32 board and port, then upload.
5. Upload `config.json` to the device filesystem with the
   [Arduino LittleFS upload plugin](https://github.com/lorol/arduino-esp32fs-plugin).
   This plugin uploads from a `data/` folder, so copy `data/config.json` into a
   `data/` folder next to the sketch before uploading.

Without a `config.json` on the filesystem, the firmware runs on its built-in
defaults - useful for a first smoke test.

## Uploading firmware

The `uploadMethod` key in `config.json` selects how new firmware reaches the
device.

### USB (`"uploadMethod": "usb"`)

The default. Connect the board over USB and run:

```bash
pio run -t upload      # flash the firmware
pio run -t uploadfs    # flash config.json to LittleFS
pio device monitor     # watch serial output
```

### Over the air (`"uploadMethod": "ota"`)

This project is designed mainly for battery-powered builds, so OTA stays
battery-friendly: a deep-sleeping device cannot listen continuously, so instead
the firmware offers one update opportunity in the brief active window right after
a fetch (WiFi is already connected), then sleeps again. The `ota.mode` key picks
how that window behaves (`window` or `proxy`).

It also handles a steady (wall) power source: with constant power there is no
battery to conserve, so the `periodic` mode is more convenient - the device
refreshes itself on a timer and the fetched data always reflects the latest
source code, with no one having to push.

> **Status:** the `window` push path is supported and reliable. The pull-based
> modes (`proxy`, `periodic`) and the build pipeline that feeds them are a work in
> progress - usable, but still being finalized.

The first OTA-capable build must always be flashed once over USB so the device
has OTA firmware to start from:

```bash
pio run -t upload      # flash the OTA-enabled firmware once
pio run -t uploadfs    # flash config.json (uploadMethod = "ota")
```

**When to use OTA.** It is most useful for initial setup, for debugging an IoT
device before it is deployed to its final location, and - via proxy mode - for
fleets that need frequent unattended updates.

#### Mode: `window` (push an update)

Listen for a pushed update for `ota.windowMs` after each fetch. Good for hands-on
work: trigger a fetch, then push the build before the window closes.

1. Set `ota.mode` to `window` (and optionally `ota.password`).
2. Note the device's IP from the serial log (`OTA ready: ... (IP 192.x.y.z)`).
   Use the IP, not `esp32-fetch-data.local`: the `.local` name needs an mDNS
   resolver (avahi/Bonjour), which is often missing on Linux and in containers.
3. Trigger a fetch (press the button), which opens the listen window.
4. Within the window, push the new build with one of:

```bash
# Helper script (downloads a release build and pushes it) - simplest
./scripts/update-esp32.sh 192.x.y.z

# espota.py directly (bundled with the ESP32 Arduino core)
python3 "$(find ~/.platformio/packages -name espota.py | head -1)" \
  -i 192.x.y.z -p 3232 -f .pio/build/esp32dev/firmware.bin

# PlatformIO: it auto-switches upload_protocol to espota for an IP/host port
pio run -t upload --upload-port 192.x.y.z
```

> **"No response from device" / "never found device"?** The device only answers
> while its OTA listener is up. In a sleep power mode that is just the
> `ota.windowMs` window after a fetch, so press the button first, then push
> within the window (raise `ota.windowMs` if you need more time). For relaxed
> hands-on updates, set `power.shutdown.method` to `none`: the listener then runs
> continuously and you can push at any time. Always target the device IP, and
> make sure your machine is on the same subnet. The firmware keeps its WiFi radio
> awake while listening so the upload handshake is not missed.

`pio run -t upload --upload-port <ip>` prints a warning suggesting
`upload_protocol = espota` in `platformio.ini`; it is only a hint (PlatformIO
already switches automatically) and is safe to ignore. Do not set that key in the
shared `[env:esp32dev]`, or normal USB uploads would break.

#### Mode: `proxy` (device pulls a newer build)

> **Work in progress.** The pull-based pipeline (this mode and `periodic`, plus
> the build workflow below) is still being finalized. The `window` push path
> above is the supported way to update today.

The device checks a manifest URL after each fetch and updates itself when a newer
build is published, so no one has to be present to push. Set `ota.mode` to
`proxy` and `ota.proxyUrl` to the manifest URL.

The manifest is small JSON describing the latest build:

```json
{
  "version": "2026-01-01T00:00:00Z",
  "url": "https://raw.githubusercontent.com/<owner>/<repo>/main/firmware/firmware.bin"
}
```

- `version` - compared as a string against the last installed version, so use an
  ISO-8601 UTC timestamp (or any value that sorts newest-last). The device
  updates only when the manifest version sorts after the stored one.
- `url` - direct link to the raw `firmware.bin` image.

The included [build workflow](#automated-firmware-build-github-actions-wip)
produces `firmware.bin` and `manifest.json` and commits them under `firmware/`,
so a typical proxy URL points at the committed manifest:

```text
https://raw.githubusercontent.com/<owner>/<repo>/main/firmware/manifest.json
```

> The ESP32 flashes a **raw `firmware.bin`** image and does not unzip archives
> on-device, so the `url` must point at the `.bin` itself, not a `.zip` or an
> Actions artifact (which is a zip and needs authentication). HTTPS hosts such as
> raw.githubusercontent.com work out of the box; the client does not pin a
> certificate.

#### Mode: `periodic` (steady power, always current)

For a device on constant power (a wall adapter), use `periodic`. It pulls from
the same manifest as `proxy`, but on a recurring timer instead of only after a
fetch, so the firmware tracks the latest source automatically and the fetched
data always reflects the current code - the most convenient option when battery
life is not a concern.

1. Set `power.shutdown.method` to `none` (always-on; `periodic` needs the device
   awake to poll).
2. Set `ota.mode` to `periodic`, `ota.proxyUrl` to the manifest URL, and
   `ota.refreshMs` to the poll interval (default 1 hour).

The device checks once at power-up and then every `ota.refreshMs`. The push
listener still runs continuously in this mode, so you can also push a build at
any time. In a sleep power mode `periodic` degrades to a per-fetch `proxy` check,
but steady power is what it is meant for.

### Automated firmware build (GitHub Actions, WIP)

> **Work in progress.** This pipeline is still being finalized; expect rough
> edges. The `window` push path is the reliable update method for now.

`.github/workflows/build-firmware.yml` builds `firmware.bin` and commits it under
[`firmware/`](../firmware/) so a device can pull it from a stable raw URL - no
GitHub Release required, and no zipped Actions artifact to unpack.

The build is switch-gated, so ordinary commits never rebuild or overwrite the
published image. A run builds only when `firmware/on` exists and contains `1`:

```bash
echo 1 > firmware/on
git add firmware/on
git commit -m "build firmware"
git push
```

The run then builds (with the OTA flag forced on), writes `firmware/firmware.bin`,
`firmware/manifest.json`, and `firmware/checksums.txt`, deletes `firmware/on` so
it is a one-shot, and commits the result. `manifest.json` carries the build's
ISO-8601 timestamp as `version` and the raw `firmware.bin` URL as `url`, so a
device whose `ota.proxyUrl` points at the committed `manifest.json` converges on
the new build on its next check.

For a hands-on push instead, run
[`scripts/update-esp32.sh`](../scripts/update-esp32.sh) during the device's OTA
window (it pushes your local build, or the published one with `--published`).

### WiFi credentials and OTA security

Published binaries must not carry network secrets. The firmware keeps credentials
out of the image by provisioning them once, over USB, into the device's NVS flash
(which survives OTA, since an update rewrites only the app partition):

1. **First USB flash:** ship a `config.json` with your real `wifi.ssid` /
   `wifi.password`. On boot the device copies them into NVS.
2. **Later OTA builds:** leave `wifi.ssid` empty in any redistributed
   `config.json`. The device falls back to the credentials stored in NVS, so the
   public `firmware.bin` and config carry no secrets.

The device stores only three things in NVS: the last fetched value (one slot,
overwritten each fetch), the version and time of the last proxy install, and the
WiFi credentials. NVS is not encrypted by default; enable
[NVS encryption](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_encryption.html)
for production hardware if the credentials must be protected at rest.

## Simulation (Wokwi)

You can run the firmware in a simulated circuit with no hardware using the Wokwi
for VS Code extension. A circuit (`diagram.json`) and Wokwi config (`wokwi.toml`)
are included. See the step-by-step [Wokwi VS Code quickstart](wokwi-vscode.md).

In simulation the firmware runs on its built-in defaults (`config.json` is not
loaded), which match the reference circuit.

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
