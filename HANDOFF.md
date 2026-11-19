# Debrief handoff (2026-11-18, work done on the Windows PC)

## Where things are
- **GitHub:** `andrzejski/debrief`, branch `main`, at "Debrief 1.0.0", the first full release. 0.1.0 was a beta and is marked as a pre-release on GitHub.
- **Windows working copy:** `C:\Users\AndrewKalinowski\Desktop\debrief\debrief`, branch `master`, tracking `origin/main`.
  Commits are made as `andrzejski` (set in this repo's own git config).
- **Windows dev toolchain:** `C:\Users\AndrewKalinowski\AppData\Local\DebriefDev\` (Qt 6.12 LLVM-MinGW, whisper.cpp 1.9.5, `run_tests.ps1`).
  Local build and tests: `build-win-local/` (git-ignored).
- **GitHub sign-in on Windows:** GitHub CLI (`gh`) is installed and signed in as `andrzejski`, and git uses it for github.com.

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

## Next: build the Windows zip and release v1.0.0 (on Omarchy)
No `v1.0.0` tag or release exists yet. Creating the release creates the tag, which starts the Linux package build.

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
