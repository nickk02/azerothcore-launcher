# AzerothCore Launcher

A launcher for World of Warcraft 3.3.5a (Wrath of the Lich King) private realms.

The launcher stores your realm address and your client path. It saves your
account name in the Windows Credential Manager. It shows if the realm is
online. It starts the client against that realm.

Windows only. The app uses C++/WinRT and WinUI 3 (Windows App SDK). It builds
as an unpackaged, self-contained x64 application.

> **The game client is not included.** This project does not download or
> supply Blizzard game data. Use your own 3.3.5a client.

## Install

1. Download `AzerothCoreSetup.exe` from [Releases](../../releases).
2. Run it. Setup installs to `%USERPROFILE%\AzerothCore`. It does not need
   administrator rights.
3. Copy your 3.3.5a client into the `Client\` folder. Setup creates this folder
   and opens it at the end.
4. Start the launcher. Click **Browse**. Select `Wow.exe` in that folder.
5. Type the realm address, your account name, and your password. Click **Play**.

Setup also installs the Visual C++ 2015-2022 x64 redistributable if it is
missing.

## Layout

| Path | Contents |
|---|---|
| `Core/` | Config, credentials, realm checks, addon source, version. No WinUI dependency, so the tests can build it. |
| `Pages/` | The XAML pages and their code-behind. |
| `MainWindow.xaml` | The shell. A fixed 1100x720 window with a title bar that holds no controls. |
| `installer.iss` | The Inno Setup script. |
| `tools/run-tests.ps1` | Builds and runs the `Core` tests. |
| `docs/RELEASING.md` | How to cut a release. |

## Build

You need Visual Studio 2022 or later with the C++ workload and the Windows SDK.

```
nuget restore azerothcore.slnx
msbuild azerothcore.vcxproj /p:Configuration=Release /p:Platform=x64
```

The build writes a self-contained deployment to `x64\Release\azerothcore\`.

### Version numbers

The version has one source: the git tag. The build writes the version into the
`VERSIONINFO` resource in `app.rc`. The app then reads the version from its own
executable. Supply the version for a release build:

```
msbuild azerothcore.vcxproj /p:Configuration=Release /p:Platform=x64 /p:AcVersion=2026.08.06
```

If you do not supply `AcVersion`, the build uses `0.0.0`. An untagged build
must not report a release version.

### Artwork

`Assets/wotlk-*` holds the hero image, the wordmark, and the application icon.
Blizzard owns this art. The files are in the repository, so a clone builds the
complete product.

The build also works without these files. `Assets` is a wildcard content item,
and the icon resource compiles only if the `.ico` file is present. The result
is a launcher with no art. Both workflows check for the four files, because
this build passes and hides the fault until somebody opens the app.

## Tests

```
.\tools\run-tests.ps1
```

Each file in `Core/*Tests.cpp` is a separate program with its own `main()`.
Each one tests a single component. These files are not part of
`azerothcore.vcxproj`, because a WinUI application cannot link five extra
`main` functions.

The tests report failure through `assert`. Do not compile them with `NDEBUG`
defined. `NDEBUG` removes every assert, and all five tests then pass and report
success. The runner supplies `/UNDEBUG` to prevent this.

## Continuous integration

| Workflow | Runs on | Steps |
|---|---|---|
| `build.yml` | Push or PR to `main` | Restore, build Release x64, run the `Core` tests, upload the output |
| `release.yml` | Tag `v*` | Build, test, compile the installer with the version from the tag, attach the installer and a SHA256 file |
| `felbite-healthcheck.yml` | Daily | Check that the addon source responds |

Both build workflows check the artwork before they compile. A CI installer and
a local installer contain the same files.

## Status

These features work: realm configuration, client path, credential storage and
autofill, realm status, client launch, and the installer.

These features are absent: addon management, the armory view, and update
checks. `Pages/AddonsPage` compiles, but nothing opens it. The armory view has
no data source.

## Licence

None yet. Until this project adds a licence, default copyright applies and you
receive no rights to redistribute.
