# AzerothCore Launcher

A small Windows launcher for World of Warcraft 3.3.5a (Wrath of the Lich King)
private servers, including any AzerothCore realm. You enter your server's
address once. The launcher then writes the realmlist, shows whether the realm
is online, remembers your account name, and starts the game.

> **The game client is not included.** This project does not download or
> supply Blizzard game data. Use your own 3.3.5a client.

## What you need

- Windows 10 or 11, 64-bit.
- A 3.3.5a (build 12340) client folder that contains `Wow.exe`.
- The address of the server you want to play on. Your server's admin gives you
  this.

## Install

1. Download `AzerothCoreSetup.exe` from [Releases](../../releases).
2. Windows may show a blue "Windows protected your PC" screen, because the
   installer is not code-signed. Click **More info**, then **Run anyway**.
3. Setup installs to `%USERPROFILE%\AzerothCore`. It does not need
   administrator rights. It also installs the Visual C++ 2015-2022 x64
   redistributable if it is missing.
4. Copy your 3.3.5a client into the `Client\` folder that Setup opens at the
   end, or keep it where it is and point the launcher at it in step 5.
5. Start the launcher. Click **Browse** and pick `Wow.exe` from your client
   folder.

## Set your server's realmlist

Type the realm address into **Realm address**. Use the same text you would put
after `set realmlist` in `WTF\realmlist.wtf`, for example `logon.example.com`
or `192.168.1.100`. The launcher checks port 3724 for the status line. If your
server's login port is different, add it as `host:port`.

Type your account name and password. Tick **Remember me** if you want the
launcher to keep them in the Windows Credential Manager and type them into the
game's login screen for you. Click **Play**.

When you click Play, the launcher writes `WTF\realmlist.wtf` (and the locale
folder copies, if you have them) for you. You do not edit that file by hand.

The status line above the account field shows **Online** or **Unreachable**
for the address you typed.

## If it will not start

**"That isn't a 3.3.5a Wow.exe" or "Failed to launch WoW".** You picked the
wrong file or the wrong folder. Pick the file named exactly `Wow.exe` that sits
next to the `Data` folder of a 3.3.5a client. Do not pick `Launcher.exe`,
`Wow-64.exe`, or a `Wow.exe` from a different expansion. If the client lives in
`Program Files`, move it somewhere your user can write to, such as
`C:\Games\WoW335`, because the launcher has to write
`WTF\realmlist.wtf`.

**Status says Unreachable.** The address or port is wrong, or the server is
down. Check the address with the server's admin.

**Windows says the installer is unsafe.** See step 2 of Install. The warning
appears because the installer is unsigned. The release page lists a SHA256
checksum for the file if you want to check it yourself.

---

# For developers

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

### Updates

The launcher checks GitHub for a newer release at startup. If it finds one, it
downloads the installer, checks it against the `.sha256` the release publishes,
runs it, and closes itself. The installer cannot replace files the app holds
open, so the app has to exit for the update to apply.

**The checksum step is not optional.** Without it the launcher would run an
executable it fetched over the network on the strength of a URL. A file that
fails the check is deleted and never runs.

Version comparison is numeric per component, so `2026.8.10` is newer than
`2026.8.9`; a string compare gets that backwards.

A build reporting `0.0.0-dev` never updates, because that string does not parse
as a version and nothing is ever newer than an unparseable one. A build
reporting plain `0.0.0`, which is what `msbuild` stamps without `AcVersion`,
does parse and **is** older than any release, so it will update itself. If you
want a local build to leave itself alone, give it the current release version:
`/p:AcVersion=<latest tag>`.

A failed check is silent. The launcher must still start when GitHub is
unreachable.

### Artwork

`Assets/` holds original art made for this project (hero, wordmark, icon). The
build still works without it, with no art and no build error, so both CI
workflows check that the four files exist.

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

Both build workflows check the artwork before they compile. A CI installer and
a local installer contain the same files.

## Licence

MIT. See [LICENSE](LICENSE).

