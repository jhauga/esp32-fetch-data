#!/usr/bin/env bash
#
# update-esp32.sh - push a released firmware build to a device over WiFi.
#
# This is the manual companion to the proxy update mode: instead of letting the
# device pull a new build on its own, you push one during its OTA listen window
# (ota.mode = "window"). It downloads firmware.bin from a GitHub release and
# sends it with espota.py (the ESP32 over-the-air uploader bundled with the
# Arduino core).
#
# Usage:
#   ./scripts/update-esp32.sh <device-host-or-ip> [version]
#
#   <device-host-or-ip>  e.g. esp32-fetch-data.local or 10.0.0.42
#   [version]            release tag (default: latest)
#
# Environment overrides:
#   REPO       owner/repo to pull from   (default: parsed from git remote)
#   OTA_PASS   password if ota.password is set in config.json
#   ESPOTA     path to espota.py         (default: auto-detected under PlatformIO)
#
# Trigger the device's OTA window first (press the button), then run this within
# ota.windowMs. Confirm reachability with: ping <device-host-or-ip>
set -euo pipefail

DEVICE="${1:-}"
VERSION="${2:-latest}"
if [ -z "${DEVICE}" ]; then
  echo "usage: $0 <device-host-or-ip> [version]" >&2
  exit 1
fi

# Resolve owner/repo from the git remote unless REPO is set explicitly.
if [ -z "${REPO:-}" ]; then
  origin="$(git config --get remote.origin.url || true)"
  REPO="$(printf '%s' "${origin}" | sed -E 's#(git@github.com:|https://github.com/)##; s#\.git$##')"
fi
if [ -z "${REPO:-}" ]; then
  echo "error: could not determine REPO; set REPO=owner/repo" >&2
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

# Build the firmware.bin download URL for the requested release.
if [ "${VERSION}" = "latest" ]; then
  BIN_URL="https://github.com/${REPO}/releases/latest/download/firmware.bin"
else
  BIN_URL="https://github.com/${REPO}/releases/download/${VERSION}/firmware.bin"
fi

workdir="$(mktemp -d)"
trap 'rm -rf "${workdir}"' EXIT

echo "Downloading ${VERSION} firmware from ${REPO}..."
curl -fL "${BIN_URL}" -o "${workdir}/firmware.bin"

echo "Pushing to ${DEVICE} (OTA window must be open)..."
python3 "${ESPOTA}" \
  --ip "${DEVICE}" \
  --port 3232 \
  ${OTA_PASS:+--auth "${OTA_PASS}"} \
  --file "${workdir}/firmware.bin"

echo "Update sent to ${DEVICE}."
