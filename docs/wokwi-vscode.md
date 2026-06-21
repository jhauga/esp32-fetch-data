# Wokwi for VS Code - Quickstart

Run the firmware in a simulated ESP32 circuit, right inside VS Code - no physical
hardware needed. This guide assumes you have never used Wokwi before.

The repo already includes everything the simulator needs:

- `diagram.json` - the virtual circuit (ESP32 + LCD + button)
- `wokwi.toml` - tells Wokwi which compiled firmware to run

You only have to build the firmware once and press play.

## Prerequisites (one-time)

1. **Wokwi Simulator** extension - installed.
2. **PlatformIO IDE** extension - used to compile the firmware. Install it from the
   VS Code Extensions panel (search "PlatformIO IDE") if you do not have it. After
   it installs, let it finish setting up and reload VS Code when prompted.

## Step 1 - Activate your free Wokwi license (one-time)

The extension needs a free license for personal/educational use.

1. Press `F1` (or `Ctrl+Shift+P`) to open the Command Palette.
2. Type **`Wokwi: Request a new License`** and press Enter.
3. Your browser opens wokwi.com. Sign in (or create a free account) and click the
   button to authorize. The page sends you back to VS Code and the license is
   saved automatically.

You only do this once per machine.

## Step 2 - Build the firmware

Wokwi runs the *compiled* firmware, so build it first.

**Easiest way:** click the **checkmark (✓) icon** in the blue PlatformIO toolbar at
the bottom of the VS Code window. This runs a build.

**Or** via the Command Palette: `F1` → **`PlatformIO: Build`**.

**Or** in the terminal:

```bash
pio run
```

Wait for `SUCCESS` in the terminal. This creates the files `wokwi.toml` points to:

```
.pio/build/esp32dev/firmware.bin
.pio/build/esp32dev/firmware.elf
```

> Rebuild (repeat this step) any time you change the `.ino` or `.h` files. Wokwi
> always runs the last build, not your unsaved edits.

## Step 3 - Start the simulator

1. Open `diagram.json` in the editor (optional, but it shows the circuit).
2. Press `F1` → **`Wokwi: Start Simulator`**.

A panel opens showing the ESP32 board, the LCD, and the green button. The LCD stays
**dark** - the firmware does not light it or fetch until the action fires.

## Step 4 - Use it

- On power-up the device does **not** fetch automatically, and the LCD stays off. It
  waits, polling the button.
- **Click the green button** in the diagram to run a fetch. The LCD lights up, the
  device connects to WiFi, fetches the data file, and shows the result. (Wokwi's
  `Wokwi-GUEST` network has real internet access, so the live fetch works in the
  simulation.)
- The result stays on screen for `display.displayTimeMs` (default 10 s), then the LCD
  turns off again and waits. Click the button any time to fetch again.
- The **serial monitor** appears in the simulator panel. It prints the boot, WiFi,
  and fetch messages.

On power-up the serial monitor prints:

```
Boot (wake cause: 0).
Idle: waiting for action to fetch.
```

After you click the button, a healthy fetch prints:

```
Connected. IP: 10.13.37.2
Fetched data: Hello from the data file
```

> **Why no deep sleep in the simulator?** The built-in defaults use the always-on
> polling mode so the button reliably triggers a fetch in Wokwi. Deep sleep (with
> button wake) is the battery-saving mode for real hardware - enable it in
> `config.json` with `power.shutdown.method = "deepSleep"`. See
> [configuration.md](configuration.md).

To stop, press `F1` → **`Wokwi: Stop Simulator`** (or close the panel).

## Notes

- **Configuration in simulation:** the simulator does not load `config.json`, so the
  firmware runs on its built-in defaults (defined in `Config.h`). Those defaults match
  the reference circuit, so it works out of the box. To try different settings in
  simulation, edit the defaults in `Config.h` and rebuild.
- **Button only:** the firmware never fetches on its own. The LCD stays dark until you
  click the button; it lights up to show the result, then turns off after the display
  window (`display.displayTimeMs`).
- **A dark LCD is normal:** between fetches the display is intentionally off to save
  power. Click the button to wake it.
- **Deep sleep:** not used by default (see the note in Step 4). When enabled in
  `config.json` for hardware, the device sleeps after the display window and the
  button press wakes it to fetch again.

## Troubleshooting

| Symptom                                            | Fix                                                                                 |
| -------------------------------------------------- | ----------------------------------------------------------------------------------- |
| "Cannot find firmware" / file not found            | Run **Step 2** (build) first. Confirm `.pio/build/esp32dev/firmware.bin` exists.    |
| `Wokwi: Start Simulator` is missing                | The extension is not installed/enabled, or VS Code needs a reload.                  |
| License / activation prompt keeps appearing        | Re-run **`Wokwi: Request a new License`** and complete the browser step.            |
| LCD is dark                                        | Expected while idle. Click the button to fetch. If it stays dark after a click, rebuild (Step 2) and restart the simulator. |
| No data, only a connection error                   | The data URL may be unreachable; check `data.url` default in `Config.h`.            |
| Changes do not appear                              | Rebuild (Step 2). Wokwi runs the compiled binary, not the open editor.              |
