#!/bin/bash
set -euo pipefail

# Builds Debrief for 64-bit Windows from Linux and packs the handoff zip:
# the app folder, the installer scripts, and the documentation.
#
#   packaging/windows/build.sh [toolchain-folder]
#
# The toolchain folder (default ../debrief-windows-toolchain) holds what
# docs/Building.md says to download once:
#   llvm-mingw/   the cross compiler
#   qt/           Qt for Windows, the same version as this machine's Qt
#   runtime/      libc++.dll and libunwind.dll, from Qt's own download
#   whisper.cpp/  optional checkout; fetched by CMake when absent
#   models/       ggml-base.en.bin

root="$(cd "$(dirname "$0")/../.." && pwd)"
toolchain="$(cd "${1:-$root/../debrief-windows-toolchain}" && pwd)"
version="$(sed -n 's/.*setApplicationVersion("\(.*\)").*/\1/p' "$root/src/main.cpp" | head -1)"
build="$root/build-windows"
name="Debrief-$version-windows-x64"
stage="$build/$name"

for needed in llvm-mingw/bin/x86_64-w64-mingw32-clang++ qt/bin/Qt6Core.dll runtime/libc++.dll models/ggml-base.en.bin; do
  [[ -e "$toolchain/$needed" ]] || { echo "Missing $toolchain/$needed (see docs/Building.md)" >&2; exit 1; }
done

whisper=()
[[ -d "$toolchain/whisper.cpp" ]] && whisper=(-DFETCHCONTENT_SOURCE_DIR_WHISPER="$toolchain/whisper.cpp")

cmake -S "$root" -B "$build" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$root/packaging/windows/toolchain.cmake" \
  -DLLVM_MINGW="$toolchain/llvm-mingw" \
  -DCMAKE_PREFIX_PATH="$toolchain/qt" \
  -DQT_HOST_PATH=/usr -DQT_HOST_PATH_CMAKE_DIR=/usr/lib/cmake \
  "${whisper[@]}" >/dev/null
cmake --build "$build" -j"$(nproc)"

rm -rf "$stage" "$build/$name.zip"
mkdir -p "$stage"
python3 "$root/packaging/windows/package.py" \
  --exe "$build/Debrief.exe" --qt "$toolchain/qt" --runtime "$toolchain/runtime" \
  --objdump "$toolchain/llvm-mingw/bin/llvm-objdump" --importscanner /usr/lib/qt6/qmlimportscanner \
  --qml "$root/src" --out "$stage/app"

mkdir -p "$stage/app/models" "$stage/licenses"
cp "$toolchain/models/ggml-base.en.bin" "$stage/app/models/"
handoff="$root/packaging/windows/handoff"
cp "$handoff/Install.cmd" "$handoff/Install.ps1" "$handoff/README.txt" "$stage/"
cp "$handoff/Uninstall.cmd" "$handoff/Uninstall.ps1" "$stage/app/"
cp -r "$handoff/docs" "$stage/"
cp "$root/packaging/windows/licenses/"* "$stage/licenses/"
cp "$root/LICENSE" "$stage/licenses/Debrief-LICENSE.txt"

# Windows line endings for what Windows reads as text or runs as a script.
find "$stage" -maxdepth 2 \( -name '*.cmd' -o -name '*.ps1' -o -name '*.txt' -o -name '*.md' \) -not -path '*/qml/*' \
  -exec sed -i 's/\r$//; s/$/\r/' {} +

(cd "$build" && zip -qr -9 "$name.zip" "$name")
(cd "$build" && sha256sum "$name.zip" > "$name.zip.sha256")
ls -lh "$build/$name.zip"
