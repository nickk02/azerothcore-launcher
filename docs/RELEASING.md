# Releasing

## Cut a release

```
git tag v2026.08.06
git push origin v2026.08.06
```

The `release.yml` workflow then does the rest. It tests and builds the app with
the version from the tag, compiles the Windows installer, builds the macOS DMG,
and attaches `AzerothCoreSetup.exe`, `AzerothCore.dmg` and a `.sha256` file for
each to the GitHub release.

Use a `vMAJOR.MINOR.PATCH` tag. The workflow rejects any other shape, because
the Windows version resource cannot hold it.

## Before you tag

- `main` is green.
- `cargo test` passes on your machine.
- The app starts and the panel draws. CI proves that the code compiles and the
  tests pass. No workflow starts the user interface.

## Version numbers

`AC_VERSION` (set by `release.yml` from the tag) is stamped into the Windows
exe resources and compiled into the app. The installer takes the same value
through `ISCC /DMyAppVersion=...`. `release.yml` compares the built exe against
the tag and fails if they differ.

If you build by hand, supply both.

## Check a finished release

```powershell
(Get-Item installer\dist\AzerothCoreSetup.exe).VersionInfo.FileVersion
(Get-Item target\release\azerothcore.exe).VersionInfo.FileVersion
```

Both must show the tag without the leading `v`. The launcher shows its version
at the bottom of the panel.

## Update checks

The launcher updates itself. At startup it reads
`/repos/nickk02/azerothcore-launcher/releases/latest`, compares the tag against
its own version, and if the release is newer it downloads the installer,
verifies it against the published `.sha256`, and runs it.

This puts two requirements on every release, and `release.yml` meets both:

1. **The installer assets must be named `AzerothCoreSetup.exe` (Windows) and
   `AzerothCore.dmg` (macOS)**, each with a `.sha256` of the same name. The
   launcher matches on those exact names.
2. **Both assets for a platform must be present.** A release with an installer
   and no checksum is treated as no update at all, rather than as a reason to
   skip verification.

If you ever upload an installer by hand, upload its `.sha256` too, generated
from the file you actually attached. A checksum that does not match the asset
stops every client from updating, silently, because a failed verification is
deliberately quiet.
