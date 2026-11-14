# How Debrief for Windows is built

Debrief is one codebase (C++ and Qt Quick, built with CMake). On Omarchy it
leans on tools every Omarchy desktop has; on Windows those are replaced by
code inside the app. The CMake option `DEBRIEF_NATIVE` chooses, and is always
on for Windows:

| Job | Omarchy | Windows (`DEBRIEF_NATIVE`) | Source |
|---|---|---|---|
| Record the microphone | ffmpeg | Qt Multimedia (`QAudioSource`), converted to 16 kHz mono | `src/native/native_services.cpp` |
| Screenshot | grim | `QScreen::grabWindow`, all monitors composed into one PNG | same |
| Transcribe | Superwhisper | whisper.cpp, statically linked, on a worker thread | same |
| Presence while the window is closed | Omarchy bar widget | Notification-area icon (`QSystemTrayIcon`) | `src/native/tray.cpp` |
| "Session saved" notification | notify-send | Notification-area balloon | same |
| Control channel for `debrief start` etc. | Unix socket | Named pipe `debrief-control-<user>` | `src/control.cpp` |
| Open Claude in a terminal | xdg-terminal-exec + bash | A generated `.cmd` run in a new console | `src/backend.cpp` |
| Theme | Omarchy theme, live | Windows light/dark mode, live | `src/theme.cpp` |

Everything else (the session model, sentence segmentation, transcript and
session.json writers, the window) is shared and unchanged.

## What was tested, and where

- The whole test suite passes on Linux in both configurations (the default, and
  `-DDEBRIEF_NATIVE=ON`, which compiles the same service code Windows uses).
- The native configuration's tests cover the audio converter (48 kHz stereo
  float and 8 kHz 8-bit input), the transcript clean-up, a missing model, and
  a real transcription of a speech sample with the shipped `ggml-base.en.bin`.
- The Windows executable is cross-compiled; its import table and those of
  every bundled DLL are checked at packaging time, so nothing it needs is
  missing from the folder.
- **Not tested: running on Windows.** The tray icon, WASAPI recording, GDI
  screen capture, the named pipe, and the installer script have been compiled
  or reviewed but not executed on a Windows machine.

## Rebuilding (from Linux)

The build machine needs CMake, Python 3, zip, and Qt 6 development packages
(base, declarative, multimedia, svg) whose version matches the Windows Qt you
download: Qt's build tools (moc, rcc, the QML tools) run from the host.

Put these in one folder, by default `../debrief-windows-toolchain` beside the
source tree:

1. **`llvm-mingw/`**: the cross compiler, the release Qt's Windows packages were
   built with (20260602 for Qt 6.12.0), for your build machine's architecture:
   <https://github.com/mstorsjo/llvm-mingw/releases>
2. **`qt/`**: Qt for Windows, `win64_llvm_mingw`, modules qtbase, qtdeclarative,
   qtsvg, qtmultimedia, plus `d3dcompiler_47`. Either `aqt install-qt windows
   desktop 6.12.0 win64_llvm_mingw -m qtmultimedia`, or download the `.7z`
   archives from
   <https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/qt6_6120/qt6_6120_llvm_mingw/>
   and extract them into `qt/`.
3. **`runtime/`**: `libc++.dll` and `libunwind.dll`, from
   `llvm-mingw-20260602-ucrt-x86_64-runtime.zip` in the same Qt download folder.
4. **`models/ggml-base.en.bin`**:
   <https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin>
5. Optionally **`whisper.cpp/`**, a checkout of tag `v1.9.5`; without it CMake
   fetches it.

Then:

```bash
packaging/windows/build.sh            # or: packaging/windows/build.sh /path/to/toolchain
```

It cross-compiles `Debrief.exe`, gathers the DLLs and QML modules it needs
(`packaging/windows/package.py`, which does windeployqt's job and fails if any
DLL would be missing), adds the model, installer and docs, and writes
`build-windows/Debrief-<version>-windows-x64.zip`.

## Building on Windows instead

Install Qt 6.12 (LLVM-MinGW kit, with Qt Multimedia) and CMake, then from a Qt
command prompt:

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
windeployqt --qmldir src build\Debrief.exe
```

and copy a `ggml-*.bin` model into `build\models\`. MSVC is not supported as
is: the build passes GCC-style warning flags.

## Notes for whoever changes this

- **AVX2 is required.** The Windows build of whisper.cpp is compiled with
  SSE4.2/AVX/AVX2/FMA/F16C/BMI2 on (`CMakeLists.txt`); every processor Windows
  11 supports has them. Windows on ARM runs the x64 build under emulation, which needs Windows 11 24H2 or later for AVX2.
- **Not code-signed**, so SmartScreen warns on first run. Signing the exe (and
  wrapping the folder in an MSIX or Inno Setup installer) is the next step if
  this goes to more people.
- **Log**: on Windows, Qt's warnings go to `%LOCALAPPDATA%\debrief\debrief.log`
  (`src/main.cpp`).
- **Strings** added for Windows are in `src/i18n/en.json` and `da.json` like
  the rest; the test suite checks the catalogs against the sources.
