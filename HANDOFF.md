# Debrief handoff (2026-11-18, work done on the Windows PC)

## Where things are
- **GitHub:** `andrzejski/debrief`, branch `main`, at "Debrief 1.0.0", the first full release. 0.1.0 was a beta and is marked as a pre-release on GitHub.
- **Windows working copy:** `C:\Users\AndrewKalinowski\Desktop\debrief\debrief`, branch `master`, tracking `origin/main`.
  Commits are made as `andrzejski` (set in this repo's own git config).
- **Windows dev toolchain:** `C:\Users\AndrewKalinowski\AppData\Local\DebriefDev\` (Qt 6.12 LLVM-MinGW, whisper.cpp 1.9.5, `run_tests.ps1`).
  Local build and tests: `build-win-local/` (git-ignored).
- **GitHub sign-in on Windows:** GitHub CLI (`gh`) is installed and signed in as `andrzejski`, and git uses it for github.com.

## After 1.0.0 (2026-11-19, Windows PC)
- **Reports renamed** to `Debrief-Report.md` and `Debrief-Report.html` (prompt, skill, template, docs). The names
  live in `Backend::kReportMarkdown` / `kReportPage`. Older sessions keep their `bug-report.*` files.
- **Folder opens when the report is done.** After **Report with Claude** launches, the app checks the session folder
  every second (for up to three hours). Once `Debrief-Report.html` is newer than the request and unchanged between two
  checks, with `Debrief-Report.md` beside it, Windows opens Explorer on the folder with the page selected; Linux opens
  the folder with `QDesktopServices`. Test: `finishedReportIsShownOnce`. (38 tests at that point.)
- **More trigger words.** "screencap" (or "screen cap"), "capture", and "snap" take a screenshot as "screenshot"
  does (`Debrief::asksForScreenshot`). They match from the start of a word, so "captured" and "snapshot" count too.
  The window's own hint text still names only "screenshot"; the translations were left alone.
- **Terminal gives focus back (Windows only).** After **Report with Claude**, the app notes the window that was in
  front, waits for the console or Windows Terminal window to come forward, and polls `Backend::folderTrusted`
  (the folder's `hasTrustDialogAccepted` in `~/.claude.json`, or a new conversation file under
  `~/.claude/projects/`). Once trusted, and only if the terminal is still in front, focus goes back to the earlier
  window. Trust detection has a test (`folderIsTrustedOnceClaudeSaysSo`); the focus switch itself has **not been
  tried in the packaged app yet**. Linux does nothing here so far (`hyprctl dispatch focuscurrentorlast` is the
  likely way).
- **QA report / SOP switch.** A switch in the top right corner of the window (`Backend::sop`, kept in
  `settings.ini` in the app's config folder) chooses what **Report with Claude** asks for. QA report is the prompt
  as before. SOP uses `Backend::reportPrompt(true)`, which asks for `Debrief-SOP.md` and `Debrief-SOP.html`: a
  step-by-step procedure with the same crops, started from the new `src/skill/sop-template.html` and described in
  the "Writing an SOP" part of `SKILL.md`. `debrief prompt sop` prints it. The folder opens when the SOP files are
  done, as for a report. New strings "QA report" and "SOP" are in `en.json` and `da.json`.
  Tests on Windows: 41 passed, 0 failed, 3 skipped, three runs in a row. No SOP has been generated from a real
  session yet.
- **macOS edition.** The built-in edition (`DEBRIEF_NATIVE`) now builds as `Debrief.app` too: system light/dark
  theme, menu bar icon, microphone permission asked through Qt (`NSMicrophoneUsageDescription` in
  `packaging/macos/Info.plist.in`), Claude opened in Terminal through a `.command` script, the finished report
  shown with `open -R`. `packaging/macos/build.sh` runs `macdeployqt`, adds the speech model, signs ad hoc, and
  zips. The **Test** workflow builds, tests, and packages it for arm64 and x86_64 and keeps the zips as run
  artifacts; the **Release** workflow attaches both to a tag's release. Guide: `docs/macos.md`.
  **Checked only on GitHub's runners** (build, tests, package, `--version` from the packaged app). Nobody has yet
  run it at a real Mac: the microphone and Screen Recording prompts, Terminal launch, Finder reveal, menu bar icon,
  and Dock-icon reopen all need a first try. Not signed with a Developer ID or notarized. No focus hand-back and
  no start-at-login on macOS.
- **README rewritten** for the app as it is now (three editions, trigger words, the QA report / SOP switch, the
  files a report leaves, the command line), with a new `docs/screenshot.png` grabbed from the real window with a
  staged session (made-up invoice data).
- **v1.0.0 is released** (2026-11-19) with everything above: the Windows zip, both macOS zips, and the x86_64
  Linux package. The Windows zip was built on this PC, not on Omarchy: `packaging/windows/build-native.ps1` to
  compile, then `packaging/windows/package.py` with the dev Qt's own `llvm-objdump.exe` and `qmlimportscanner.exe`
  (`--runtime` is `Qt/bin`), the model from the installed app, and `d3dcompiler_47.dll` copied from the installed
  app because this Qt download has none. The staged app started on a clean PATH and answered `status`, so the
  earlier platform-plugin error did not come back. Not attached: the aarch64 Linux package (`docs/releasing.md`
  step 4).

## Done for 1.0.0
- **History joined.** The Windows PC had its own copy of the early commits. Its 0.1.1 fix now sits on top of
  GitHub's `v0.1.0` work (Linux and Windows guides, PKGBUILD, release workflow). Nothing on GitHub was overwritten.
- **0.1.1 stop fix** (`a8b8dd0`): stopping a session on Windows finishes promptly.
- **Session name kept on Windows** (`544302f`): virus scanning or indexing can briefly hold a just-written file,
  and Windows then refuses to rename the session folder (error 5). The rename now retries for up to a second.
  This was the intermittent `sessionKeepsAScreenshotOnlyWhenAskedFor` failure, about 1 run in 3 to 1 in 8; it passed 70 of 70 after the fix.
- **Report with Claude** (`a2a17f8`, from the Omarchy handoff patch, SHA-256 checked): the B button label, a prompt for
  `bug-report.md` plus a `bug-report.html` page grouped by product area with crops, `report-template.html`, and the
  skill installed on each press. Also added `.gitattributes` so `src/skill/*` stays LF on Windows (the embedded
  `SKILL.md` must start `---\nname: debrief\n`), and the Windows User Guide now says crops need ImageMagick or Python with Pillow.
- **Version 1.0.0** (planned as 0.1.2 in `d371f84`, renumbered as the first full release): `src/main.cpp` (twice), `packaging/arch/PKGBUILD`, `packaging/windows/debrief.rc`,
  `packaging/windows/handoff/README.txt`.
- **Tests on Windows:** 37 passed, 0 failed, 3 skipped, three runs in a row.

## Earlier plan: build the Windows zip and release v1.0.0 (on Omarchy)
Superseded: v1.0.0 was released from the Windows PC on 2026-11-19 (see above). Step 1 still applies to the Omarchy copy.

1. **Get a fresh copy.** On 2026-11-18 the GitHub repo was deleted and recreated so every commit is credited to
   `andrzejski` only (no work-account or Claude co-author credit). Every commit ID changed, so the old Omarchy copy
   no longer matches GitHub; `git pull` there would fail or mix the two histories. Its uncommitted report change is
   already in `main`. Move the old copy aside and clone again:
   ```bash
   mv ~/Work/debrief ~/Work/debrief-old
   gh repo clone andrzejski/debrief ~/Work/debrief
   cd ~/Work/debrief
   git config user.name "Andrew Kalinowski"
   git config user.email "andrew.kalinowski.91@gmail.com"
   git log --oneline -1          # Debrief 1.0.0
   ```
   Delete `~/Work/debrief-old` once the new copy builds. If the Windows toolchain lived inside the old folder, move it
   back out first (`build.sh` looks for `../debrief-windows-toolchain` by default).
2. Build the zip (toolchain set-up in `packaging/windows/handoff/docs/Building.md`):
   ```bash
   packaging/windows/build.sh
   ```
   It writes `build-windows/Debrief-1.0.0-windows-x64.zip`. Check that `.sha256` sits beside it.
3. Create the release with the zip:
   ```bash
   v=1.0.0
   gh release create "v$v" --target main --title "Debrief $v" --generate-notes \
     "build-windows/Debrief-$v-windows-x64.zip" \
     "build-windows/Debrief-$v-windows-x64.zip.sha256"
   ```
4. Optional: the aarch64 package, as in `docs/releasing.md` step 4.

## Still open
0. **Backup of the old repo:** `Desktop\debrief\Backup debrief-repo (2026.11.18)` on the Windows PC has the full old
   history (`debrief-full-history.bundle`) and the 0.1.0 release files.
1. **Windows packaging isn't certified on Windows itself.** The native `windeployqt` staging hit a platform-plugin
   discovery error, which is why the zip comes from `build.sh` on Linux. Try the 1.0.0 zip on this PC after release.
2. The live Claude naming test was skipped (needs `DEBRIEF_LIVE`). Worth one manual check with `claude` on PATH.
3. Qt warns that no fonts are deployed in the test run (`QFontDatabase: Cannot find font directory`). Harmless for
   tests; worth checking in the packaged build.
4. Work from `C:\Users\AndrewKalinowski\GitHub\debrief` on Windows: `gh repo clone andrzejski/debrief` there, then retire the Desktop copy.
5. **Hosts file on Windows:** an `xteink-unlocker` block in `C:\Windows\System32\drivers\etc\hosts` was sending
   `api.github.com` to `192.168.137.1`, which broke every GitHub sign-in. It's removed. If it goes back for the
   e-reader unlocker, pushes from this PC will fail again.

## Background reading
- `docs/releasing.md`: the release steps.
- `docs/windows-stop-fix.md`: the 0.1.1 stop fix, how to build on Windows, test env vars.
- `packaging/windows/handoff/docs/`: User Guide, Building, Troubleshooting.
