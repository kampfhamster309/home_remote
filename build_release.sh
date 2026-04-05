#!/usr/bin/env bash
# Build firmware and produce a versioned .bin ready for nano_backbone upload.
# Usage: ./build_release.sh
#
# Output: home_remote_v<VERSION>.bin in the project root.
# Version is read from the FIRMWARE_VERSION build flag in platformio.ini.

set -euo pipefail

cd "$(dirname "$0")"

# Extract version from:  -DFIRMWARE_VERSION='"1.2.3"'
VERSION=$(grep -oP "FIRMWARE_VERSION='\"\\K[^\"]+" platformio.ini)
if [[ -z "${VERSION}" ]]; then
    echo "ERROR: could not parse FIRMWARE_VERSION from platformio.ini" >&2
    exit 1
fi

DEST="home_remote_v${VERSION}.bin"
SRC=".pio/build/cyd/firmware.bin"

echo "Building home_remote v${VERSION}..."
pio run -e cyd

cp "${SRC}" "${DEST}"

SIZE=$(du -h "${DEST}" | cut -f1)
echo ""
echo "  ${DEST}  (${SIZE})"
echo ""
echo "Upload this file to nano_backbone as device type: esp32_2432s028"
