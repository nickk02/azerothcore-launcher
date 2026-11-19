use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

pub fn is_valid_wow_exe(path: &Path) -> bool {
    path.is_file()
        && path.file_name().map(|n| n.to_string_lossy().eq_ignore_ascii_case("wow.exe")).unwrap_or(false)
}

/// Writes `WTF/realmlist.wtf` and a copy in each `Data/<locale>/` folder.
pub fn write_realmlist(wow_folder: &Path, address: &str) -> std::io::Result<()> {
    let line = format!("set realmlist {address}\n");
    let wtf = wow_folder.join("WTF");
    fs::create_dir_all(&wtf)?;
    fs::write(wtf.join("realmlist.wtf"), &line)?;
    if let Ok(entries) = fs::read_dir(wow_folder.join("Data")) {
        for entry in entries.flatten() {
            if entry.path().is_dir() {
                let _ = fs::write(entry.path().join("realmlist.wtf"), &line);
            }
        }
    }
    Ok(())
}

/// A Wine build the launcher can run the client with.
#[cfg(not(windows))]
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Engine {
    pub name: &'static str,
    pub bin: PathBuf,
}

#[cfg(not(windows))]
pub const NO_WINE_MESSAGE: &str = "Wine was not found. Install it with: brew tap gcenx/wine && brew install --cask gcenx/wine/wine-stable. CrossOver, Whisky and Sikarugir also work. Then click Play again.";

/// Known Wine locations, best first. A Wine from Homebrew or a Gcenx app comes
/// before CrossOver, because CrossOver's own `wine` script wants a CrossOver bottle.
#[cfg(not(windows))]
fn engine_candidates(home: &Path) -> Vec<Engine> {
    let app = |name: &'static str, rel: &str| Engine { name, bin: PathBuf::from(rel) };
    let mut v = vec![
        app("Wine (Homebrew)", "/opt/homebrew/bin/wine"),
        app("Wine (Homebrew)", "/opt/homebrew/bin/wine64"),
        app("Wine (Homebrew)", "/usr/local/bin/wine"),
        app("Wine (Homebrew)", "/usr/local/bin/wine64"),
    ];
    for dir in ["/Applications", &home.join("Applications").display().to_string()] {
        for bundle in ["Wine Stable", "Wine Staging", "Wine Devel"] {
            v.push(Engine { name: "Wine (app)", bin: PathBuf::from(format!("{dir}/{bundle}.app/Contents/Resources/wine/bin/wine")) });
        }
    }
    // Whisky keeps its Wine build in the user's library.
    v.push(Engine { name: "Whisky", bin: home.join("Library/Application Support/com.isaacmarovitz.Whisky/Libraries/Wine/bin/wine64") });
    v.push(app("CrossOver", "/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine"));
    v
}

#[cfg(not(windows))]
fn pick_engine(candidates: Vec<Engine>, exists: impl Fn(&Path) -> bool) -> Option<Engine> {
    candidates.into_iter().find(|e| exists(&e.bin))
}

/// Looks for an installed Wine, CrossOver or a wrapper that carries its own Wine.
#[cfg(not(windows))]
pub fn find_wine() -> Option<Engine> {
    let home = dirs::home_dir().unwrap_or_default();
    let mut found = pick_engine(engine_candidates(&home), |p| p.is_file());
    if found.is_none() {
        // Sikarugir wrappers each carry an engine inside the wrapper app.
        for dir in [PathBuf::from("/Applications/Sikarugir"), home.join("Applications/Sikarugir")] {
            let Ok(entries) = fs::read_dir(dir) else { continue };
            for entry in entries.flatten() {
                let bin = entry.path().join("Contents/SharedSupport/wine/bin/wine64");
                if bin.is_file() {
                    return Some(Engine { name: "Sikarugir", bin });
                }
            }
        }
    }
    found.take()
}

/// The launcher's own prefix, so it never touches `~/.wine` or another app's bottle.
#[cfg(not(windows))]
pub fn prefix_dir() -> PathBuf {
    dirs::config_dir().unwrap_or_else(|| PathBuf::from(".")).join("AzerothCore").join("prefix")
}

#[cfg(not(windows))]
pub fn prefix_ready() -> bool {
    prefix_dir().join("system.reg").is_file()
}

/// Windows needs no prefix.
#[cfg(windows)]
pub fn prefix_ready() -> bool {
    true
}

/// Wine environment shared by prefix setup and the game. Wine Mono and Gecko are
/// off because the client needs neither and their download prompt blocks setup.
#[cfg(not(windows))]
fn wine_command(engine: &Engine) -> Command {
    let mut c = Command::new(&engine.bin);
    c.env("WINEPREFIX", prefix_dir())
        .env("WINEDLLOVERRIDES", "mscoree,mshtml=")
        .env("WINEDEBUG", "-all");
    c
}

