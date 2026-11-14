# Debrief for Windows: user guide

## A session

1. **Start**: press Space in the window, click **Start session**, or right-click
   the notification-area icon and pick **Start session**. The window can stay
   closed; the icon shows a red dot while recording.
2. **Talk** through what you're doing, in sentences, with short pauses. Each
   sentence is transcribed when you pause.
3. **Say "screenshot"** when something is worth capturing, then pause. Any
   sentence with the word in it counts ("take a screenshot of this", "screenshot
   please"). The screen is captured the moment you stop talking and kept only if
   the word was in what you said, so the picture is of what you were looking at
   as you said it. All monitors are captured into one image.
4. **Stop**: Space, **Stop session**, or the icon's menu. Debrief finishes the
   last sentences, then names the session after what it was about.
5. **Bug report**: press B (**Bug report with Claude**). A terminal opens in the
   session folder and starts Claude Code with a prompt that reads the transcript,
   looks at every screenshot, and writes `bug-report.md`. This needs
   [Claude Code](https://claude.com/claude-code) installed; without it,
   everything else still works and you can drag the session folder into any
   Claude conversation yourself.

## What a session is

One folder per session under `Documents\Debrief\`, with its recording beside it:

```text
2026-11-13_10-55-04_save-button-grey\
  transcript.md           time-coded transcript, each screenshot under its words
  session.json            the same for programs: screenshots[].transcript_index, said, context
  shot-01-00-03-12.png    the screenshots, numbered, with their time codes
2026-11-13_10-55-04_save-button-grey.wav    the whole session, 16 kHz mono
```

The name is the time the session started and two or three words for what it was
about. If Claude Code is installed it picks the words (one short question to its
smallest model, sent only the first part of the transcript); otherwise they are
the words you said most. Until a session stops, its folder has only the time.

If your Documents folder is backed up by OneDrive, sessions are too. They are
ordinary files: move, zip, or delete them as you like.

## Keyboard (in the window)

| Key   | Action                              |
|-------|-------------------------------------|
| Space | Start or stop a session             |
| S     | Take a screenshot now               |
| O     | Open the session folder             |
| B     | Write a bug report with Claude      |
| ?     | Show or hide the shortcuts          |
| Q     | Quit (asks first while recording)   |

## Notification area

| Action              | What it does                                         |
|---------------------|------------------------------------------------------|
| Left-click          | Open the window                                      |
| Right-click         | Start/stop session, Screenshot now, Open Debrief, Open session folder, Open all sessions, Quit |
| Click a "session saved" notification | Open that session's folder          |

Closing the window leaves Debrief running here. **Quit** in the menu (or Q in
the window) ends it; if a session is recording it is stopped and saved first.

Windows 11 puts new icons behind the **^** arrow. Drag Debrief's icon onto the
taskbar to keep it visible, or turn it on under Settings > Personalization >
Taskbar > Other system tray icons.

## Starting with Windows

The installer asks. To change it later, run `Install.cmd` again, or add/remove
the shortcut yourself: press Win+R, type `shell:startup`, and put a shortcut to
`Debrief.exe --hidden` there (or delete the one that is there).

## Command line

`Debrief.exe` takes the same commands as on Linux, which is how you bind a
session to a hotkey (PowerToys Keyboard Manager, AutoHotkey, a Stream Deck):

```bat
"%LOCALAPPDATA%\Programs\Debrief\Debrief.exe" toggle    & rem start or stop a session
"%LOCALAPPDATA%\Programs\Debrief\Debrief.exe" start     & rem starts Debrief hidden if it isn't running
"%LOCALAPPDATA%\Programs\Debrief\Debrief.exe" stop
"%LOCALAPPDATA%\Programs\Debrief\Debrief.exe" shot      & rem screenshot now
"%LOCALAPPDATA%\Programs\Debrief\Debrief.exe" status --json
"%LOCALAPPDATA%\Programs\Debrief\Debrief.exe" prompt    & rem the bug-report prompt the B button uses
"%LOCALAPPDATA%\Programs\Debrief\Debrief.exe" skill install   & rem a Claude Code skill describing the session format
```

Commands exit 0 or 1. Debrief is a windowed program, so a console doesn't wait
for it: in a script, use `start /wait "" Debrief.exe status --json` or redirect
the output to a file or pipe to read it.

## Speech recognition

Transcription is done on your machine by [whisper.cpp](https://github.com/ggml-org/whisper.cpp),
built into `Debrief.exe`, with the model file in `models\`. Nothing you say
leaves the computer.

The model that ships, `ggml-base.en.bin` (148 MB), is English-only and quick:
a sentence takes a second or two on a recent laptop, using at most four
processor cores for that moment. To trade speed for accuracy, or to speak
another language, replace it:

1. Download another model from <https://huggingface.co/ggerganov/whisper.cpp/tree/main>:
   - `ggml-small.en.bin` (488 MB): noticeably more accurate English, about 3x slower
   - `ggml-base.bin` / `ggml-small.bin`: any language, detected as you speak
   - `ggml-tiny.en.bin` (78 MB): for a slow machine
2. Put it in `%LOCALAPPDATA%\Programs\Debrief\models\` and remove the old one
   (Debrief uses the first `ggml-*.bin` it finds there, by name).
3. Quit and reopen Debrief.

The model is loaded when a session's first sentence arrives and released two
minutes after the last one, so it costs no memory while Debrief idles.

## Appearance

Debrief follows Windows' light or dark mode (Settings > Personalization >
Colors) and switches with it.

## How this differs from Debrief on Omarchy

| On Omarchy (Linux)                         | On Windows                                        |
|--------------------------------------------|---------------------------------------------------|
| Superwhisper transcribes                   | whisper.cpp, built in                             |
| ffmpeg records, grim takes screenshots     | Built in (Qt)                                     |
| A widget in the Omarchy bar                | An icon in the notification area                  |
| Follows the Omarchy theme                  | Follows Windows light/dark mode                   |
| Quits when its window closes and it's idle | Stays in the notification area until you quit it  |
| `debrief bar install`                      | Not applicable                                    |

Sessions are the same format on both, so a folder recorded on one reads the
same on the other.
