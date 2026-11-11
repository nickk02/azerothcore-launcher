# AzerothCore Launcher

A small launcher for Windows and macOS for World of Warcraft 3.3.5a (Wrath of the Lich King)
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
   administrator rights.
4. Copy your 3.3.5a client into the `Client\` folder that Setup opens at the
   end, or keep it where it is and point the launcher at it in step 5.
5. Start the launcher. Click **Browse** and pick `Wow.exe` from your client
   folder.

## macOS

Download `AzerothCore.dmg` from [Releases](../../releases) and drag the app to
Applications. The app is not notarized, so on first launch right-click it and
choose **Open**. It needs macOS 11 or later.

The 3.3.5a client is a Windows program, so the Mac app starts it through
Wine. Install Wine (for example `brew install --cask --no-quarantine wine-stable`)
or CrossOver first. Pick `Wow.exe` with **Browse**. The app does not type your
login into the game; it only remembers it in the Keychain and fills its own
fields.

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

## Playing on a realm behind a Cloudflare Tunnel

If the realm's admin hosts it through a Cloudflare Tunnel (no public IP or VPS),
they give you an address that starts with `wss://`, for example
`wss://play.example.com`. Type it into **Realm address** like any other. The
launcher then listens on `127.0.0.1` ports 3724 and 8085 and carries the game's
traffic to the realm for you. Nothing else needs installing, and no Cloudflare
account is needed. Keep those two ports free, and do not run a local server on
them.

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

The launcher is one Rust crate for Windows and macOS. `src/lib.rs` holds the
logic (settings, realm check, realmlist, launch, updates, credentials) and
`src/main.rs` holds the window, built with egui.

## Build and test

You need [Rust](https://rustup.rs) (stable).

```
cargo test --workspace
cargo run --release
```

The Windows exe is statically linked, so it needs no runtime. Windows builds
use the MSVC toolchain. To build the macOS app and DMG, run
`scripts/build-macos.sh` on a Mac. The Windows installer is built from
`installer/installer.iss` with Inno Setup.

### Hosting without a VPS (gateway)

Cloudflare's edge carries only HTTP(S) for a public hostname, and the WoW client
speaks plain TCP. The `gateway/` crate bridges the two: it accepts WebSocket
connections and forwards each one to the login or world server, byte for byte.

```
azerothcore-gateway --listen 127.0.0.1:8080 --auth 127.0.0.1:3724 --world 127.0.0.1:8085
```

Then point a Cloudflare Tunnel published application at it. Use one hostname, for
example `play.example.com`, with service `http://localhost:8080`. The gateway
serves `/auth` and `/world`. On the AzerothCore auth database, the realm must
point at the player's local proxy:

```sql
UPDATE realmlist SET address = '127.0.0.1', localAddress = '127.0.0.1', port = 8085 WHERE id = 1;
```

Players use `wss://play.example.com` as their realm address. The launcher starts a
local proxy on 127.0.0.1:3724 and :8085, wraps each game connection in a
WebSocket to `/auth` or `/world`, and writes `127.0.0.1` into the realmlist. It
only works through this launcher, not the stock client. Expect a little extra
latency from the extra hop. The proxy pings every 25 seconds so Cloudflare does
not drop idle connections.

`cargo test --workspace` covers this on loopback: a mock game server, the gateway,
the launcher proxy and a TCP client, including a 2 MB stream, 40 concurrent
connections, close propagation both ways, and the `wss://` TLS path with a
throwaway certificate. It does not cover Cloudflare itself.

### Version numbers

The version has one source: the git tag. Set `AC_VERSION` when you build a
release:

```
AC_VERSION=2026.08.06 cargo build --release
```

The build stamps it into the Windows exe resources and the app reads it back at
runtime. Without `AC_VERSION` the app reports `0.0.0-dev`.

### Updates

The launcher checks GitHub for a newer release at startup. If it finds one, it
downloads the installer, checks it against the `.sha256` the release publishes,
and runs it. On Windows the launcher then closes itself, because the installer
cannot replace a running exe. On macOS it opens the DMG.

**The checksum step is not optional.** A file that fails the check never runs.

Version comparison is numeric per component, so `2026.8.10` is newer than
`2026.8.9`. A build reporting `0.0.0-dev` never updates, because that string
does not parse as a version.

A failed check is silent. The launcher must still start when GitHub is
unreachable.

### Credentials

"Remember me" stores the login in the Windows Credential Manager or the macOS
Keychain. On Windows the launcher also types it into the game's login screen
after Play. Logins saved by the old Windows app are not carried over; enter
them once more.

### Artwork

`assets/` holds original art made for this project (hero, wordmark, icon). It
is compiled into the executable.

## Continuous integration

| Workflow | Runs on | Steps |
|---|---|---|
| `build.yml` | Push or PR to `main` | Test and build on Windows and macOS, upload the output |
| `release.yml` | Tag `v*` | Test, build with the version from the tag, build the installer and DMG, attach them with SHA256 files |

## Licence

MIT. See [LICENSE](LICENSE).
