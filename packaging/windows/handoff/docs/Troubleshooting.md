# Troubleshooting

**The log.** Debrief writes what goes wrong to
`%LOCALAPPDATA%\debrief\debrief.log` (paste that into Explorer's address bar).
When reporting a problem, send that file.

**Windows 0.1.1 update.** Built and tested natively on Windows. The test run passed 35 checks, including microphone stop/restart, speech transcription, and UI behavior; two Linux-only tests and the opt-in live Claude test were skipped. See `windows-stop-fix.md` for changes and diagnosis limits.

## Installing

| What you see | What to do |
|---|---|
| "Windows protected your PC" (SmartScreen) | **More info** > **Run anyway**. Debrief isn't code-signed. |
| `Install.cmd` says it can't find `app\Debrief.exe` | You ran it from inside the zip. Extract All first. |
| PowerShell says scripts are disabled | Run `Install.cmd`, not `Install.ps1`; it allows just this one script. If your organization blocks that too, skip installing and run `app\Debrief.exe` directly. |
| Antivirus quarantines something | Unsigned programs that record the screen and microphone can look suspicious. Restore the file and allow the Debrief folder, or run it from where you extracted it. |

## Starting

| What you see | What to do |
|---|---|
| "The code execution cannot proceed because X.dll was not found" | The app folder is incomplete. Extract the zip again and reinstall. If it names a DLL that isn't in the `app` folder at all, send its name back: that's a packaging miss. |
| "Debrief couldn't open its window" | Send the log. As a stopgap, try setting the environment variable `QSG_RHI_BACKEND=opengl` or `QT_QUICK_BACKEND=software` and starting it again (graphics driver trouble). |
| Nothing seems to happen | It may already be running: look for its icon behind the **^** by the clock. Starting Debrief a second time just brings up the first one's window. |
| The window is blank or flickers | Graphics driver. Set `QT_QUICK_BACKEND=software` (Win+R > `sysdm.cpl` > Advanced > Environment Variables) and restart Debrief. |

## Recording

| What you see | What to do |
|---|---|
| "No microphone was found" | Plug one in or enable it: Settings > System > Sound > Input. Debrief uses the default input device chosen there. |
| "Couldn't open the microphone" | Settings > Privacy & security > Microphone: turn on **Microphone access** and **Let desktop apps access your microphone**. |
| Recording runs but no sentences appear | You're not being heard: check the input level under Settings > System > Sound > Input while you talk, and that the right microphone is the default. The dot by "Recording" brightens when Debrief hears speech. |
| Sentences only appear after long delays | Sentences end when you pause, so leave a moment of quiet between them. In a noisy room a headset helps, since Debrief waits for quiet to end a sentence. |
| "No speech model was found…" | `models\ggml-base.en.bin` is missing from the app folder. Reinstall, or put a model there (see the user guide). |
| Words are often wrong | Use a closer microphone, or switch to the `small.en` model (see the user guide). |
| A screenshot you didn't ask for | "Capture" and "snap" are trigger words too, along with "screencap", and count wherever they come in a sentence ("it doesn't capture the input"). Delete the extra `shot-*.png` or leave it; the transcript shows the sentence it came from. |
| Saying "screenshot" takes none | It has to be transcribed as "screenshot" or "screen shot" (or "screencap", "capture", or "snap"). Say it in a short sentence and pause after. Check the transcript for what was heard. **Screenshot now** (S) always works. |
| Screenshots are black or miss a window | Some protected content (DRM video, certain banking or password windows) can't be captured by any app. |
| A second monitor is misplaced in the screenshot | Mixed display scaling can shift where monitors are placed in the combined image. Each monitor's content is still complete. Please report it with your display layout. |

## Report with Claude

| What you see | What to do |
|---|---|
| A terminal opens and says `'claude' is not recognized` | Install Claude Code (<https://claude.com/claude-code>), open a new terminal so it's on your PATH, and press B again. |
| Sessions are named with odd words | Without Claude Code, a session is named from the words you said most. With it, the name comes from Claude. |

## Removing every trace

Uninstalling removes the app, shortcuts, and log. What stays is yours to keep or
delete: `Documents\Debrief` (your sessions) and, if you ran `skill install`,
`%USERPROFILE%\.claude\skills\debrief`.
