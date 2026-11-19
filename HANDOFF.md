# Debrief handoff (2026-11-18, work done on the Windows PC)

## Where things are
- **GitHub:** `andrzejski/debrief`, branch `main` at `8a81311` "Debrief 0.1.2". The Linux Test workflow passed on it.
- **Windows working copy:** `C:\Users\AndrewKalinowski\Desktop\debrief\debrief`, branch `master`, tracking `origin/main`.
  A local branch `backup/master-before-rebase` holds the pre-rebase history and can be deleted.
- **Windows dev toolchain:** `C:\Users\AndrewKalinowski\AppData\Local\DebriefDev\` (Qt 6.12 LLVM-MinGW, whisper.cpp 1.9.5, `run_tests.ps1`).
  Local build and tests: `build-win-local/` (git-ignored).
- **GitHub sign-in on Windows:** GitHub CLI (`gh`) is installed and signed in as `andrzejski`, and git uses it for github.com.

## Done in 0.1.2
- **History joined.** The Windows PC had its own copy of the early commits. Its 0.1.1 fix now sits on top of
  GitHub's `v0.1.0` work (Linux and Windows guides, PKGBUILD, release workflow). Nothing on GitHub was overwritten.
- **0.1.1 stop fix** (`b14b6c0`): stopping a session on Windows finishes promptly.
- **Session name kept on Windows** (`6edde53`): virus scanning or indexing can briefly hold a just-written file,
  and Windows then refuses to rename the session folder (error 5). The rename now retries for up to a second.
  This was the intermittent `sessionKeepsAScreenshotOnlyWhenAskedFor` failure, about 1 run in 3 to 1 in 8; it passed 70 of 70 after the fix.
- **Report with Claude** (`4d7fd12`, from the Omarchy handoff patch, SHA-256 checked): the B button label, a prompt for
  `bug-report.md` plus a `bug-report.html` page grouped by product area with crops, `report-template.html`, and the
  skill installed on each press. Also added `.gitattributes` so `src/skill/*` stays LF on Windows (the embedded
  `SKILL.md` must start `---\nname: debrief\n`), and the Windows User Guide now says crops need ImageMagick or Python with Pillow.
- **Version 0.1.2** (`8a81311`): `src/main.cpp` (twice), `packaging/arch/PKGBUILD`, `packaging/windows/debrief.rc`,
  `packaging/windows/handoff/README.txt`.
- **Tests on Windows:** 37 passed, 0 failed, 3 skipped, three runs in a row.

## Next: build the Windows zip and release v0.1.2 (on Omarchy)
No `v0.1.2` tag or release exists yet. Creating the release creates the tag, which starts the Linux package build.

1. The Omarchy copy still has the report change as uncommitted work. It is in `main` now, so put it aside and update:
   ```bash
   cd ~/Work/debrief
   git stash push -u -m "report-with-claude, now in main"
   git pull
   git log --oneline -1          # 8a81311 Debrief 0.1.2
   ```
   Drop the stash once you've checked `main` has everything: `git stash drop`.
2. Build the zip (toolchain set-up in `packaging/windows/handoff/docs/Building.md`):
   ```bash
   packaging/windows/build.sh
   ```
   It writes `build-windows/Debrief-0.1.2-windows-x64.zip`. Check that `.sha256` sits beside it.
3. Create the release with the zip:
   ```bash
   v=0.1.2
   gh release create "v$v" --target main --title "Debrief $v" --generate-notes \
     "build-windows/Debrief-$v-windows-x64.zip" \
     "build-windows/Debrief-$v-windows-x64.zip.sha256"
   ```
4. Optional: the aarch64 package, as in `docs/releasing.md` step 4.

## Still open
1. **Windows packaging isn't certified on Windows itself.** The native `windeployqt` staging hit a platform-plugin
   discovery error, which is why the zip comes from `build.sh` on Linux. Try the 0.1.2 zip on this PC after release.
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
