//! WebSocket-to-TCP gateway.
//!
//! The WoW client speaks plain TCP, which Cloudflare's edge will not carry for a
//! public hostname. The launcher wraps each connection in a WebSocket instead, and
//! this gateway (running next to the server, behind `cloudflared`) unwraps it:
//! every binary message is a chunk of the TCP stream, in order.
//!
//! `/auth` goes to the login server and `/world/<port>` to the world server whose
//! public port is `<port>` (one path per realm, since realms listen on different ports).
//! The servers themselves are unchanged.

use futures_util::{SinkExt, StreamExt};
use std::collections::BTreeMap;
use std::sync::Arc;
use std::time::Duration;
use tokio::io::{AsyncRead, AsyncReadExt, AsyncWrite, AsyncWriteExt};
use tokio::net::{TcpListener, TcpStream};
use tokio_tungstenite::tungstenite::handshake::server::{ErrorResponse, Request, Response};
use tokio_tungstenite::tungstenite::{http, Message};
use tokio_tungstenite::{accept_hdr_async, WebSocketStream};

/// Where each path forwards to, as `host:port`.
#[derive(Clone, Debug)]
pub struct Routes {
    pub auth: String,
    /// World servers by the port clients know them by (the realmlist `port` column).
    pub worlds: BTreeMap<u16, String>,
}

impl Routes {
    fn target(&self, path: &str) -> Option<String> {
        if path == "/auth" {
            return Some(self.auth.clone());
        }
        let port: u16 = path.strip_prefix("/world/")?.parse().ok()?;
        self.worlds.get(&port).cloned()
    }
}

/// Keeps idle connections alive through Cloudflare, which drops quiet WebSockets.
pub const PING_EVERY: Duration = Duration::from_secs(25);

/// Moves bytes both ways between a WebSocket and a TCP stream until either side ends.
pub async fn pump<S>(ws: WebSocketStream<S>, tcp: TcpStream, ping_every: Duration)
where
    S: AsyncRead + AsyncWrite + Unpin + Send + 'static,
{
    let (mut sink, mut stream) = ws.split();
    let (mut rd, mut wr) = tcp.into_split();

    let to_tcp = async move {
        while let Some(Ok(msg)) = stream.next().await {
            match msg {
                Message::Binary(bytes) => {
                    if wr.write_all(&bytes).await.is_err() {
                        break;
                    }
                }
                Message::Close(_) => break,
                _ => {} // pings are answered by the library, pongs and text are ignored
            }
        }
        let _ = wr.shutdown().await;
    };

    let to_ws = async move {
        let mut buf = vec![0u8; 16 * 1024];
        let mut tick = tokio::time::interval(ping_every);
        tick.tick().await;
        loop {
            tokio::select! {
                n = rd.read(&mut buf) => match n {
                    Ok(0) | Err(_) => break,
                    Ok(n) => if sink.send(Message::Binary(buf[..n].to_vec())).await.is_err() { break },
                },
                _ = tick.tick() => if sink.send(Message::Ping(Vec::new())).await.is_err() { break },
            }
        }
        let _ = sink.close().await;
    };

    // Either direction ending ends the connection.
    tokio::select! {
        _ = to_tcp => {}
        _ = to_ws => {}
    }
}

async fn handle(stream: TcpStream, routes: Arc<Routes>) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    stream.set_nodelay(true)?;
    let mut target: Option<String> = None;
    let ws = accept_hdr_async(stream, |req: &Request, resp: Response| -> Result<Response, ErrorResponse> {
        match routes.target(req.uri().path()) {
            Some(t) => { target = Some(t); Ok(resp) }
            None => Err(http::Response::builder().status(404).body(Some("not found".to_string())).unwrap()),
        }
    })
    .await?;

    let tcp = TcpStream::connect(target.ok_or("no route")?).await?;
    tcp.set_nodelay(true)?;
    pump(ws, tcp, PING_EVERY).await;
    Ok(())
}

/// Accepts WebSocket connections on `listener` forever.
pub async fn serve(listener: TcpListener, routes: Routes) {
    let routes = Arc::new(routes);
    loop {
        match listener.accept().await {
            Ok((stream, _)) => {
                let routes = routes.clone();
                tokio::spawn(async move { let _ = handle(stream, routes).await; });
            }
            Err(_) => tokio::time::sleep(Duration::from_millis(50)).await,
        }
    }
}
