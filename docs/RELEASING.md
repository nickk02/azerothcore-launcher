# Releasing

## Cut a release

```
git tag v2026.08.06
git push origin v2026.08.06
```

The `release.yml` workflow then does the rest. It builds the app, runs the
tests, compiles the installer, and attaches `AzerothCoreSetup.exe` and a
`.sha256` file to the GitHub release.

Use a `vMAJOR.MINOR.PATCH` tag. The workflow rejects any other shape, because
`VERSIONINFO` cannot hold it.

## Before you tag

- `main` is green.
- `.\tools\run-tests.ps1` passes on your machine.
- The app starts and the panel draws. CI proves that the code compiles. No
  workflow starts the user interface.

## Artwork

The art is in the repository, so a CI build produces the complete product. You
do not need to build or upload anything by hand.

Both workflows stop if any of the four asset files is missing. The build itself
does not stop. `Assets` is a wildcard content item, and the icon resource is
conditional. A missing file therefore produces a launcher with a blank panel
and no build error. The check exists to catch that.

Note that the installer carries this artwork to everybody who downloads it.

## Version numbers

Two separate mechanisms set the version, and both read the tag:

| Mechanism | Sets |
|---|---|
| `msbuild /p:AcVersion=2026.08.06` | `VERSIONINFO` in `azerothcore.exe` |
| `ISCC /DMyAppVersion=2026.08.06` | The installer |

`release.yml` supplies both, then compares the built binary against the tag and
fails if they differ. An earlier release shipped an installer marked
`2026.08.06` that held an app reporting `0.0.0`, because the workflow set only
one of the two.

If you build by hand, supply both.

## Check a finished release

```powershell
(Get-Item dist\AzerothCoreSetup.exe).VersionInfo.FileVersion
(Get-Item x64\Release\azerothcore\azerothcore.exe).VersionInfo.FileVersion
```

Both must show the tag without the leading `v`. The launcher shows the second
value at the bottom right of its panel.

## Update checks

The launcher does not check for updates yet. The parts it needs are in place.
`Core::AppVersion::Current()` returns the running version. Releases use `vX.Y.Z`
tags and a fixed asset name, so a query to
`/repos/nickk02/azerothcore-launcher/releases/latest` is a small job.

Decide one thing first. The launcher can show a message and open the release
page, or it can download the installer and run it. If it runs the installer, it
must check the file against the published `.sha256` first. Do not run a
downloaded executable on trust.
