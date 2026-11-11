use serde::Deserialize;
use sha2::{Digest, Sha256};
use std::io::Read;
use std::path::PathBuf;

const API: &str = "https://api.github.com/repos/nickk02/azerothcore-launcher/releases/latest";

#[cfg(windows)]
const INSTALLER: &str = "AzerothCoreSetup.exe";
#[cfg(not(windows))]
const INSTALLER: &str = "AzerothCore.dmg";

pub const CURRENT_VERSION: &str = env!("CARGO_PKG_VERSION");

#[derive(Debug, Default, Clone)]
pub struct UpdateInfo {
    pub latest: String,
    pub installer_url: Option<String>,
    pub checksum_url: Option<String>,
    pub available: bool,
}

/// Numeric compare. A missing component is 0. Any non-integer part gives false.
pub fn is_newer(candidate: &str, current: &str) -> bool {
    fn parts(text: &str) -> Option<Vec<u64>> {
        text.split('.')
            .map(|p| if !p.is_empty() && p.bytes().all(|b| b.is_ascii_digit()) { p.parse().ok() } else { None })
            .collect()
    }
    let (Some(a), Some(b)) = (parts(candidate), parts(current)) else { return false };
    for i in 0..a.len().max(b.len()) {
        let (l, r) = (a.get(i).copied().unwrap_or(0), b.get(i).copied().unwrap_or(0));
        if l != r {
            return l > r;
        }
    }
    false
}

#[derive(Deserialize)]
struct Release {
    tag_name: String,
    assets: Vec<Asset>,
}
#[derive(Deserialize)]
struct Asset {
    name: String,
    browser_download_url: String,
}

pub fn from_release(json: &str, current: &str) -> Option<UpdateInfo> {
    let release: Release = serde_json::from_str(json).ok()?;
    let tag = release.tag_name.trim_start_matches(['v', 'V']).to_string();
    let find = |name: String| release.assets.iter().find(|a| a.name == name).map(|a| a.browser_download_url.clone());
    let mut info = UpdateInfo {
        installer_url: find(INSTALLER.to_string()),
        checksum_url: find(format!("{INSTALLER}.sha256")),
        ..Default::default()
    };
    // Both assets are required, so an update is never installed unverified.
    info.available = info.installer_url.is_some() && info.checksum_url.is_some() && is_newer(&tag, current);
    info.latest = tag;
    Some(info)
}

pub fn check() -> UpdateInfo {
    let body = ureq::get(API).set("User-Agent", "azerothcore-launcher").call().ok().and_then(|r| r.into_string().ok());
    body.and_then(|b| from_release(&b, CURRENT_VERSION)).unwrap_or_default()
}

pub fn verify(bytes: &[u8], checksum_file: &str) -> Result<(), String> {
    let expected = checksum_file.split_whitespace().next().unwrap_or("").to_lowercase();
    if expected.len() != 64 {
        return Err("The checksum file is not valid.".into());
    }
    let actual: String = Sha256::digest(bytes).iter().map(|b| format!("{b:02x}")).collect();
    if actual == expected { Ok(()) } else { Err("The download did not match its checksum.".into()) }
}

fn download(url: &str) -> Result<Vec<u8>, String> {
    let mut buf = Vec::new();
    ureq::get(url)
        .set("User-Agent", "azerothcore-launcher")
        .call()
        .map_err(|e| format!("Update failed: {e}"))?
        .into_reader()
        .read_to_end(&mut buf)
        .map_err(|e| format!("Update failed: {e}"))?;
    Ok(buf)
}

/// Downloads the installer, checks its SHA-256, and opens it.
pub fn install(info: &UpdateInfo) -> Result<(), String> {
    let (Some(installer), Some(checksum)) = (&info.installer_url, &info.checksum_url) else {
        return Err("The release has no installer.".into());
    };
    let sum = String::from_utf8_lossy(&download(checksum)?).into_owned();
    let bytes = download(installer)?;
    verify(&bytes, &sum)?;

    // Written only after the hash matches.
    let ext = if cfg!(windows) { "exe" } else { "dmg" };
    let target: PathBuf = std::env::temp_dir().join(format!("AzerothCore-{}.{ext}", info.latest));
    let _ = std::fs::remove_file(&target);
    std::fs::write(&target, bytes).map_err(|e| format!("Update failed: {e}"))?;
    open_file(&target)
}

fn open_file(path: &std::path::Path) -> Result<(), String> {
    let (program, args): (&str, Vec<&std::ffi::OsStr>) = if cfg!(windows) {
        ("cmd", vec!["/C".as_ref(), "start".as_ref(), "".as_ref(), path.as_os_str()])
    } else if cfg!(target_os = "macos") {
        ("open", vec![path.as_os_str()])
    } else {
        ("xdg-open", vec![path.as_os_str()])
    };
    std::process::Command::new(program).args(args).spawn().map(|_| ()).map_err(|e| format!("Update failed: {e}"))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn is_newer_matches_native_apps() {
        assert!(is_newer("2026.08.07", "2026.08.06"));
        assert!(!is_newer("2026.08.06", "2026.08.06"));
        assert!(is_newer("2026.8.10", "2026.8.9"));
        assert!(!is_newer("2026.8", "2026.8.0"));
        assert!(is_newer("2026.8.1", "2026.8"));
        assert!(!is_newer("2026.08.06", "0.0.0-dev"));
        assert!(!is_newer("", "2026.08.06"));
        assert!(!is_newer("v2026.08.07", "2026.08.06"));
        assert!(is_newer("2026.08.06", "0.0.0"));
    }

    #[test]
    fn needs_both_assets() {
        let name = INSTALLER;
        let full = format!(r#"{{"tag_name":"v2026.9.1","assets":[{{"name":"{name}","browser_download_url":"u1"}},{{"name":"{name}.sha256","browser_download_url":"u2"}}]}}"#);
        let info = from_release(&full, "2026.8.1").unwrap();
        assert!(info.available);
        assert_eq!(info.latest, "2026.9.1");

        let partial = format!(r#"{{"tag_name":"v2026.9.1","assets":[{{"name":"{name}","browser_download_url":"u1"}}]}}"#);
        assert!(!from_release(&partial, "2026.8.1").unwrap().available);
    }

    #[test]
    fn verifies_checksum() {
        let good = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad  AzerothCore.dmg\n";
        assert!(verify(b"abc", good).is_ok());
        assert!(verify(b"abd", good).is_err());
        assert!(verify(b"abc", "short").is_err());
    }
}
