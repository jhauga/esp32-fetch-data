#!/usr/bin/env bash
#
# update-esp32.sh - push a firmware build to a device over WiFi.
#
# This is the manual companion to the proxy update mode: instead of letting the
# device pull a new build on its own, you push one during its OTA listen window
# (ota.mode = "window"). It sends the image with espota.py (the ESP32 over-the-air
# uploader bundled with the Arduino core).
#
# By default it pushes your local build (.pio/build/esp32dev/firmware.bin). With
# --published it instead fetches the firmware/firmware.bin that the build
# workflow committed to the repo.
#
# Usage:
#   ./scripts/update-esp32.sh <device-ip> [--published]
#
#   <device-ip>   the device IP from the serial log (e.g. 192.0.2.42); prefer the
#                 IP over the .local name, which needs an mDNS resolver
#   --published   fetch firmware/firmware.bin from the repo instead of the local build
#
# Environment overrides:
#   REPO       owner/repo to pull from   (default: parsed from git remote)
#   BRANCH     branch for --published     (default: main)
#   OTA_PASS   password if ota.password is set in config.json
#   ESPOTA     path to espota.py          (default: auto-detected under PlatformIO)
#
# Trigger the device's OTA window first (press the button), then run this within
# ota.windowMs. Confirm reachability with: ping <device-ip>
set -euo pipefail

DEVICE="${1:-}"
SOURCE="${2:-local}"
if [ -z "${DEVICE}" ]; then
  echo "usage: $0 <device-ip> [--published]" >&2
  exit 1
fi

# Locate espota.py: honor $ESPOTA, else search the PlatformIO framework package.
if [ -z "${ESPOTA:-}" ]; then
  ESPOTA="$(find "${HOME}/.platformio/packages" -name espota.py 2>/dev/null | head -1 || true)"
fi
if [ -z "${ESPOTA:-}" ] || [ ! -f "${ESPOTA}" ]; then
  echo "error: espota.py not found; set ESPOTA=/path/to/espota.py" >&2
  exit 1
fi

workdir="$(mktemp -d)"
trap 'rm -rf "${workdir}"' EXIT

if [ "${SOURCE}" = "--published" ]; then
  # Resolve owner/repo from the git remote unless REPO is set explicitly.
  if [ -z "${REPO:-}" ]; then
    origin="$(git config --get remote.origin.url || true)"
    REPO="$(printf '%s' "${origin}" | sed -E 's#(git@github.com:|https://github.com/)##; s#\.git$##')"
  fi
  if [ -z "${REPO:-}" ]; then
    echo "error: could not determine REPO; set REPO=owner/repo" >&2
    exit 1
  fi
  BIN_URL="https://raw.githubusercontent.com/${REPO}/${BRANCH:-main}/firmware/firmware.bin"
  IMAGE="${workdir}/firmware.bin"
  echo "Fetching published firmware from ${REPO}..."
  curl -fL "${BIN_URL}" -o "${IMAGE}"
else
  IMAGE=".pio/build/esp32dev/firmware.bin"
  if [ ! -f "${IMAGE}" ]; then
    echo "error: ${IMAGE} not found; run 'pio run' first or pass --published" >&2
    exit 1
  fi
fi

echo "Pushing to ${DEVICE} (OTA window must be open)..."
python3 "${ESPOTA}" \
  --ip "${DEVICE}" \
  --port 3232 \
  ${OTA_PASS:+--auth "${OTA_PASS}"} \
  --file "${IMAGE}"

echo "Update sent to ${DEVICE}."
