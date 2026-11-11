//! Local proxy for realms served through a Cloudflare Tunnel.
//!
//! The WoW client only speaks plain TCP. When the realm address is a gateway URL
//! (`wss://play.example.com`), the launcher listens on 127.0.0.1:3724 and on each world
//! port (8085, 8086) and wraps every connection in a WebSocket to `<url>/auth` or
//! `<url>/world/<port>`, where the gateway unwraps it. Each realm's `address` in the
//! server's realmlist table must be 127.0.0.1 so the world connection comes back through
//! the same proxy, and its `port` picks which world listener the client uses.

use gateway::{pump, PING_EVERY};
use std::net::SocketAddr;
use std::sync::{Arc, OnceLock};
use std::time::Duration;
use tokio::io::{AsyncRead, AsyncWrite};
use tokio::net::{TcpListener, TcpStream};
use tokio::runtime::Runtime;
use tokio_rustls::rustls::pki_types::ServerName;
use tokio_rustls::rustls::{ClientConfig, RootCertStore};
use tokio_rustls::TlsConnector;
use tokio_tungstenite::tungstenite::client::IntoClientRequest;
use tokio_tungstenite::{client_async, WebSocketStream};

pub const AUTH_PORT: u16 = 3724;
/// World ports the launcher listens on, one per realm (the realmlist `port` column).
pub const WORLD_PORTS: [u16; 2] = [8085, 8086];
/// What the realmlist must say when the tunnel is in use.
pub const LOCAL_ADDRESS: &str = "127.0.0.1";

const CONNECT_TIMEOUT: Duration = Duration::from_secs(10);

/// `wss://host` (or `https://host`) means "use the gateway". Returns the base URL
/// with a `ws`/`wss` scheme and no trailing slash. Plain `host[:port]` returns None.
pub fn gateway_url(address: &str) -> Option<String> {
    let text = address.trim().trim_end_matches('/');
    let (scheme, rest) = text.split_once("://")?;
    let scheme = match scheme.to_ascii_lowercase().as_str() {
        "wss" | "https" => "wss",
        "ws" | "http" => "ws",
        _ => return None,
    };
    if rest.is_empty() { None } else { Some(format!("{scheme}://{rest}")) }
}

trait Io: AsyncRead + AsyncWrite + Unpin + Send {}
impl<T: AsyncRead + AsyncWrite + Unpin + Send> Io for T {}

fn tls_config() -> Arc<ClientConfig> {
    static CONFIG: OnceLock<Arc<ClientConfig>> = OnceLock::new();
    CONFIG
        .get_or_init(|| {
            let mut roots = RootCertStore::empty();
            roots.extend(webpki_roots::TLS_SERVER_ROOTS.iter().cloned());
            let provider = Arc::new(tokio_rustls::rustls::crypto::ring::default_provider());
            let config = ClientConfig::builder_with_provider(provider)
                .with_safe_default_protocol_versions()
                .expect("TLS versions")
                .with_root_certificates(roots)
                .with_no_client_auth();
            Arc::new(config)
        })
        .clone()
}

async fn connect(url: &str) -> Result<WebSocketStream<Box<dyn Io>>, String> {
    connect_with(url, tls_config()).await
}

async fn connect_with(url: &str, tls_config: Arc<ClientConfig>) -> Result<WebSocketStream<Box<dyn Io>>, String> {
    let request = url.into_client_request().map_err(|e| e.to_string())?;
    let uri = request.uri().clone();
    let tls = uri.scheme_str() == Some("wss");
    let host = uri.host().ok_or("no host in gateway address")?.to_string();
    let port = uri.port_u16().unwrap_or(if tls { 443 } else { 80 });

    let tcp = TcpStream::connect((host.as_str(), port)).await.map_err(|e| e.to_string())?;
    tcp.set_nodelay(true).ok();
    let io: Box<dyn Io> = if tls {
        let name = ServerName::try_from(host.clone()).map_err(|e| e.to_string())?;
        Box::new(TlsConnector::from(tls_config).connect(name, tcp).await.map_err(|e| e.to_string())?)
    } else {
        Box::new(tcp)
    };
    let (ws, _) = client_async(request, io).await.map_err(|e| e.to_string())?;
    Ok(ws)
}

