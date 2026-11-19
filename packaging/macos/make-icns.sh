#!/bin/bash
# Makes a macOS icon file from one 1024-pixel PNG, with the tools every Mac has.
#   make-icns.sh icon-1024.png Debrief.icns
set -euo pipefail

png="$1"
icns="$2"
set="$(mktemp -d)/Debrief.iconset"
mkdir -p "$set"
for size in 16 32 128 256 512; do
  sips -z "$size" "$size" "$png" --out "$set/icon_${size}x${size}.png" >/dev/null
  sips -z "$((size * 2))" "$((size * 2))" "$png" --out "$set/icon_${size}x${size}@2x.png" >/dev/null
done
iconutil --convert icns --output "$icns" "$set"
rm -rf "$(dirname "$set")"
