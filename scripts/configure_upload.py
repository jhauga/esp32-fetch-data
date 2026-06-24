"""Make OTA opt-in from data/config.json (plug and play).

PlatformIO runs this before compiling. It reads `uploadMethod` from
data/config.json and, when it is "ota", defines the ARDUINO_OTA build flag so the
OTA service in src/Arduino_ota.h is compiled in. Any other value (or a missing /
malformed file) leaves OTA out and the firmware is flashed over USB as usual.

The very first OTA-capable build must still be flashed over USB so the device has
OTA firmware to listen with; after that, upload over the air with
`pio run -t upload --upload-port <hostname>.local`. Change uploadMethod, rebuild,
done.
"""

import json
import os

Import("env")  # noqa: F821  (provided by PlatformIO)


def load_upload_method():
    """Return uploadMethod from data/config.json, or 'usb' on any problem."""
    path = os.path.join(env["PROJECT_DIR"], "data", "config.json")  # noqa: F821
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return str(json.load(handle).get("uploadMethod", "usb")).strip().lower()
    except (OSError, ValueError) as exc:
        print("configure_upload: could not read data/config.json (%s); "
              "defaulting to USB upload." % exc)
        return "usb"


method = load_upload_method()

# "ota" compiles the OTA service in; any other value builds USB-only.
if method == "ota":
    env.Append(CPPDEFINES=["ARDUINO_OTA"])  # noqa: F821
elif method != "usb":
    print("configure_upload: unknown uploadMethod '%s'; building for USB "
          "upload." % method)

print("configure_upload: uploadMethod='%s'%s" % (
    method, " (-D ARDUINO_OTA)" if method == "ota" else ""))
