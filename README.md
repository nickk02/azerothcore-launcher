# AzerothCore Launcher

A launcher for World of Warcraft 3.3.5a (Wrath of the Lich King) private realms.
It stores your realm address and client path, remembers your account in the
Windows Credential Manager, reports whether the realm is reachable, and starts
the client against it.

Windows only. C++/WinRT on WinUI 3 (Windows App SDK), built as an unpackaged,
self-contained x64 application.

> **No game client is included.** Nothing here downloads, bundles, or
> redistributes Blizzard game data. You supply your own legally obtained 3.3.5a
> client.

## Install

Download `AzerothCoreSetup.exe` from [Releases](../../releases) and run it. It
installs per-user under `%USERPROFILE%\AzerothCore` and needs no administrator
rights. The Visual C++ 2015-2022 x64 redistributable is fetched and installed
automatically if it is missing.

Setup creates an empty `Client\` folder beside the app and opens it when it
finishes. Copy your 3.3.5a client in there, then use **Browse** in the launcher
to point at `Wow.exe`.

## Layout

| Path | What it is |
|---|---|
| `Core/` | Everything that is not UI: config, credentials, realm probing, addon source, version. No WinUI dependency, which is what makes it testable. |
| `Pages/` | The XAML pages and their code-behind. |
| `MainWindow.xaml` | The shell: a fixed 1100x720 window and a title bar that is chrome only. |
| `installer.iss` | Inno Setup script for the installer. |
| `tools/run-tests.ps1` | Builds and runs the `Core` test suites. |
| `docs/RELEASING.md` | How a release is cut. |

## Building

Requires Visual Studio 2022 or newer with the C++ workload and the Windows SDK.

```
nuget restore azerothcore.slnx
msbuild azerothcore.vcxproj /p:Configuration=Release /p:Platform=x64
```

The output is a self-contained deployment in `x64\Release\azerothcore\`.

### Artwork

`Assets/wotlk-*` is Blizzard-owned art used for the hero image, wordmark and
application icon. It is tracked here, so a clone builds the complete product.

The build tolerates it being absent anyway (`Assets` is a wildcard content item
and the icon resource is conditional), which yields a working but art-free
launcher. CI checks explicitly for the files, since that tolerance would
otherwise let a missing asset through unnoticed.

### Versioning

The version comes from one place: the build stamps `app.rc`'s `VERSIONINFO`, and
the app reads its own resource back at runtime. Pass it explicitly for a
release build:

```
msbuild azerothcore.vcxproj /p:Configuration=Release /p:Platform=x64 /p:AcVersion=2026.08.06
```

Without `AcVersion` you get `0.0.0-dev`, which is the honest label for an
untagged build rather than something that looks like a release.

## Tests

```
.\tools\run-tests.ps1
```

Each `Core/*Tests.cpp` is a standalone program with its own `main()` that
asserts its way through one component. They are deliberately not part of
`azerothcore.vcxproj`, which is a WinUI application and cannot link five extra
`main` functions.

They fail via `assert`, so they **must not** be compiled with `NDEBUG` defined
or every one of them passes unconditionally while looking green. The runner
passes `/UNDEBUG` for exactly that reason.

## CI

| Workflow | Trigger | Does |
|---|---|---|
| `build.yml` | push / PR to `main` | Restores, builds Release x64, runs the `Core` tests, uploads the build output |
| `release.yml` | tag `v*` | Builds, tests, compiles the installer with the version taken from the tag, attaches it plus a SHA256 to the release |
| `felbite-healthcheck.yml` | daily | Checks the addon source is still reachable |

Both build workflows verify the UI assets are present before compiling, so a
CI-produced installer is the same product a local build produces.

## Status

Working: realm configuration, client path, credential storage and autofill,
realm reachability, launching the client, the installer.

Not built: addon management (`Pages/AddonsPage` exists but is unreachable on
purpose rather than shipped half-finished), the armory view (no data source),
and in-app update checking.

## Licence

Not yet chosen. Until one is added, default copyright applies and no
redistribution rights are granted.
