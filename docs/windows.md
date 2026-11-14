# Debrief for Windows

The Windows edition runs on Windows 11 with nothing else to install: the
microphone and the screen go through Qt, whisper.cpp transcribes inside the app
with a model file beside it, and the app sits in the notification area. For
Linux, see [linux.md](linux.md).

## Install

1. Download `Debrief-<version>-windows-x64.zip` from the
   [latest release](https://github.com/andrzejski/debrief/releases/latest).
2. Extract the whole zip (right-click it > Extract All). Don't run it from
   inside the zip.
3. Double-click `Install.cmd`. If Windows shows "Windows protected your PC",
   click More info, then Run anyway; Debrief isn't code-signed.
4. Answer whether Debrief should start when you sign in.

No administrator rights are needed. Debrief goes in
`%LOCALAPPDATA%\Programs\Debrief` and shows up in the Start menu. To run it
without installing, start `app\Debrief.exe` from the extracted folder.

To remove it: Settings > Apps > Installed apps > Debrief > Uninstall. Your
sessions in `Documents\Debrief` stay.

## Using it

1. Open Debrief from the Start menu and click **Start session** (or press
   Space). The first time, Windows may ask to let Debrief use the microphone.
2. Say a sentence and pause; it appears in the window within a second or two.
3. Say "take a screenshot" and pause; a screenshot appears under your words.
4. Click **Stop session**. Debrief names the session and tells you where it is.

Closing the window doesn't stop Debrief. It stays in the notification area (the
icons by the clock), with a red dot while recording. Left-click the icon to open
the window; right-click it to start or stop a session, take a screenshot, open
the folders, or quit.

## More

The full Windows documentation ships inside the zip and lives here in the
repository:

- [User guide](../packaging/windows/handoff/docs/User-Guide.md): the
  notification area, starting with Windows, the command line, changing the
  speech model
- [Troubleshooting](../packaging/windows/handoff/docs/Troubleshooting.md)
- [Building](../packaging/windows/handoff/docs/Building.md): how the Windows
  edition is cross-compiled from Linux and packed
