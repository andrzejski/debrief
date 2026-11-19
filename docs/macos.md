# Debrief for macOS

The macOS edition has nothing else to install: the microphone and the screen
go through Qt, whisper.cpp transcribes inside the app with a model file that
ships in it, and the app sits in the menu bar. It is the same built-in edition
as [Windows](windows.md); for Linux, see [linux.md](linux.md).

> The macOS edition is new. It is built and tested on GitHub's Mac machines
> (Apple silicon and Intel), but the parts that need a person at a Mac, the
> microphone and screen permissions, Terminal, and Finder, have had little use
> yet. If something doesn't behave as this page says, that is worth an issue.

## Install

1. Download the zip for your Mac from the
   [latest release](https://github.com/andrzejski/debrief/releases/latest):
   `Debrief-<version>-macos-arm64.zip` for Apple silicon (M1 and later), or
   `Debrief-<version>-macos-x86_64.zip` for an Intel Mac.
2. Double-click the zip, then drag `Debrief.app` into Applications.
3. The first time, right-click (or Control-click) Debrief and choose **Open**,
   then **Open** again. Debrief isn't signed with an Apple developer
   certificate, so macOS asks before running it.
   - On macOS 15 and later, the question is under System Settings > Privacy &
     Security: scroll down and click **Open Anyway**.
   - If macOS says the app is damaged, run
     `xattr -dr com.apple.quarantine /Applications/Debrief.app` in Terminal and
     open it again.

To remove it: quit Debrief from its menu bar icon and drag `Debrief.app` to the
Trash. Your sessions in `~/Documents/Debrief` stay.

## The two permissions

- **Microphone.** macOS asks when you start your first session. If you said no,
  turn Debrief on under System Settings > Privacy & Security > Microphone.
- **Screen Recording.** macOS asks when the first screenshot is taken. Turn
  Debrief on under System Settings > Privacy & Security > Screen & System Audio
  Recording, then quit and reopen Debrief. Until then screenshots show only
  the desktop picture and Debrief's own window.

## Using it

1. Open Debrief and click **Start session** (or press Space).
2. Say a sentence and pause; it appears in the window within a second or two.
3. Say "take a screenshot" and pause; a screenshot appears under your words.
   "Screencap", "capture", and "snap" work too, and S takes one by hand.
4. Click **Stop session**. Debrief names the session and tells you where it is.
5. Choose **QA report** or **SOP** with the switch in the top right corner, then
   press B (**Report with Claude**). Terminal opens in the session folder and
   starts [Claude Code](https://claude.com/claude-code). When both files are
   written, Finder opens on the session folder with the page selected.

Closing the window doesn't stop Debrief. It stays in the menu bar, with a red
dot while recording. Its menu starts or stops a session, takes a screenshot,
opens the folders, or quits; clicking the Dock icon brings the window back.

Differences from Windows, for now: the terminal doesn't hand focus back after
you trust the folder, and there is no start-at-login question (add Debrief
under System Settings > General > Login Items if you want that).

The command line is inside the app:

```bash
/Applications/Debrief.app/Contents/MacOS/Debrief status --json
```

## Building

Needs Xcode's command line tools, CMake, and Qt 6 with Qt Multimedia
(`brew install cmake qt`, or Qt's own installer with `QT_ROOT_DIR` pointing at
it).

```bash
packaging/macos/build.sh
```

It builds `Debrief.app`, puts Qt and the speech model inside it (the model,
about 150 MB, is downloaded once into `build-macos/models`), signs it ad hoc,
and writes `build-macos/Debrief-<version>-macos-<arch>.zip` with its
`.sha256`. The zip is for the kind of Mac it was built on.

To build and run the tests without packaging:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build -j
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

The **Test** workflow does both on every push, for Apple silicon and Intel, and
keeps the two zips as artifacts of the run for two weeks.
