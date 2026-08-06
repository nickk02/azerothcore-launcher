# Assets

Blizzard-owned art used by the launcher:

- `wotlk-hero.png` - the hero image behind the panel
- `wotlk-logo.png` - the Wrath of the Lich King wordmark
- `wotlk-icon.png` and `wotlk-icon.ico` - the application icon

These files are in the repository. Earlier they were gitignored under a rule
that said the project never redistributes them. That rule stopped being true
when the installer became a public release asset, because the installer
contains the same art. Excluding the source files changed nothing about what
the project published. It only stopped CI from building the real product.

The build still works if you delete these files. `Assets` is a wildcard content
item, and the icon resource compiles only when `wotlk-icon.ico` is present. You
get a launcher with no art and no build error. Both CI workflows check for the
four files, because a missing file is otherwise silent.
