# Debrief

![Debrief recording a session: the transcript with a screenshot under the sentence that asked for it, and the QA report / SOP switch in the top right corner](docs/screenshot.png)

Talk while you work. Debrief records your voice, transcribes it a sentence at
a time, and takes a screenshot of the whole screen whenever you say
**"screenshot"**. Each screenshot is tied to the words you said around it, so
the session folder reads as a story. One button then hands that folder to
Claude Code, which writes either a **QA report** on what you reviewed or an
**SOP** (a step-by-step procedure) for what you walked through, with crops of
just the part of each screenshot you were talking about.

Recording, transcription, and screenshots all happen on your own machine.
Nothing is sent anywhere until you ask Claude for a report.

## Get it

Debrief is one app with three editions. Each has its own download on the
[latest release](https://github.com/andrzejski/debrief/releases/latest) and its
own guide:

| Edition | Download | Guide |
|---|---|---|
| **Windows 11** | `Debrief-<version>-windows-x64.zip` | [docs/windows.md](docs/windows.md) |
| **macOS** (Apple silicon or Intel) | `Debrief-<version>-macos-arm64.zip` or `-x86_64.zip` | [docs/macos.md](docs/macos.md) |
| **Linux (Omarchy)** | `debrief-<version>-1-<arch>.pkg.tar.zst` | [docs/linux.md](docs/linux.md) |

The Windows and macOS editions have everything built in, including the speech
model. The Linux edition uses what an Omarchy desktop already has: ffmpeg,
grim, Superwhisper, and the Omarchy bar.

Reports need [Claude Code](https://claude.com/claude-code) installed. Without
it everything else still works, and you can drag a session folder into any
Claude conversation yourself.

## A session

1. **Start.** Press Space in the window, or use the notification-area icon
   (Windows), the menu bar icon (macOS), or the bar icon (Linux).
2. **Talk through what you're doing.** Each sentence appears in the window a
   moment after you pause.
3. **Ask for a screenshot.** Say "screenshot" and pause. Any sentence with the
   word in it counts ("take a screenshot of this"), and so do "screencap",
   "capture", and "snap". The screen is captured the moment you stop talking
   and kept only if one of those words was in what you said. S takes one by
   hand.
4. **Stop.** Debrief finishes transcribing the last sentences, then names the
   session after what it was about.
5. **Choose what to write** with the switch in the top right corner of the
   window. Debrief remembers the choice.

   | Switch | For | Writes |
   |---|---|---|
   | **QA report** | Reviewing or debugging something | `Debrief-Report.md`, everything in the order you said it, and `Debrief-Report.html`, the same feedback grouped by the part of the product it is about, with a scorecard of what works and what doesn't |
   | **SOP** | Showing how a task is done | `Debrief-SOP.md` and `Debrief-SOP.html`, a standard operating procedure: numbered steps written as instructions, with your tips and warnings at the step they belong to |

6. **Press B (Report with Claude).** A terminal opens in the session folder
   and starts Claude Code with the prompt for the kind you chose. It reads the
   transcript, looks at every screenshot, cuts crops of just the relevant part
   of each one (kept in `crops/`), and writes the two files. Claude Code first
   asks whether you trust the folder.
7. **Read it.** When both files are written, the session folder opens with the
   page selected (Explorer on Windows, Finder on macOS, the file manager on
   Linux).

On Windows, once you have trusted the folder, the terminal hands focus back to
the window you were in.

## What a session leaves behind

Each session is one folder under `Documents/Debrief/`, with its recording
beside it, so the folder alone can be dragged into Claude:

```text
2026-11-13_10-55-04_save-button-grey/
  transcript.md           time-coded transcript, each screenshot embedded under its words
  session.json            the same for agents: screenshots[].transcript_index, said, context
  shot-01-00-03-12.png    the screenshots, numbered, with their time codes
  Debrief-Report.md       after a QA report: the feedback in the order it was said
  Debrief-Report.html     ...and grouped by product area, with a scorecard
  Debrief-SOP.md          after an SOP: the procedure, step by step
  Debrief-SOP.html        ...and as a page, each step with its crops
  crops/                  the pieces of screenshots the pages show
2026-11-13_10-55-04_save-button-grey.wav    the whole session, 16 kHz mono
```

The name is the time the session started and two or three words for what it
was about. Claude Code picks the words from the transcript when the session
stops (one short question to its smallest model); without Claude Code they are
the words you said most. Until then the folder has only its time.

Sessions are the same format on every edition, so a folder recorded on one
reads the same on the others.

## Keyboard

| Key   | Action                                   |
|-------|------------------------------------------|
| Space | Start or stop a session                  |
| S     | Take a screenshot now                    |
| O     | Open the session folder                  |
| B     | Write a report with Claude (QA or SOP)   |
| ?     | Show or hide the shortcuts               |
| Q     | Quit (asks first while recording)        |

## From the command line

```bash
debrief start | stop | toggle | shot    # drive the running app
debrief status --json                   # what it is doing
debrief prompt                          # the QA report prompt
debrief prompt sop                      # the SOP prompt
debrief skill install                   # the Claude Code skill and its page templates
```

**Report with Claude** installs the skill in `~/.claude/skills/debrief` each
time, so Claude Code lays the report out the same way wherever it runs. On
macOS the command is `/Applications/Debrief.app/Contents/MacOS/Debrief`.

## Building

All three editions build from this one codebase (C++ and Qt Quick, with
CMake). The `DEBRIEF_NATIVE` option chooses between the built-in edition and
the Omarchy one; it is always on for Windows and macOS.

- Linux: [docs/linux.md](docs/linux.md#building)
- Windows: [packaging/windows/handoff/docs/Building.md](packaging/windows/handoff/docs/Building.md)
- macOS: [docs/macos.md](docs/macos.md#building)
- Publishing the downloads: [docs/releasing.md](docs/releasing.md)

## License

MIT; see [LICENSE](LICENSE). The Windows and macOS downloads also carry the
licenses of what they bundle (Qt, whisper.cpp, the Whisper model), in their
`licenses` folder.
