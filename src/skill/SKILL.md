---
name: debrief
description: Read a Debrief debugging session (a folder of audio, transcript, and voice-triggered screenshots) and write a bug report from it. Use when the user mentions Debrief, a debugging session they recorded, ~/Documents/Debrief, or asks for a bug report from their screenshots and narration.
---

# Debrief sessions

Debrief records the user talking while they debug and takes a screenshot of the
whole screen whenever they say "screenshot" (or press S / run `debrief shot`).
Each session is a folder under `~/Documents/Debrief/`, named by its start time
and two or three words for what it was about
(`2026-11-13_10-55-04_save-button-grey`). The description is added when the
session finishes. The folder holds everything but the audio:

```text
transcript.md           what was said, time-coded, each screenshot embedded under the words it was taken during
session.json            the same, for reading programmatically
shot-01-00-03-12.png    the screenshots (number, then the HH-MM-SS time code)
```

The recording is beside the folder under the same name
(`../2026-11-13_10-55-04_save-button-grey.wav`, 16 kHz mono); `session.json`
has its path in `audio`. A folder dragged into a chat arrives without it.
Sessions from before this layout have `audio.wav` and `screenshots/` inside.

`session.json`:

- `transcript[]`: `index`, `start`/`end` (HH:MM:SS) and `start_ms`/`end_ms`,
  `status` (`done`, `pending`, `failed`), `text`, and `screenshots` (the numbers
  of the screenshots taken during it).
- `screenshots[]`: `number`, `file`, `time`, `source` (`voice` or `manual`),
  `transcript_index`, `said` (the sentence that asked for it), and `context`
  (the sentences around it). The words just before a screenshot usually say
  what is wrong on it.
- `title` is the description in the folder's name; `finished` is false while
  the session is still recording.

## Writing a bug report

1. Read `session.json` and `transcript.md`, then look at every screenshot.
2. Write `bug-report.md` in the session folder: a short summary, steps to
   reproduce (from what the user did and said), expected and actual behaviour,
   then each issue in order with its screenshot embedded
   (`![...](shot-...png)`), its time code, and what the user said about it.
3. Quote the user where it helps. Don't invent details the session doesn't
   show; say what is unclear instead.

## Commands

```bash
debrief status --json     # {"running":..,"state":"idle|recording|finishing","folder":..,"screenshots":..}
debrief start | stop | toggle | shot
debrief prompt            # the prompt the app's "Bug report with Claude" button uses
```
