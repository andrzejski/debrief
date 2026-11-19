DEBRIEF FOR macOS                                                version @VERSION@
================================================================================

Debug out loud. Debrief records your voice while you work, writes down what you
say a sentence at a time, and takes a screenshot of the whole screen whenever
you say "screenshot" (or "screencap", "capture", or "snap"). Each screenshot is
tied to the words you said around it, so a session reads as a story that Claude
can turn into a QA report or a step-by-step procedure.

Everything runs on your Mac. Nothing is sent anywhere unless you ask Claude for
a report.


INSTALL
-------

1. Drag Debrief.app into your Applications folder.
2. The first time, right-click (or Control-click) Debrief in Applications and
   choose Open, then Open again. Debrief isn't signed with an Apple developer
   certificate, so macOS asks before it runs it.
   - If macOS says the app is damaged or can't be opened, open Terminal and run
       xattr -dr com.apple.quarantine /Applications/Debrief.app
     then open it again.
   - On macOS 15 and later the question is under System Settings > Privacy &
     Security: scroll down and click Open Anyway.

Make sure the download matches your Mac: the arm64 zip is for Apple silicon
(M1 and later), the x86_64 zip for Intel Macs.


THE TWO PERMISSIONS
-------------------

- Microphone: macOS asks when you start your first session. Say yes.
- Screen Recording: macOS asks when the first screenshot is taken. Turn Debrief
  on under System Settings > Privacy & Security > Screen & System Audio
  Recording, then quit and reopen Debrief. Until you do, screenshots show only
  the desktop picture and Debrief's own window.


USING IT
--------

Space starts and stops a session. Talk through what you're doing; say
"screenshot" and pause when something is worth capturing. S takes one by hand.
While its window is closed Debrief stays in the menu bar; click the Dock icon
to bring the window back.

The switch in the top right corner chooses what Report with Claude (B) writes:
a QA report, or an SOP (a standard operating procedure for what you walked
through). It opens Terminal in the session folder and starts Claude Code, which
must be installed (https://claude.com/claude-code). When the report is written,
Finder opens on the session folder with the page selected.

Sessions are kept in Documents/Debrief.

From Terminal:
    /Applications/Debrief.app/Contents/MacOS/Debrief --help


REMOVE
------

Quit Debrief from its menu bar icon and drag Debrief.app to the Trash. Your
sessions in Documents/Debrief stay where they are.


LICENSES
--------

Debrief is under the MIT license. The licenses folder has it and those of what
the app carries inside: Qt, whisper.cpp, and the Whisper speech model.
