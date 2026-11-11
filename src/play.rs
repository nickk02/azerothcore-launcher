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

#[cfg(not(windows))]
fn find_wine() -> Option<PathBuf> {
    [
        "/opt/homebrew/bin/wine64",
        "/opt/homebrew/bin/wine",
        "/usr/local/bin/wine64",
        "/usr/local/bin/wine",
        "/usr/bin/wine",
        "/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine",
    ]
    .iter()
    .map(PathBuf::from)
    .find(|p| p.is_file())
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
        let wine = find_wine().ok_or("Wine was not found. Install it (for example with Homebrew) or CrossOver, then try again.")?;
        let mut c = Command::new(wine);
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
}
