# Assets

Blizzard-owned art used by the launcher's UI and application icon:

- `wotlk-hero.png` - the hero image behind the panel
- `wotlk-logo.png` - the Wrath of the Lich King wordmark
- `wotlk-icon.png` / `wotlk-icon.ico` - the application icon

These are tracked in the repository. They were previously gitignored under a
"never redistributed" rule, which stopped being accurate once the installer
that embeds the very same art began shipping as a public release asset. Keeping
the sources out only prevented CI from building the real product while changing
nothing about what was actually published.

The build still tolerates their absence: `Assets` is a wildcard content item
and the icon resource is conditional on `wotlk-icon.ico` existing, so removing
them yields a working but art-free launcher rather than a build error. CI checks
for them explicitly, because that tolerance would otherwise hide a mistake.
