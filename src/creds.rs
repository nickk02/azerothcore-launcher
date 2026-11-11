//! One saved login in the OS store: Credential Manager on Windows, Keychain on macOS.
use keyring::Entry;

const SERVICE: &str = "AzerothCore";
const SLOT: &str = "login";

fn entry() -> keyring::Result<Entry> {
    Entry::new(SERVICE, SLOT)
}

/// The account name is stored with the password as "account\npassword".
pub fn store(account: &str, password: &str) -> keyring::Result<()> {
    entry()?.set_password(&format!("{account}\n{password}"))
}

pub fn get() -> Option<(String, String)> {
    let blob = entry().ok()?.get_password().ok()?;
    let (account, password) = blob.split_once('\n')?;
    Some((account.to_string(), password.to_string()))
}

pub fn clear() {
    if let Ok(e) = entry() {
        let _ = e.delete_credential();
    }
}
