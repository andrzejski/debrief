#!/bin/bash
# Builds Debrief.app on a Mac and zips it with its guide and licenses:
#   build-macos/Debrief-<version>-macos-<arch>.zip (+ .sha256)
#
# Needs Xcode's command line tools, CMake, and Qt 6 with Qt Multimedia.
# Qt is found through QT_ROOT_DIR (what the CI's Qt install sets), or
# Homebrew's `qt`. The speech model is downloaded once into build-macos/models
# unless DEBRIEF_MODEL_FILE names one that is already on disk.
#
# The app is signed ad hoc, not with a Developer ID, and not notarized, so
# macOS asks before opening it the first time; README.txt says what to do.
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
build="$root/build-macos"
version="$(sed -n 's/.*setApplicationVersion("\(.*\)").*/\1/p' "$root/src/main.cpp" | head -1)"
arch="$(uname -m)"
name="Debrief-$version-macos-$arch"

qt="${QT_ROOT_DIR:-}"
if [[ -z "$qt" ]]; then
  qt="$(brew --prefix qt 2>/dev/null || true)"
fi
[[ -x "$qt/bin/macdeployqt" ]] || { echo "Qt 6 wasn't found. Set QT_ROOT_DIR, or brew install qt." >&2; exit 1; }

cmake -S "$root" -B "$build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$qt" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-13.0}"
cmake --build "$build" --target debrief -j"$(sysctl -n hw.ncpu)"

app="$build/Debrief.app"
[[ -d "$app" ]] || { echo "The build didn't produce $app" >&2; exit 1; }

# The speech model sits inside the app, where it looks first.
model="${DEBRIEF_MODEL_FILE:-$build/models/ggml-base.en.bin}"
if [[ ! -f "$model" ]]; then
  mkdir -p "$(dirname "$model")"
  curl --fail --location --silent --show-error --output "$model.part" \
    https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin
  mv "$model.part" "$model"
fi
mkdir -p "$app/Contents/Resources/models"
cp "$model" "$app/Contents/Resources/models/"

# Qt's frameworks, plugins, and QML modules go inside the app, then the whole
# of it is signed again, since copying them in breaks the linker's signature.
"$qt/bin/macdeployqt" "$app" -qmldir="$root/src" -verbose=1
# Debrief uses no database, and these plugins want client libraries that
# aren't in the app.
rm -rf "$app/Contents/PlugIns/sqldrivers"
codesign --force --deep --sign - "$app"
codesign --verify --deep --strict "$app"

stage="$build/$name"
rm -rf "$stage" "$build/$name.zip"
mkdir -p "$stage/licenses"
cp -R "$app" "$stage/"
sed "s/@VERSION@/$version/" "$root/packaging/macos/README.txt" > "$stage/README.txt"
cp "$root/LICENSE" "$stage/licenses/Debrief-LICENSE.txt"
cp "$root/packaging/macos/THIRD-PARTY-NOTICES.txt" "$stage/licenses/"
cp "$root/packaging/windows/licenses/Qt-LGPL-3.0.txt" "$root/packaging/windows/licenses/GPL-3.0.txt" \
   "$root/packaging/windows/licenses/whisper.cpp-LICENSE.txt" "$stage/licenses/"

# ditto keeps the app's symlinks and signature, which zip does not.
(cd "$build" && ditto -c -k --keepParent "$name" "$name.zip")
(cd "$build" && shasum -a 256 "$name.zip" > "$name.zip.sha256")
echo "$build/$name.zip"
