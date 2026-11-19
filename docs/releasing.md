# Releasing

One version, one GitHub release, one download per edition:

| Asset | Edition | Built by |
|---|---|---|
| `Debrief-<version>-windows-x64.zip` (+ `.sha256`) | Windows | `packaging/windows/build.sh`, on a Linux machine with the Windows toolchain |
| `Debrief-<version>-macos-arm64.zip` and `-x86_64.zip` (+ `.sha256`) | macOS | `.github/workflows/release.yml`, when the tag is pushed (`packaging/macos/build.sh` on GitHub's Mac machines) |
| `debrief-<version>-1-x86_64.pkg.tar.zst` | Linux | `.github/workflows/release.yml`, when the tag is pushed |
| `debrief-<version>-1-aarch64.pkg.tar.zst` | Linux | `makepkg` on an aarch64 machine (GitHub's runners build x86_64 only) |

## Steps

1. Set the version in four places, and commit: `setApplicationVersion` in
   `src/main.cpp` (twice), `DEBRIEF_VERSION` in `CMakeLists.txt` (what the
   macOS app shows), `pkgver` in `packaging/arch/PKGBUILD`, and the version at
   the top of `packaging/windows/handoff/README.txt`.
2. Build the Windows zip: `packaging/windows/build.sh` (see
   [Building.md](../packaging/windows/handoff/docs/Building.md)).
3. Push, then create the release with the Windows zip. Creating it also creates
   the tag, which starts the workflow that builds and attaches the x86_64 Linux
   package and the two macOS zips:

   ```bash
   v=0.1.0
   git push
   gh release create "v$v" --target main --title "Debrief $v" --generate-notes \
     "build-windows/Debrief-$v-windows-x64.zip" \
     "build-windows/Debrief-$v-windows-x64.zip.sha256"
   ```

4. On an aarch64 machine, build and attach that package too:

   ```bash
   cd packaging/arch && PKGEXT=.pkg.tar.zst makepkg -f
   gh release upload "v$v" debrief-$v-1-aarch64.pkg.tar.zst
   ```

The macOS zips are signed ad hoc, not with an Apple Developer ID, and are not
notarized, so macOS asks before opening the app the first time
([macos.md](macos.md#install) says how). Signing and notarizing would need an
Apple developer account and its certificate in the repository's secrets.

If the Linux package or the macOS zips need rebuilding for a tag that already exists, run the
**Release** workflow by hand from the Actions tab and give it the tag.
