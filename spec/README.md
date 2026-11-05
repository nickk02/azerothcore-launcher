# Launcher behaviour spec

This folder is the contract between the Windows and macOS launchers. Both
apps must behave as written here. Each app has its own native UI and its own
implementation; they stay in line through this spec and the test vectors in
`vectors/`. A change to behaviour starts here, and both apps follow.

## Settings

A plain text file of `key=value` lines. A line with no `=` is ignored. The
key is the text before the first `=`. Unknown keys are ignored. Missing
keys take the default.

| Key | Default | Meaning |
| --- | --- | --- |
| `WowPath` | empty | Full path to `Wow.exe` |
| `RealmAddress` | empty | `host` or `host:port` |
| `CredentialVaultEnabled` | `0` | `1` means "Remember me" is on. Any other value means off. |

Save writes all three keys, in this order, each followed by a newline.

| Platform | File |
| --- | --- |
| Windows | `%APPDATA%\AzerothCore\settings.ini` |
| macOS | `~/Library/Application Support/AzerothCore/settings.ini` |

Reference: `vectors/settings.ini`.

## Realm address

Split at the first `:`. With no colon, the port is 3724. If the text after
the colon is not a number, the port is 3724. Vectors: `vectors/addresses.json`.

## Realm status

An empty address is **Unconfigured**. Otherwise open a TCP connection to
host and port. A completed connection is **Online**. A refusal, an error, or
no answer within 5 seconds is **Unreachable**. Close the connection at once.
No data is sent.

## Play

1. `Wow.exe` must exist, be a regular file, and be named `Wow.exe`
   (case-insensitive). Otherwise Play fails.
2. If the realm address is not empty, write `set realmlist <address>` plus a
   newline to `<wow folder>/WTF/realmlist.wtf`. Create `WTF` if needed.
   Replace the file. Reference: `vectors/realmlist.wtf`.
3. Write the same line to `realmlist.wtf` in every direct subfolder of
   `<wow folder>/Data` (the locale folders). Ignore errors here.
4. Start `Wow.exe` with the wow folder as its working directory.

On macOS, step 4 starts the game through a Wine-compatible runner. That is
the only step that differs.

## Playtime

Read the first integer in `<wow folder>/WTF/azerothcore_playtime.txt`
(seconds). If the file is missing, the value is 0. Format as whole hours and
remaining whole minutes. Vectors: `vectors/playtime.json`.

## Saved login

With "Remember me" on, the account name and password are kept in the
platform's secure store (Windows Credential Manager, macOS Keychain). They
are never written to `settings.ini`. With it off, nothing is kept and any
stored entry is removed.

## Updates

1. Request `https://api.github.com/repos/nickk02/azerothcore-launcher/releases/latest`
   with a `User-Agent` header.
2. Remove one leading `v` from `tag_name`. The rest is the latest version.
3. The release needs both assets for the platform. If either is missing, the
   result is an error and no update is offered.

   | Platform | Installer asset | Checksum asset |
   | --- | --- | --- |
   | Windows | `AzerothCoreSetup.exe` | `AzerothCoreSetup.exe.sha256` |
   | macOS | `AzerothCore.dmg` | `AzerothCore.dmg.sha256` |

4. An update is available only if the latest version is newer than the
   running version. Vectors: `vectors/versions.json`.
5. To install: download the checksum file (first word is 64 hex digits,
   compare in lower case). Download the installer. Compare its SHA-256. On
   any mismatch, or if the checksum is not 64 characters, discard the file
   and fail. Write the file to disk only after the hash matches.

A build without a release version reports `0.0.0` and is never offered an
update as "newer than a release".

## Not in scope here

Layout, controls, fonts, window chrome, and menus follow each platform's own
guidelines. They are not part of this spec.
