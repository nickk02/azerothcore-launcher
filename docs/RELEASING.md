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

## The one thing that is not automatic

**A CI-built installer contains no Blizzard artwork.**

`Assets/wotlk-*.png|ico` are gitignored and never committed, so the runner has
no access to them. The build tolerates that by design: the hero image and logo
simply do not render, and the executable gets the default Windows icon.

So there are two different installers possible from the same commit:

| Built by | Artwork | Use for |
|---|---|---|
| CI, from a tag | none | reproducible builds, anything public |
| A machine with `Assets/` populated | full | what actually looks like the product |

If a release needs the art, build it locally and upload it over the CI asset:

```
msbuild azerothcore.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:AcVersion=2026.08.06
"C:\Program Files\Inno Setup 7\ISCC.exe" /DMyAppVersion=2026.08.06 installer.iss
gh release upload v2026.08.06 dist\AzerothCoreSetup.exe --clobber
```

Note `/p:AcVersion` and `/DMyAppVersion` are separate and both matter: the first
stamps the executable, the second stamps the installer. Passing only one gives
you a release whose installer and application disagree about what version they
are. The workflow passes both from the tag; by hand, it is on you.

Also worth being deliberate about: publishing an installer containing that
artwork on a public repository redistributes it, which is the thing
`.gitignore` line 1 exists to prevent for the source. That is a judgement call
each time, not a default.

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
