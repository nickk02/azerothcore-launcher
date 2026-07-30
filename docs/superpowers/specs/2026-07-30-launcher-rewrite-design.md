# AzerothCore Launcher — ground-up rewrite design

## Context

`nickk02/azerothcore-launcher` is currently a GitHub fork of `wallski/wallmane` with a renamed/rebranded codebase (namespace, config paths, configurable realm address). Nick wants both the GitHub fork relationship and the underlying implementation gone — a genuinely independent project, not a relabeled derivative. The old code is preserved locally at `wallmane-reference/` for reference and is not reused; nothing in the rewrite is copied or adapted from it.

The old fork will be deleted on GitHub (Nick's own action, not automatable here — deleting a repo is outside what this agent will do regardless of authorization). A fresh, non-fork repo replaces it under the same name.

## Decisions locked in this pass

- **Tech stack:** WinUI 3 / C++ / XAML, unchanged from the old project. The headless build pipeline (MSBuild + Inno Setup) is already proven and stays.
- **Scope:** full feature parity with the old app's feature set (launch, addon browsing, character armory, settings) plus the new credential-passthrough feature, in one pass rather than an MVP-first ramp.
- **Architecture:** `Frame` + page-per-section navigation (`HomePage`, `AddonsPage`, `CharactersPage`, `SettingsPage`, each its own file pair), replacing the old single-`MainWindow` design where every page was a sibling panel manually toggled and all logic lived in one large code-behind file. Plain code-behind per page — no full MVVM/data-binding layer, which would be real infrastructure cost with no one to benefit from the added testability on a single-developer app with no test suite planned for the UI layer itself.
- **Visual direction:** period-accurate 2008 Blizzard WotLK launcher aesthetic — the actual local WotLK box art and logo (`Assets/wotlk-*.png`, gitignored, never committed to the public repo, same policy as the old project), hero-art-forward layout, no news panel, plain/flat modern window chrome (not skeuomorphic Windows-XP chrome, not a glowing custom "fantasy UI" look). Reference material: `wow-launcher.PNG` (screenshot of the real client-era launcher) and the real `Launcher.exe`, both supplied locally and not committed to the repo.

## Core flow

1. User points the launcher at their WoW install once, via Settings (browse to `Wow.exe`). No hunting for the folder on every launch — the path persists.
2. Play launches the game with `realmlist.wtf` already written from the configured realm address.
3. **Login is native to the game client, not the launcher.** The launcher has no account-authentication UI of its own. Its only job re: credentials is autofill: on launch, if the user has opted in, it pulls the stored account name/password from Windows Credential Vault and simulates entering them on the game client's own login screen. Credentials are stored locally via Credential Vault only — never transmitted to any server, never written to disk in plaintext, opt-in.

## Addons

- The "Addons" nav button opens the in-app addon browser (search/install) by default, not a raw folder. A folder-open shortcut lives inside that page for anyone who wants to manage addons by hand.
- Source: Felbite, via the existing scrape-based `IAddonSource` implementation (HTML fetch against `felbite.com`'s search page + regex extraction — no official API, no contract). Chosen because it catalogs 3.3.5a-specific addons, which a general-purpose catalog does not guarantee.
- CurseForge is added later as a second `IAddonSource` implementation, using their official API, once Nick registers his own free developer key at `console.curseforge.com` (account creation is not something this agent does on his behalf). Not blocking for the initial rewrite.
- **Felbite scraper health check:** because the Felbite integration has no API contract, it will fail silently the moment `felbite.com` changes its page structure, degrading to empty search results with no visible error. Concretely: a small standalone script (not part of the AzerothCore C++ gtest suite — this is launcher-specific) that runs the same search + regex-extraction logic against a known query, asserts it gets a non-empty, well-formed result, and fails loudly if not. Wired into a scheduled GitHub Actions workflow on `nickk02/azerothcore-launcher` (daily cadence is enough) so breakage surfaces as a failed CI run Nick sees, not as a silent empty addon list a user reports weeks later.

## Components

- `App.xaml/.cpp/.h` — app entry, same role as before.
- `MainWindow.xaml/.cpp/.h` — thin shell: custom titlebar, nav rail, hosts a `Frame` for page content.
- `Pages/HomePage` — hero art, realm status, Play button. No news feed (explicitly cut).
- `Pages/AddonsPage` — Felbite (later +CurseForge) search/install via `IAddonSource`, plus the "open addons folder" shortcut.
- `Pages/CharactersPage` — armory view (WebView2 3D model + stat panel).
- `Pages/SettingsPage` — WoW path browse, realm address, credential vault opt-in/clear, cache clear.
- `Core/WowInstall` — detect/validate the WoW folder, launch the process, write `realmlist.wtf`, track playtime.
- `Core/RealmConfig` — settings persistence under `%APPDATA%\AzerothCore\`.
- `Core/RealmStatusChecker` — TCP reachability check for the configured realm, pulled out as its own reusable service (was inline in the old `MainWindow.xaml.cpp`).
- `Core/AddonCatalog` + `Core/IAddonSource` implementations (`FelbiteSource`, later `CurseForgeSource`) — carried forward from the old project's already-pluggable design, reimplemented fresh.
- `Core/ArmoryClient` — character data fetch for the Characters page.
- `Core/CredentialVault` — new. Windows Credential Vault read/write, simulated autofill on the WoW client's login screen.

## Data flow

App launch loads `RealmConfig` from disk, then navigates to `HomePage`. Each page kicks off its own async work on `OnNavigatedTo` rather than one `LoadDataAsync()` doing everything up front: Home checks realm status and playtime, Addons performs an initial catalog search, Characters fetches armory data. Settings two-way binds to `RealmConfig` and persists on save. Play calls `WowInstall::LaunchWow`, then `CredentialVault::AutofillLogin` if credential passthrough is enabled.

## Error handling

- Network calls (addon search, armory, realm status) degrade to a visible placeholder/error state rather than crash — same pattern as the old app, appropriate for non-critical desktop-app features.
- WoW launch failures (bad path, missing exe) get a visible error, never silent.
- Credential vault failures (access denied, no stored credential) show an explicit "not signed in" state — never silently proceed as if autofill worked.
- Felbite scraper failures surface as a visible "addon search unavailable" state in `AddonsPage`, not an empty-looking result list that reads as "no addons found."

## Testing

No automated UI test framework (WinAppDriver etc.) — deliberate scope call for a single-developer app with no existing test infra. Manual smoke-test checklist instead: build → launch → browse to WoW folder → set realm address → launch game → confirm `realmlist.wtf` written and credential autofill (if enabled) → search/install an addon → check the armory view. The Felbite health check (above) is the one piece of automated verification in scope, since it guards against silent breakage of an external, contract-free dependency rather than testing our own code.

## Out of scope for this pass

- CurseForge integration itself (interface point is designed for it, implementation waits on Nick's API key).
- Any actual redistribution of the WoW client — the installer creates an empty `Client\` folder with instructions; bundling Blizzard's copyrighted client binary is not something this project does, consistent with earlier decisions this session (the torrent/magnet-link feature was removed from the old fork for the same reason).