/// Creates the prefix with `wineboot`. The first run takes 20 to 60 seconds.
#[cfg(not(windows))]
pub fn setup_prefix(engine: &Engine) -> Result<(), String> {
    use std::time::{Duration, Instant};
    let dir = prefix_dir();
    fs::create_dir_all(&dir).map_err(|e| format!("Could not create the Wine prefix at {}: {e}", dir.display()))?;
    let mut child = wine_command(engine)
        .args(["wineboot", "--init"])
        .stdout(std::process::Stdio::null())
        .stderr(std::process::Stdio::null())
        .spawn()
        .map_err(|e| format!("Could not start {}: {e}", engine.name))?;
    let start = Instant::now();
    loop {
        match child.try_wait() {
            Ok(Some(_)) => break,
            Ok(None) if start.elapsed() > Duration::from_secs(180) => {
                let _ = child.kill();
                return Err("Wine took longer than 3 minutes to set up its prefix. Quit Wine and try again.".into());
            }
            Ok(None) => std::thread::sleep(Duration::from_millis(250)),
            Err(e) => return Err(format!("Wine setup failed: {e}")),
        }
    }
    // wineboot can exit before wineserver has written the registry, so wait for it.
    let wait = Instant::now();
    while !(prefix_ready() && dir.join("user.reg").is_file()) && wait.elapsed() < Duration::from_secs(60) {
        std::thread::sleep(Duration::from_millis(250));
    }
    if prefix_ready() { Ok(()) } else { Err(format!("{} did not create a prefix at {}. Run it from Terminal to see why.", engine.name, dir.display())) }
}

pub fn launch(wow_path: &str, realm_address: &str) -> Result<(), String> {
    let exe = PathBuf::from(wow_path);
    if !is_valid_wow_exe(&exe) {
        return Err("That isn't a 3.3.5a Wow.exe.".into());
    }
    let folder = exe.parent().unwrap_or(Path::new(".")).to_path_buf();
    if !realm_address.is_empty() {
        write_realmlist(&folder, realm_address).map_err(|e| format!("Could not write realmlist: {e}"))?;
    }

    #[cfg(windows)]
    let mut cmd = Command::new(&exe);
    #[cfg(not(windows))]
    let mut cmd = {
        let engine = find_wine().ok_or(NO_WINE_MESSAGE)?;
        if !prefix_ready() {
            setup_prefix(&engine)?;
        }
        let mut c = wine_command(&engine);
        c.arg(exe.file_name().unwrap());
        c
    };
    cmd.current_dir(&folder).spawn().map(|_| ()).map_err(|e| format!("Failed to launch WoW: {e}"))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn writes_realmlist_everywhere() {
        let dir = tempfile::tempdir().unwrap();
        fs::create_dir_all(dir.path().join("Data/enUS")).unwrap();
        write_realmlist(dir.path(), "logon.example.com").unwrap();
        let main = fs::read_to_string(dir.path().join("WTF/realmlist.wtf")).unwrap();
        let locale = fs::read_to_string(dir.path().join("Data/enUS/realmlist.wtf")).unwrap();
        assert_eq!(main, "set realmlist logon.example.com\n");
        assert_eq!(locale, main);
    }

    #[test]
    fn validates_wow_exe() {
        let dir = tempfile::tempdir().unwrap();
        let good = dir.path().join("WOW.EXE");
        let bad = dir.path().join("Other.exe");
        fs::write(&good, b"").unwrap();
        fs::write(&bad, b"").unwrap();
        assert!(is_valid_wow_exe(&good));
        assert!(!is_valid_wow_exe(&bad));
        assert!(!is_valid_wow_exe(dir.path()));
        assert!(!is_valid_wow_exe(&dir.path().join("missing.exe")));
    }

    #[cfg(not(windows))]
    #[test]
    fn picks_first_existing_engine() {
        let home = Path::new("/Users/x");
        let have = |p: &Path| p == Path::new("/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine") || p == Path::new("/Applications/Wine Stable.app/Contents/Resources/wine/bin/wine");
        let e = pick_engine(engine_candidates(home), have).unwrap();
        assert_eq!(e.name, "Wine (app)");
        let only_cx = |p: &Path| p.to_string_lossy().contains("CrossOver.app");
        assert_eq!(pick_engine(engine_candidates(home), only_cx).unwrap().name, "CrossOver");
        assert!(pick_engine(engine_candidates(home), |_| false).is_none());
    }
}
