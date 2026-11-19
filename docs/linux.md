# Debrief for Linux (Omarchy)

The Linux edition is made for [Omarchy](https://omarchy.org) and leans on what
every Omarchy desktop has: ffmpeg records the microphone, grim takes the
screenshots, Superwhisper transcribes, and a widget in the Omarchy bar shows
whether a session is recording. For Windows, see [windows.md](windows.md).

## Install

Download `debrief-<version>-1-<arch>.pkg.tar.zst` for your processor
(`x86_64` or `aarch64`) from the
[latest release](https://github.com/andrzejski/debrief/releases/latest), then:

```bash
sudo pacman -U debrief-*.pkg.tar.zst
debrief bar install     # optional: the bar widget
```

Transcription runs locally through [Superwhisper](https://superwhisper.com)'s
offline models, so it needs Superwhisper installed. Without it Debrief still
records audio and takes screenshots by hand (S), and says so in the window.

To remove it: `debrief bar remove`, then `sudo pacman -R debrief`. Your
sessions in `~/Documents/Debrief/` stay.

## Using it

1. Start a session: press Space in the window, click the bug in the Omarchy bar,
   or run `debrief start`. The window can stay closed; the bar icon turns red
   while recording.
2. Talk through what you're doing. When something is worth capturing, say
   "screenshot" and pause for a moment.
3. Stop the session from the window, the bar, or `debrief stop`.
4. Press B (**Report with Claude**) to have Claude Code write
   `bug-report.md` and `bug-report.html` in the session folder. The page
   groups the feedback by the part of the product it is about and shows crops
   of the screenshots, which need ImageMagick (`magick`) or Python with
   Pillow to cut.

Closing the window doesn't stop a session; the bar shows it's still recording.
The session folder and the keyboard shortcuts are described in the
[README](../README.md#a-session).

## Bar widget

```bash
debrief bar install     # links the widget into ~/.config/omarchy/plugins and adds it to the bar
debrief bar remove
```

Left click starts or stops a session, right click opens Debrief, and middle
click opens the session folder.

## Command line

```bash
debrief start | stop | toggle    # drive the session; start launches Debrief if needed
debrief shot                     # screenshot now, e.g. from a key binding
debrief status --json
debrief prompt                   # the report prompt the B button uses
debrief skill install            # the Claude Code skill for the session format and the report; B installs it too
```

Commands exit 0 or 1, report errors on stderr, and take `--json`.

## Building

```bash
bin/build                                 # into build/
bin/test                                  # build, then run the tests offscreen
cmake --install build --prefix ~/.local   # try it in the app launcher
```

It needs Qt 6 (base, declarative, svg, wayland), ffmpeg, grim, and Superwhisper
for transcripts. [packaging/arch/PKGBUILD](../packaging/arch/PKGBUILD) is the
full list, and builds the package the release carries:

```bash
cd packaging/arch && makepkg -si
```
