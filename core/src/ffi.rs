//! Swift-facing API, generated with UniFFI. Thin wrappers only: the logic lives in the
//! modules next to this one so the egui app and the Swift app behave the same.
use crate::{config, creds, play, realm, session, update};
use std::sync::Arc;

#[derive(uniffi::Record, Clone, Debug, Default)]
pub struct Settings {
    pub wow_path: String,
    pub realm_address: String,
    pub remember_login: bool,
}

#[derive(uniffi::Record, Clone, Debug)]
pub struct Login {
    pub account: String,
    pub password: String,
}

#[derive(uniffi::Enum, Clone, Copy, Debug, PartialEq, Eq)]
pub enum RealmStatus {
    Unconfigured,
    Online,
    Unreachable,
}

#[derive(uniffi::Record, Clone, Debug, Default)]
pub struct UpdateStatus {
    pub latest: String,
    pub available: bool,
    pub installer_url: Option<String>,
    pub checksum_url: Option<String>,
}

#[derive(uniffi::Record, Clone, Debug)]
pub struct WineStatus {
    /// Name of the Wine build that will run the game, or None when nothing is installed.
    pub engine: Option<String>,
    pub prefix_ready: bool,
    /// What to tell the player when `engine` is None.
    pub help: String,
}

#[derive(Debug, uniffi::Error)]
pub enum LauncherError {
    Message { text: String },
}

impl std::fmt::Display for LauncherError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let Self::Message { text } = self;
        f.write_str(text)
    }
}

impl From<String> for LauncherError {
    fn from(text: String) -> Self {
        Self::Message { text }
    }
}

#[uniffi::export]
pub fn current_version() -> String {
    update::CURRENT_VERSION.to_string()
}

#[uniffi::export]
pub fn load_settings() -> Settings {
    let c = config::Config::load();
    Settings { wow_path: c.wow_path, realm_address: c.realm_address, remember_login: c.remember_login }
}

#[uniffi::export]
pub fn save_settings(settings: Settings) -> Result<(), LauncherError> {
    let c = config::Config { wow_path: settings.wow_path, realm_address: settings.realm_address, remember_login: settings.remember_login };
    c.save().map_err(|e| LauncherError::from(format!("Failed to save settings: {e}")))
}

/// Blocks for up to 5 seconds. Call it off the main thread.
#[uniffi::export]
pub fn check_realm(address: String) -> RealmStatus {
    match realm::check(&address) {
        realm::Reachability::Unconfigured => RealmStatus::Unconfigured,
        realm::Reachability::Online => RealmStatus::Online,
        realm::Reachability::Unreachable => RealmStatus::Unreachable,
    }
}

#[uniffi::export]
pub fn is_valid_wow_exe(path: String) -> bool {
    play::is_valid_wow_exe(std::path::Path::new(&path))
}

#[uniffi::export]
pub fn store_login(account: String, password: String) -> Result<(), LauncherError> {
    creds::store(&account, &password).map_err(|e| LauncherError::from(format!("Could not save to the Keychain: {e}")))
}

#[uniffi::export]
pub fn saved_login() -> Option<Login> {
    creds::get().map(|(account, password)| Login { account, password })
}

#[uniffi::export]
pub fn clear_login() {
    creds::clear();
}

/// Blocks on the network. Call it off the main thread.
#[uniffi::export]
pub fn check_update() -> UpdateStatus {
    let u = update::check();
    UpdateStatus { latest: u.latest, available: u.available, installer_url: u.installer_url, checksum_url: u.checksum_url }
}

/// Downloads, verifies the SHA256 and opens the installer. Call it off the main thread.
#[uniffi::export]
pub fn install_update(info: UpdateStatus) -> Result<(), LauncherError> {
    let u = update::UpdateInfo { latest: info.latest, installer_url: info.installer_url, checksum_url: info.checksum_url, available: info.available };
    update::install(&u).map_err(LauncherError::from)
}

/// Folder a player can drop `hero.png`, `logo.png` and `icon.png` into to replace the art.
#[uniffi::export]
pub fn art_dir() -> String {
    config::art_dir().display().to_string()
}

#[uniffi::export]
pub fn wine_status() -> WineStatus {
    #[cfg(not(windows))]
    {
        let engine = play::find_wine();
        let help = if engine.is_some() { String::new() } else { play::NO_WINE_MESSAGE.to_string() };
        WineStatus { engine: engine.map(|e| e.name.to_string()), prefix_ready: play::prefix_ready(), help }
    }
    #[cfg(windows)]
    WineStatus { engine: Some("Windows".into()), prefix_ready: true, help: String::new() }
}

/// Owns the local proxy. Keep one for the life of the app.
#[derive(uniffi::Object)]
pub struct Launcher {
    session: session::Session,
}

#[uniffi::export]
impl Launcher {
    #[uniffi::constructor]
    pub fn new() -> Arc<Self> {
        Arc::new(Self { session: session::Session::new() })
    }

    /// Blocks while Wine builds its prefix on first run (up to about a minute). Call it off the main thread.
    pub fn play(&self, wow_path: String, realm_address: String) -> Result<(), LauncherError> {
        self.session.play(&wow_path, &realm_address).map_err(LauncherError::from)
    }
}
