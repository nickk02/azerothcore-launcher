# Assets

This directory holds Blizzard-owned art assets used by the launcher's UI and
app icon. They are gitignored (see `.gitignore`) and are never committed or
redistributed. A fresh clone will not have them.

Before doing a full build with the real icon and hero art, drop these files
in locally:

- `wotlk-hero.png`
- `wotlk-icon.png`
- `wotlk-icon.ico`
- `wotlk-logo.png`

Without `wotlk-icon.ico` present, `app.rc`'s resource-compile step is skipped
(see the `Condition="Exists('Assets\wotlk-icon.ico')"` on the
`ResourceCompile` item in `azerothcore.vcxproj`), so the project still builds
cleanly, just without a custom app icon.
