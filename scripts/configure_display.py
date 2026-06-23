"""Make the build plug-and-play from data/config.json.

PlatformIO runs this before compiling. It reads the `display` block from
data/config.json and, based on `display.type`:

  1. defines the matching driver build flag (e.g. USE_SSD1306) so the right
     class in src/Display.h is compiled in;
  2. adds that driver's library to lib_deps so it is installed and linked;
  3. injects the display settings (type, address, geometry, I2C pins) as
     compile-time defaults so the firmware selects the configured display even
     in the Wokwi simulator, which never loads config.json.

On real hardware config.json is still read from LittleFS at runtime and
overrides these defaults with the same values, so config.json stays the single
source of truth. Change config.json, rebuild, done.
"""

import json
import os

Import("env")  # noqa: F821  (provided by PlatformIO)

# display.type -> (build-flag macro or None, [library specs])
# "lcd_i2c" is always compiled, so it needs no extra flag or library beyond the
# LiquidCrystal_I2C dependency already in platformio.ini.
DRIVERS = {
    "lcd_i2c": (None, []),
    "lcd": ("USE_LCD_PARALLEL", ["arduino-libraries/LiquidCrystal@^1.0.7"]),
    "oled_ssd1306": (
        "USE_SSD1306",
        ["adafruit/Adafruit SSD1306@^2.5.0", "adafruit/Adafruit GFX Library@^1.11.0"],
    ),
    "oled_sh1107": (
        "USE_SH1107",
        ["adafruit/Adafruit SH110X@^2.1.0", "adafruit/Adafruit GFX Library@^1.11.0"],
    ),
    "tft_ili9341": (
        "USE_ILI9341",
        ["adafruit/Adafruit ILI9341@^1.6.0", "adafruit/Adafruit GFX Library@^1.11.0"],
    ),
    "tft_ili9341_touch": (
        "USE_ILI9341_TOUCH",
        ["adafruit/Adafruit ILI9341@^1.6.0", "adafruit/Adafruit GFX Library@^1.11.0"],
    ),
    "matrix_max7219": (
        "USE_MAX7219",
        ["majicdesigns/MD_MAX72XX@^3.5.0", "majicdesigns/MD_Parola@^3.7.0"],
    ),
}


def load_display_config():
    """Return the display dict from data/config.json, or {} on any problem."""
    path = os.path.join(env["PROJECT_DIR"], "data", "config.json")  # noqa: F821
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return json.load(handle).get("display", {}) or {}
    except (OSError, ValueError) as exc:
        print("configure_display: could not read data/config.json (%s); "
              "using built-in defaults." % exc)
        return {}


def parse_address(value, fallback="0x27"):
    """Accept hex strings ("0x3c") or numbers; return a hex literal string."""
    try:
        if isinstance(value, str):
            return hex(int(value, 0))
        if isinstance(value, (int,)):
            return hex(value)
    except (TypeError, ValueError):
        pass
    return fallback


display = load_display_config()
dtype = str(display.get("type", "lcd_i2c")).strip().lower()

macro, libs = DRIVERS.get(dtype, (None, []))
if dtype not in DRIVERS:
    print("configure_display: unknown display.type '%s'; building with the "
          "lcd_i2c / serial fallback only." % dtype)

# 1) compile the matching driver.
if macro:
    env.Append(CPPDEFINES=[macro])  # noqa: F821

# 2) pull in just the library that driver needs.
if libs:
    project_config = env.GetProjectConfig()  # noqa: F821
    section = "env:" + env["PIOENV"]  # noqa: F821
    existing = project_config.get(section, "lib_deps", [])
    if isinstance(existing, str):
        existing = [existing]
    merged = list(existing) + [lib for lib in libs if lib not in existing]
    project_config.set(section, "lib_deps", merged)

# 3) mirror config.json into compile-time defaults so the simulator (which does
#    not load config.json) selects the configured display.
i2c = display.get("i2c", {}) or {}
defaults = {
    "DISPLAY_TYPE_DEFAULT": env.StringifyMacro(dtype),  # noqa: F821
    "DISPLAY_ADDR_DEFAULT": parse_address(display.get("i2cAddress", "0x27")),
    "DISPLAY_COLS_DEFAULT": int(display.get("columns", 16)),
    "DISPLAY_ROWS_DEFAULT": int(display.get("rows", 2)),
    "DISPLAY_SDA_DEFAULT": int(i2c.get("sda", 21)),
    "DISPLAY_SCL_DEFAULT": int(i2c.get("scl", 22)),
}
env.Append(CPPDEFINES=[(name, value) for name, value in defaults.items()])  # noqa: F821

print("configure_display: display.type='%s'%s" % (
    dtype, "" if not macro else " (-D %s)" % macro))
