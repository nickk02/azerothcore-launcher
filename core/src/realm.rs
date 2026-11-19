use std::net::{TcpStream, ToSocketAddrs};
use std::time::Duration;

pub const DEFAULT_PORT: u16 = 3724;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Reachability {
    Unconfigured,
    Online,
    Unreachable,
}

/// "host" or "host:port". The port defaults to 3724.
pub fn parse(address: &str) -> (&str, u16) {
    match address.split_once(':') {
        None => (address, DEFAULT_PORT),
        Some((host, port)) => (host, port.parse().unwrap_or(DEFAULT_PORT)),
    }
}

/// Opens a TCP connection and closes it. No data is sent.
pub fn check(address: &str) -> Reachability {
    let text = address.trim();
    if text.is_empty() {
        return Reachability::Unconfigured;
    }
    if let Some(base) = crate::tunnel::gateway_url(text) {
        return if crate::tunnel::check(&base) { Reachability::Online } else { Reachability::Unreachable };
    }
    let (host, port) = parse(text);
    if host.is_empty() {
        return Reachability::Unreachable;
    }
    let Ok(mut addrs) = (host, port).to_socket_addrs() else { return Reachability::Unreachable };
    let timeout = Duration::from_secs(5);
    if addrs.any(|a| TcpStream::connect_timeout(&a, timeout).is_ok()) {
        Reachability::Online
    } else {
        Reachability::Unreachable
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::net::TcpListener;

    #[test]
    fn parses_address() {
        assert_eq!(parse("logon.example.com:3724"), ("logon.example.com", 3724));
        assert_eq!(parse("logon.example.com").1, 3724);
        assert_eq!(parse("192.168.1.100:8085").1, 8085);
        assert_eq!(parse("host:abc").1, 3724);
        assert_eq!(parse("host:").1, 3724);
    }

    #[test]
    fn empty_is_unconfigured() {
        assert_eq!(check("   "), Reachability::Unconfigured);
    }

    #[test]
    fn detects_online_and_unreachable() {
        let listener = TcpListener::bind("127.0.0.1:0").unwrap();
        let port = listener.local_addr().unwrap().port();
        assert_eq!(check(&format!("127.0.0.1:{port}")), Reachability::Online);
        drop(listener);
        assert_eq!(check(&format!("127.0.0.1:{port}")), Reachability::Unreachable);
    }
}
