# Debrief

![Debrief recording a debugging session](docs/screenshot.png)

Debug out loud. Debrief records your voice while you work, transcribes it a
sentence at a time, and takes a screenshot of the whole screen whenever you say
**"screenshot"**. Each screenshot is tied to the words you said around it,
so the session folder reads as a story Claude can turn into a report.

## Get it

Debrief is one app with two editions. Each has its own download on the
[latest release](https://github.com/andrzejski/debrief/releases/latest) and its
own guide:

| Edition | Download | Guide |
|---|---|---|
| **Windows 11** | `Debrief-<version>-windows-x64.zip` | [docs/windows.md](docs/windows.md) |
| **Linux (Omarchy)** | `debrief-<version>-1-<arch>.pkg.tar.zst` | [docs/linux.md](docs/linux.md) |

The Windows edition has everything built in. The Linux edition uses what an
Omarchy desktop already has: ffmpeg, grim, Superwhisper, and the Omarchy bar.

## A session

1. Start a session: press Space in the window, or use the bar icon (Linux) or
   the notification-area icon (Windows).
2. Talk through what you're doing. When something is worth capturing, say
   "screenshot" and pause for a moment. Any sentence with the word in it counts,
   and so do "screencap", "capture", and "snap".
   The screen is captured the moment you stop talking and kept only if the
   word was in what you said.
3. Stop the session. Debrief finishes transcribing the last sentences, then
   names the session after what it was about.
4. Press B (**Report with Claude**) to open Claude Code in the session
   folder with a prompt that reads the transcript, looks at every screenshot,
   and writes two reports: `Debrief-Report.md`, everything in the order you said
   it, and `Debrief-Report.html`, the same feedback grouped by the part of the
   product it is about, each entry with crops of just the relevant part of
   the screenshot (kept in `crops/`). Once both are written, the session
   folder opens in the file manager (on Windows, Explorer with
   `Debrief-Report.html` selected). On Windows the terminal also gives focus
   back to the window you were in once you have trusted the folder.

   The switch in the top right corner of the window chooses what is written.
   **QA report** is the report above. **SOP** asks instead for a standard
   operating procedure for what you walked through: `Debrief-SOP.md` and
   `Debrief-SOP.html`, numbered steps with the same crops. The choice is
   remembered.

Each session is one folder under `Documents/Debrief/`, with its recording
beside it, so the folder alone can be dragged into Claude:

```text
2026-11-13_10-55-04_save-button-grey/
  transcript.md           time-coded transcript, each screenshot embedded under its words
  session.json            the same for agents: screenshots[].transcript_index, said, context
  shot-01-00-03-12.png    the screenshots, numbered, with their time codes
2026-11-13_10-55-04_save-button-grey.wav    the whole session, 16 kHz mono
```

The name is the time the session started and two or three words for what it
was about. Claude Code picks the words from the transcript when the session
stops (one short question to its smallest model); without Claude Code they are
the words you said most. Until then the folder has only its time.

Sessions are the same format on both editions, so a folder recorded on one
reads the same on the other.

## Keyboard

| Key   | Action                          |
|-------|---------------------------------|
| Space | Start or stop a session         |
| S     | Take a screenshot now           |
| O     | Open the session folder         |
| B     | Write a report with Claude      |
| ?     | Show or hide the shortcuts      |
| Q     | Quit (asks first while recording) |

## Building

Both editions build from this one codebase (C++ and Qt Quick, with CMake); the
`DEBRIEF_NATIVE` option chooses between them and is always on for Windows.

- Linux: [docs/linux.md](docs/linux.md#building)
- Windows: [packaging/windows/handoff/docs/Building.md](packaging/windows/handoff/docs/Building.md)
- Publishing both downloads: [docs/releasing.md](docs/releasing.md)

## License

MIT; see [LICENSE](LICENSE). The Windows download also carries the licenses of
what it bundles (Qt, whisper.cpp), in its `licenses` folder.
