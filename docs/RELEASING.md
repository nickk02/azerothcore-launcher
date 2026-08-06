# Releasing

## The short version

```
git tag v2026.08.06
git push origin v2026.08.06
```

`release.yml` picks the tag up, builds, runs the tests, compiles the installer
with the version derived from the tag, and attaches `AzerothCoreSetup.exe` plus
a `.sha256` to the GitHub release.

Tags must be `vMAJOR.MINOR.PATCH`. The workflow rejects anything else rather
than producing an installer with a malformed version, since `VERSIONINFO`
cannot represent one.

## Artwork

The UI art is tracked in the repository, so a CI build from a tag produces the
complete product. There is no manual build-and-upload step, and no difference
between what CI ships and what a developer builds locally.

Both workflows fail if any of the four asset files is missing. That check exists
because the project deliberately tolerates their absence (wildcard content item,
conditional icon resource), which means a missing asset would otherwise sail
through the build and only surface when somebody opened the launcher and found
a blank panel.

Note that publishing the installer redistributes that artwork. That is a settled
decision here, not an oversight, but it is worth being conscious of.

## Before tagging

- `main` is green.
- `.\tools\run-tests.ps1` passes locally.
- The app actually launches and the panel renders. CI proves it compiles, not
  that it works; nothing in the pipeline runs the UI.

## Verifying afterwards

The installer's version and the app's version come from different mechanisms
and are worth spot-checking once:

```powershell
(Get-Item dist\AzerothCoreSetup.exe).VersionInfo.FileVersion
(Get-Item x64\Release\azerothcore\azerothcore.exe).VersionInfo.FileVersion
```

Both should read the tag without its leading `v`. The launcher displays the
second of those in the bottom right of its panel.

## Update checking

Not implemented. The groundwork is in place, though: the app knows its own
version at runtime via `Core::AppVersion::Current()`, and releases are tagged
`vX.Y.Z` with a predictable asset name, so a check against
`/repos/nickk02/azerothcore-launcher/releases/latest` is a small piece of work.

Worth deciding before building it: whether the launcher only *notifies* and
links to the release page, or downloads and runs the installer itself. The
second means the app fetches an executable and launches it, which deserves at
minimum a signature or hash check against the published `.sha256` rather than
trusting whatever the URL returns.