/// Can the gateway be reached, and does it have a login server behind it?
pub fn check(base: &str) -> bool {
    let Ok(rt) = tokio::runtime::Builder::new_current_thread().enable_all().build() else { return false };
    let url = format!("{base}/auth");
    rt.block_on(async {
        tokio::time::timeout(Duration::from_secs(5), async {
            match connect(&url).await {
                Ok(mut ws) => {
                    let _ = futures_util::SinkExt::close(&mut ws).await;
                    true
                }
                Err(_) => false,
            }
        })
        .await
        .unwrap_or(false)
    })
}

/// The running proxy. Dropping it stops the listeners and closes every connection.
pub struct Tunnel {
    runtime: Option<Runtime>,
    pub auth_addr: SocketAddr,
    /// One local listener per world port, in the order given.
    pub world_addrs: Vec<SocketAddr>,
}

impl Tunnel {
    /// Starts listening on the standard ports.
    pub fn start(base: &str) -> std::io::Result<Self> {
        let worlds: Vec<(u16, SocketAddr)> = WORLD_PORTS.iter().map(|&p| (p, SocketAddr::from(([127, 0, 0, 1], p)))).collect();
        Self::start_on(base, ([127, 0, 0, 1], AUTH_PORT).into(), &worlds)
    }

    /// `worlds` pairs each realm's public port (used in the gateway path) with the local
    /// address to listen on. They differ only in tests, which bind ephemeral ports.
    pub fn start_on(base: &str, auth: SocketAddr, worlds: &[(u16, SocketAddr)]) -> std::io::Result<Self> {
        // Bind everything first so a port clash is reported to the caller, not lost in a task.
        let mut listeners = vec![(std::net::TcpListener::bind(auth)?, "/auth".to_string())];
        for (public, bind) in worlds {
            listeners.push((std::net::TcpListener::bind(bind)?, format!("/world/{public}")));
        }
        let mut addrs = Vec::new();
        for (l, _) in &listeners {
            l.set_nonblocking(true)?;
            addrs.push(l.local_addr()?);
        }

        let runtime = tokio::runtime::Builder::new_multi_thread().worker_threads(2).enable_all().build()?;
        for (listener, path) in listeners {
            let url = format!("{base}{path}");
            let listener = {
                let _guard = runtime.enter();
                TcpListener::from_std(listener)?
            };
            runtime.spawn(async move {
                loop {
                    let Ok((client, _)) = listener.accept().await else { continue };
                    client.set_nodelay(true).ok();
                    let url = url.clone();
                    tokio::spawn(async move {
                        if let Ok(Ok(ws)) = tokio::time::timeout(CONNECT_TIMEOUT, connect(&url)).await {
                            pump(ws, client, PING_EVERY).await;
                        }
                        // On failure the client socket drops, which the game sees as a refused login.
                    });
                }
            });
        }
        Ok(Self { runtime: Some(runtime), auth_addr: addrs[0], world_addrs: addrs[1..].to_vec() })
    }
}

