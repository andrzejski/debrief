# Debrief — Windows handoff (2026-11-18, work done on the PC)

## Where things are
- **Repo (local only, no remote yet):** `C:\Users\AndrewKalinowski\Desktop\debrief\debrief`, branch `master`
- **Latest commit:** `92528fe` — "Run on Windows 11, and stop sessions promptly there (0.1.1)" (41 files)
- **Installed app:** `C:\Users\AndrewKalinowski\AppData\Local\Programs\Debrief\Debrief.exe` (0.1.1)
- **Local Windows build + test output:** `build-win-local/` (git-ignored), `build-win-local/test-results.txt`
- **Rollback of 0.1.0 exe:** `C:\Users\AndrewKalinowski\AppData\Local\DebriefDev\rollback-0.1.0\Debrief.exe`
- **Windows dev toolchain (Qt 6.12 LLVM-MinGW, whisper.cpp 1.9.5, helper scripts):** `C:\Users\AndrewKalinowski\AppData\Local\DebriefDev\`

## Done
- Compared the installed app with the repo. The installed `Debrief.exe` is byte-identical to `build-win-local/Debrief.exe`,
  so the repo holds everything the installed app has. The repo docs are newer than the installed copies.
- Committed all of the Windows work that had been sitting uncommitted:
  - Native mode (`DEBRIEF_NATIVE`, always on for Windows): Qt Multimedia mic, QScreen capture, in-process whisper.cpp
  - Notification-area tray, named-pipe control, log at `%LOCALAPPDATA%\debrief\debrief.log`, Windows light/dark theme
  - `packaging/windows/`: cross-compile (`build.sh`) and native (`build-native.ps1`) builds, installer/uninstaller, docs, licenses
  - 0.1.1 stop fix: Claude naming timeout 30 s → 2 s, keep the final mic buffer after Stop, clearer "finishing" status, timing logs
  - Tests build on Windows. Last native run: 35 passed, 0 failed, 3 skipped. Tests were not re-run for the commit.
- `.gitignore` now ignores `/build-windows.zip` (327 MB build artifact, left on disk).
- Set `core.filemode false` locally, because Windows was flagging `bin/build` and `bin/test` as mode changes.

## Open / next steps
1. **Put the repo on GitHub** (not done; `gh` isn't installed on this PC):
   ```powershell
   winget install --id GitHub.cli
   gh auth login
   gh repo create debrief --private --source "C:\Users\AndrewKalinowski\Desktop\debrief\debrief" --remote origin --push
   git clone https://github.com/<username>/debrief.git C:\Users\AndrewKalinowski\GitHub\debrief
   ```
   After that, work from `GitHub\debrief` and retire the Desktop copy.
2. **Packaging isn't certified.** `windeployqt` staging hit a platform-plugin discovery error, so no new 0.1.1 zip
   has been verified. The installed app reuses the existing 0.1.0 Qt runtime folder.
3. **Installed docs are stale.** `AppData\Local\Programs\Debrief\docs\Building.md` and `Troubleshooting.md` still have
   the 0.1.0 "not tested on Windows" notes. They'll update with the next install from the repo.
4. Qt warns that no fonts are deployed in the test run (`QFontDatabase: Cannot find font directory`). This is harmless for tests,
   but worth checking in a packaged build.
5. The live Claude naming test was skipped (needs `DEBRIEF_LIVE`). It's worth one manual check with `claude` on PATH.

## Background reading
- `docs/windows-stop-fix.md` covers the stop-fix changes, how to build on Windows, test env vars, and diagnosis limits.
- `packaging/windows/handoff/docs/` has the User Guide, Building, and Troubleshooting docs.
