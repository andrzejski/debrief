---
name: debrief
description: Read a Debrief session (a folder of audio, transcript, and voice-triggered screenshots) and write the report from it, Debrief-Report.md plus a Debrief-Report.html page grouped by product area with focused screenshot crops. Use when the user mentions Debrief, a session they recorded, ~/Documents/Debrief, the "Report with Claude" button, or asks for a report or bug report from their screenshots and narration.
---

# Debrief sessions

Debrief records the user talking while they debug or review something and takes
a screenshot of the whole screen whenever they say "screenshot", "screencap", "capture", or "snap" (or press S /
run `debrief shot`). Each session is a folder under `~/Documents/Debrief/`,
named by its start time and two or three words for what it was about
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

## Writing the report

The report is always two files in the session folder, plus a folder of crops:

```text
Debrief-Report.md    everything in the order it was said
Debrief-Report.html  the same feedback regrouped by the part of the product it is about
crops/               the focused pieces of screenshots the page shows
```

Work in this order:

1. Read `transcript.md` and `session.json` in full, then look at every
   screenshot. Read what is on each one: headings, labels, numbers, footnotes,
   the address bar. Most of the detail in a good report comes from the
   screenshots, because the transcript is short and often garbled.
2. Write `Debrief-Report.md`.
3. Cut the crops, look at them to check they show what was meant, then write
   `Debrief-Report.html`.
4. Check that every image and link in the page points at a file that exists.

### Rules for both files

- Quote the user verbatim from the transcript. The transcript is speech to
  text, so some of it is wrong. Never tidy a quote. When a garbled phrase has
  an obvious reading from what is on screen, give the reading and say it is a
  reading ("leech" is read here as "lead sheets"). When it has none, say the
  session doesn't show what was meant.
- Keep three things apart: what the user said, what the screenshot shows, and
  anything you noticed that they did not comment on. Label the third kind
  "not commented on".
- Use the product's own labels from the screenshots for screens, rows, and
  buttons.
- Record praise as well as problems. Something the user liked is an entry
  marked as working well, not a bug.
- Give the user's verdict in their words when they gave one ("one or two
  tops", "can't be released yet").
- Don't invent steps, causes, or fixes. Steps to reproduce are the steps the
  session shows.
- Identify what was under review by what is not personal: a project id from
  the address bar, an environment name, a screen title. Leave client and
  taxpayer names out of the text. The screenshots may still show them, so say
  so in the hand-off message.

### Debrief-Report.md

1. A title, then a short list: session start, audio length, screenshot count,
   the app and environment, and a line saying time codes are positions in the
   recording and quotes are the transcript verbatim.
2. **Summary**: one paragraph. Blocking findings first, then the smaller
   ones, then what was praised.
3. **Steps to reproduce**: one numbered list per return, environment, or flow
   used, with the screenshots it covers.
4. **Expected behaviour** and **Actual behaviour**: matching bullet lists.
5. **Issues, in order**: one numbered `###` entry for each thing raised, in
   the order it was said, each with a heading that states the finding and its
   time code, the full screenshot embedded (`![Screenshot 3 at 00:02:49](shot-03-00-02-49.png)`),
   the quote, and a sentence or two on what the screenshot shows. Remarks made
   without a screenshot go in the nearest entry with their time code.

### Debrief-Report.html

Start from `report-template.html`, which is beside this file. Copy it to
`Debrief-Report.html`, keep its styles and class names, and fill it in. It is one
self-contained file with no scripts and no outside resources, and it refers to
the crops and screenshots by relative path.

- **Group by product area.** An area is whatever the user was reviewing one at
  a time: a screen, a feature, a document, a lead sheet. One section per area,
  the most serious first. Name the navigation heading after the kind of area
  ("By lead sheet", "By screen").
- **Scorecard** at the top: one row per area with its status, the user's
  verdict quoted, and the main finding in a few words.
- **Status** on every area and, where it differs, every entry: Blocking,
  Issue, Open question, or Works well.
- **One entry per topic** inside an area, even when the user came back to the
  topic later or on another screenshot. Put both time codes on it or make two
  entries under the same area. Each entry has a heading that states the
  finding, the time code, the quote, what the screenshot shows, and its crops.
- **Feedback that fits no single area** (approval flow, navigation, anything
  said before the review proper) goes in its own sections after the areas.
- **Steps to reproduce** and an expected-against-actual table close the page.
- Link back to `Debrief-Report.md` in the header.

### Crops

A crop is the part of a screenshot one entry is about, cut out so the reader
sees it without hunting. Every entry gets at least one.

- Cut the smallest region that still makes sense alone: the rows in question
  with their labels, the control, the highlighted line in a document.
- Pair crops to make the point: the selected cell beside where the viewer
  landed, the broken state beside the working one, the wrong destination
  beside the right one.
- Tables wider than about six columns don't survive shrinking. Crop the label
  column and the first few value columns, and say in the caption that the rest
  read the same.
- Name them by screenshot number and content (`crops/07-part-ii.png`) and save
  them as PNG at the screenshot's own resolution.
- Each crop links to its full screenshot, has alt text, and has a caption
  saying what to look at.
- Use side-by-side grids for pairs, full width for table crops, and the narrow
  grid for small controls.

Screenshots are often shown to you scaled down. Convert the coordinates you
see to the file's real pixel size before cutting. With ImageMagick:

```bash
magick shot-07-00-26-11.png -crop 1715x558+945+1530 +repage crops/07-part-ii.png
```

Python with Pillow does the same where `magick` is missing. If neither is
available, skip the `crops/` folder and show the region in the page with a
fixed-size box, `overflow: hidden`, and the full screenshot positioned inside
it. After cutting, view the crops (a contact sheet is enough) and fix any that
miss.

### Handing off

Finish with a short message: where the two files are, the blocking findings,
which phrases you had to interpret, and that the page needs `crops/` and the
`shot-*.png` files kept beside it. Mention it if the crops show personal or
client details.

## Commands

```bash
debrief status --json     # {"running":..,"state":"idle|recording|finishing","folder":..,"screenshots":..}
debrief start | stop | toggle | shot
debrief prompt            # the prompt the app's "Report with Claude" button uses
debrief skill install     # puts this skill and report-template.html in ~/.claude/skills/debrief
```

The "Report with Claude" button installs this skill before it opens Claude, so
the copy in `~/.claude/skills/debrief` matches the app.
