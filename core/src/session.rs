//! One play session: starts the local proxy when the realm is a `wss://` gateway,
//! writes the realmlist and starts the game. Both the egui app and the Swift app call this.
use crate::tunnel::{self, Tunnel};
use std::sync::Mutex;

#[derive(Default)]
pub struct Session {
    /// Local proxy for gateway realms. It lives as long as the session.
    tunnel: Mutex<Option<(String, Tunnel)>>,
}

impl Session {
    pub fn new() -> Self {
        Self::default()
    }

    /// Starts the proxy if `realm` is a gateway address, then launches the game.
    pub fn play(&self, wow_path: &str, realm: &str) -> Result<(), String> {
        let mut realm = realm.trim().to_string();
        if let Some(base) = tunnel::gateway_url(&realm) {
            let mut slot = self.tunnel.lock().unwrap_or_else(|e| e.into_inner());
            if slot.as_ref().map(|(b, _)| b != &base).unwrap_or(true) {
                *slot = None; // free the ports before rebinding
                let t = Tunnel::start(&base).map_err(|_| "Could not start the local connection. Ports 3724, 8085 and 8086 must be free.".to_string())?;
                *slot = Some((base, t));
            }
            realm = tunnel::LOCAL_ADDRESS.to_string();
        }
        crate::play::launch(wow_path, &realm)
    }
}
