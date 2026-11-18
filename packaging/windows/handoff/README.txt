DEBRIEF FOR WINDOWS 11                                             version 0.1.1
================================================================================

Debug out loud. Debrief records your voice while you work, writes down what you
say a sentence at a time, and takes a screenshot of the whole screen whenever
you say "screenshot". Each screenshot is tied to the words you said around it,
so a session reads as a story that Claude can turn into a bug report.

Everything runs on your machine. Nothing is sent anywhere unless you ask Claude
for a bug report.


INSTALL
-------

1. Extract the whole zip (right-click it > Extract All). Don't run it from
   inside the zip.
2. Double-click Install.cmd.
   - If Windows shows "Windows protected your PC", click More info, then
     Run anyway. Debrief isn't code-signed, so Windows doesn't know it.
3. Answer whether Debrief should start when you sign in (it waits quietly in
   the notification area; Y is a good answer if you'll use it often).

That's it. No administrator rights, no other software. Debrief goes in
%LOCALAPPDATA%\Programs\Debrief and shows up in the Start menu and under
Settings > Apps > Installed apps.

Prefer not to install? The app folder is self-contained: run app\Debrief.exe
from wherever you extracted it.


FIRST SESSION (two minutes, and it checks that everything works)
----------------------------------------------------------------

1. Open Debrief from the Start menu.
2. Click Start session (or press Space).
   - The first time, Windows may ask to let Debrief use the microphone. Allow.
3. Say a sentence, pause. It appears in the window within a second or two.
4. Say "take a screenshot", pause. A screenshot appears under your words.
5. Click Stop session. Debrief names the session and tells you where it is:
   Documents\Debrief\<date>_<time>_<what-it-was-about>

If any step doesn't do that, docs\Troubleshooting.md covers it, and the log at
%LOCALAPPDATA%\debrief\debrief.log says what went wrong.


WHERE IT LIVES
--------------

Closing the window doesn't stop Debrief. It stays in the notification area
(the icons by the clock; Windows may tuck it behind the ^ arrow, and you can
drag it out to keep it visible). The icon has a red dot while recording.

  Left-click the icon     open the window
  Right-click the icon    start/stop a session, screenshot now, open folders,
                          quit

Idle, it uses no CPU and about as much memory as a small app. The speech model
is only loaded while you are in a session.


WHAT'S IN THIS FOLDER
---------------------

  Install.cmd, Install.ps1    the installer (per-user, no admin)
  README.txt                  this file
  docs\User-Guide.md          everything Debrief does, keys, command line
  docs\Troubleshooting.md     when something doesn't work
  docs\Building.md            how this was built, and how to rebuild it
  licenses\                   Debrief's license and those of what it includes
  app\                        the program
    Debrief.exe               the app
    models\ggml-base.en.bin   the speech-to-text model (English)
    Uninstall.cmd             removes Debrief (your sessions are kept)
    *.dll, qml\, platforms\, ...   Qt, which the app is built with


REMOVE
------

Settings > Apps > Installed apps > Debrief > Uninstall, or run Uninstall.cmd
in %LOCALAPPDATA%\Programs\Debrief. Your sessions in Documents\Debrief stay.
