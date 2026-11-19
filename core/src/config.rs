use std::path::PathBuf;

/// `settings.ini` in the same key=value format both native apps already use,
/// so an existing install's settings carry over.
#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct Config {
    pub wow_path: String,
    pub realm_address: String,
    pub remember_login: bool,
}

impl Config {
    pub fn path() -> PathBuf {
        // %APPDATA%\AzerothCore on Windows, ~/Library/Application Support/AzerothCore on macOS.
        let base = dirs::config_dir().unwrap_or_else(|| PathBuf::from("."));
        base.join("AzerothCore").join("settings.ini")
    }

    pub fn parse(text: &str) -> Self {
        let mut cfg = Self::default();
        for line in text.lines() {
            let Some((key, value)) = line.split_once('=') else { continue };
            match key {
                "WowPath" => cfg.wow_path = value.trim_end_matches('\r').to_string(),
                "RealmAddress" => cfg.realm_address = value.trim_end_matches('\r').to_string(),
                "CredentialVaultEnabled" => cfg.remember_login = value.trim_end_matches('\r') == "1",
                _ => {}
            }
        }
        cfg
    }

    pub fn serialize(&self) -> String {
        format!(
            "WowPath={}\nRealmAddress={}\nCredentialVaultEnabled={}\n",
            self.wow_path,
            self.realm_address,
            if self.remember_login { "1" } else { "0" }
        )
    }

    pub fn load() -> Self {
        std::fs::read_to_string(Self::path()).map(|t| Self::parse(&t)).unwrap_or_default()
    }

    pub fn save(&self) -> std::io::Result<()> {
        let path = Self::path();
        if let Some(dir) = path.parent() {
            std::fs::create_dir_all(dir)?;
        }
        std::fs::write(path, self.serialize())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn round_trip() {
        let cfg = Config { wow_path: "C:\\WoW\\Wow.exe".into(), realm_address: "logon.example.com:8085".into(), remember_login: true };
        assert_eq!(Config::parse(&cfg.serialize()), cfg);
    }

    #[test]
    fn reads_native_app_files() {
        let cfg = Config::parse("WowPath=/x/Wow.exe\r\nRealmAddress=a=b\r\nCredentialVaultEnabled=0\r\nJunk\r\n");
        assert_eq!(cfg.wow_path, "/x/Wow.exe");
        assert_eq!(cfg.realm_address, "a=b");
        assert!(!cfg.remember_login);
    }

    #[test]
    fn missing_file_gives_defaults() {
        assert_eq!(Config::parse(""), Config::default());
    }
}