impl Drop for Tunnel {
    fn drop(&mut self) {
        if let Some(rt) = self.runtime.take() {
            rt.shutdown_background();
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// The wss:// path end to end on loopback: a TLS server with a throwaway certificate,
    /// a WebSocket upgrade over it, then bytes both ways. Only the trust roots differ from
    /// production (which uses the bundled public roots).
    #[test]
    fn wss_round_trip_over_tls() {
        use futures_util::{SinkExt, StreamExt};
        use tokio_rustls::rustls::pki_types::{PrivateKeyDer, PrivatePkcs8KeyDer};
        use tokio_rustls::rustls::ServerConfig;
        use tokio_tungstenite::tungstenite::Message;

        let cert = rcgen::generate_simple_self_signed(vec!["localhost".to_string()]).unwrap();
        let provider = Arc::new(tokio_rustls::rustls::crypto::ring::default_provider());
        let server_cfg = ServerConfig::builder_with_provider(provider.clone())
            .with_safe_default_protocol_versions().unwrap()
            .with_no_client_auth()
            .with_single_cert(vec![cert.cert.der().clone()], PrivateKeyDer::Pkcs8(PrivatePkcs8KeyDer::from(cert.key_pair.serialize_der())))
            .unwrap();
        let mut roots = RootCertStore::empty();
        roots.add(cert.cert.der().clone()).unwrap();
        let client_cfg = Arc::new(
            ClientConfig::builder_with_provider(provider).with_safe_default_protocol_versions().unwrap()
                .with_root_certificates(roots).with_no_client_auth(),
        );

        let rt = tokio::runtime::Builder::new_multi_thread().worker_threads(2).enable_all().build().unwrap();
        rt.block_on(async {
            let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
            let port = listener.local_addr().unwrap().port();
            let acceptor = tokio_rustls::TlsAcceptor::from(Arc::new(server_cfg));
            tokio::spawn(async move {
                let (tcp, _) = listener.accept().await.unwrap();
                let tls = acceptor.accept(tcp).await.unwrap();
                let mut ws = tokio_tungstenite::accept_async(tls).await.unwrap();
                while let Some(Ok(Message::Binary(b))) = ws.next().await {
                    let _ = ws.send(Message::Binary(b.iter().map(|x| x.wrapping_add(1)).collect())).await;
                }
            });

            let mut ws = connect_with(&format!("wss://localhost:{port}/auth"), client_cfg.clone()).await.unwrap();
            ws.send(Message::Binary(vec![1, 2, 3])).await.unwrap();
            match ws.next().await {
                Some(Ok(Message::Binary(b))) => assert_eq!(b, vec![2, 3, 4]),
                other => panic!("unexpected: {other:?}"),
            }

            // A server whose certificate we do not trust must be refused.
            let untrusted = Arc::new(
                ClientConfig::builder_with_provider(Arc::new(tokio_rustls::rustls::crypto::ring::default_provider()))
                    .with_safe_default_protocol_versions().unwrap()
                    .with_root_certificates(RootCertStore::empty()).with_no_client_auth(),
            );
            let listener2 = TcpListener::bind("127.0.0.1:0").await.unwrap();
            let port2 = listener2.local_addr().unwrap().port();
            let acceptor2 = tokio_rustls::TlsAcceptor::from(Arc::new(
                ServerConfig::builder_with_provider(Arc::new(tokio_rustls::rustls::crypto::ring::default_provider()))
                    .with_safe_default_protocol_versions().unwrap().with_no_client_auth()
                    .with_single_cert(vec![cert.cert.der().clone()], PrivateKeyDer::Pkcs8(PrivatePkcs8KeyDer::from(cert.key_pair.serialize_der()))).unwrap(),
            ));
            tokio::spawn(async move { if let Ok((tcp, _)) = listener2.accept().await { let _ = acceptor2.accept(tcp).await; } });
            let err = connect_with(&format!("wss://localhost:{port2}/auth"), untrusted).await.err().expect("must refuse");
            assert!(err.contains("UnknownIssuer") || err.contains("certificate"), "{err}");
        });
    }

    #[test]
    fn recognises_gateway_addresses() {
        assert_eq!(gateway_url("wss://play.azerothcore.net").as_deref(), Some("wss://play.azerothcore.net"));
        assert_eq!(gateway_url("  https://play.azerothcore.net/ ").as_deref(), Some("wss://play.azerothcore.net"));
        assert_eq!(gateway_url("ws://127.0.0.1:8080").as_deref(), Some("ws://127.0.0.1:8080"));
        assert_eq!(gateway_url("logon.example.com"), None);
        assert_eq!(gateway_url("192.168.1.100:3724"), None);
        assert_eq!(gateway_url("wss://"), None);
        assert_eq!(gateway_url("ftp://x"), None);
    }
}
