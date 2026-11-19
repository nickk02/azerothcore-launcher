//! End-to-end: mock game server <- gateway <- WebSocket <- launcher proxy <- "game client".
//! Everything runs on loopback, so this covers the same path a Cloudflare Tunnel carries,
//! minus Cloudflare itself.

use gateway::{serve, Routes};
use std::collections::BTreeMap;
use launcher_core::tunnel::{self, Tunnel};
use std::io::{Read, Write};
use std::net::{SocketAddr, TcpListener, TcpStream};
use std::thread;
use std::time::Duration;

/// Mock server: sends a banner, then echoes everything back with each byte incremented.
fn mock_server(banner: &'static [u8]) -> SocketAddr {
    let listener = TcpListener::bind("127.0.0.1:0").unwrap();
    let addr = listener.local_addr().unwrap();
    thread::spawn(move || {
        for conn in listener.incoming().flatten() {
            thread::spawn(move || {
                let mut conn = conn;
                conn.write_all(banner).unwrap();
                let mut buf = [0u8; 8192];
                while let Ok(n) = conn.read(&mut buf) {
                    if n == 0 { break; }
                    let out: Vec<u8> = buf[..n].iter().map(|b| b.wrapping_add(1)).collect();
                    if conn.write_all(&out).is_err() { break; }
                }
            });
        }
    });
    addr
}

struct Rig {
    _tunnel: Tunnel,
    base: String,
    auth: SocketAddr,
    /// Two realms, as on the live server: public ports 8085 and 8086.
    world: SocketAddr,
    world2: SocketAddr,
}

fn eph() -> SocketAddr { "127.0.0.1:0".parse().unwrap() }

fn routes(auth: SocketAddr, worlds: &[(u16, SocketAddr)]) -> Routes {
    Routes { auth: auth.to_string(), worlds: worlds.iter().map(|(p, a)| (*p, a.to_string())).collect::<BTreeMap<_, _>>() }
}

fn rig() -> Rig {
    let auth_server = mock_server(b"AUTH");
    let world_server = mock_server(b"WORLD");
    let world2_server = mock_server(b"ILLIDAN");

    let gw = std::net::TcpListener::bind("127.0.0.1:0").unwrap();
    let gw_addr = gw.local_addr().unwrap();
    gw.set_nonblocking(true).unwrap();
    let routes = routes(auth_server, &[(8085, world_server), (8086, world2_server)]);
    thread::spawn(move || {
        let rt = tokio::runtime::Builder::new_multi_thread().worker_threads(2).enable_all().build().unwrap();
        rt.block_on(async { serve(tokio::net::TcpListener::from_std(gw).unwrap(), routes).await });
    });

    let base = format!("ws://{gw_addr}");
    let tunnel = Tunnel::start_on(&base, eph(), &[(8085, eph()), (8086, eph())]).unwrap();
    let (auth, world, world2) = (tunnel.auth_addr, tunnel.world_addrs[0], tunnel.world_addrs[1]);
    Rig { _tunnel: tunnel, base, auth, world, world2 }
}

fn client(addr: SocketAddr) -> TcpStream {
    let s = TcpStream::connect(addr).unwrap();
    s.set_read_timeout(Some(Duration::from_secs(10))).unwrap();
    s
}

fn read_exact(s: &mut TcpStream, n: usize) -> Vec<u8> {
    let mut v = vec![0u8; n];
    s.read_exact(&mut v).unwrap();
    v
}

#[test]
fn auth_and_world_reach_the_right_servers() {
    let rig = rig();
    assert_eq!(read_exact(&mut client(rig.auth), 4), b"AUTH");
    assert_eq!(read_exact(&mut client(rig.world), 5), b"WORLD");
    assert_eq!(read_exact(&mut client(rig.world2), 7), b"ILLIDAN");
}

#[test]
fn bytes_round_trip_in_order() {
    let rig = rig();
    let mut c = client(rig.auth);
    assert_eq!(read_exact(&mut c, 4), b"AUTH");
    c.write_all(&[0, 1, 2, 250, 255]).unwrap();
    assert_eq!(read_exact(&mut c, 5), [1, 2, 3, 251, 0]);
}

#[test]
fn large_stream_arrives_intact() {
    // Bigger than any single read or message, to catch dropped or reordered chunks.
    let rig = rig();
    let mut c = client(rig.world);
    assert_eq!(read_exact(&mut c, 5), b"WORLD");
    let data: Vec<u8> = (0..2_000_000u32).map(|i| (i.wrapping_mul(31) >> 3) as u8).collect();
    let expected: Vec<u8> = data.iter().map(|b| b.wrapping_add(1)).collect();
    let mut writer = c.try_clone().unwrap();
    let sent = data.clone();
    let t = thread::spawn(move || writer.write_all(&sent).unwrap());
    let got = read_exact(&mut c, expected.len());
    t.join().unwrap();
    assert!(got == expected, "stream corrupted");
}

#[test]
fn many_concurrent_clients() {
    let rig = rig();
    let handles: Vec<_> = (0..40u8)
        .map(|i| {
            let addr = if i % 2 == 0 { rig.auth } else { rig.world };
            thread::spawn(move || {
                let mut c = client(addr);
                let banner = if i % 2 == 0 { 4 } else { 5 };
                read_exact(&mut c, banner);
                for round in 0..20u8 {
                    let msg = [i, round, i ^ round];
                    c.write_all(&msg).unwrap();
                    assert_eq!(read_exact(&mut c, 3), [i.wrapping_add(1), round.wrapping_add(1), (i ^ round).wrapping_add(1)]);
                }
            })
        })
        .collect();
    for h in handles { h.join().unwrap(); }
}

#[test]
fn server_closing_closes_the_client() {
    let rig = rig();
    // A one-shot server that sends a line and hangs up.
    let l = TcpListener::bind("127.0.0.1:0").unwrap();
    let addr = l.local_addr().unwrap();
    thread::spawn(move || { let (mut c, _) = l.accept().unwrap(); c.write_all(b"bye").unwrap(); });
    let gw = std::net::TcpListener::bind("127.0.0.1:0").unwrap();
    let gw_addr = gw.local_addr().unwrap();
    gw.set_nonblocking(true).unwrap();
    thread::spawn(move || {
        let rt = tokio::runtime::Builder::new_multi_thread().worker_threads(1).enable_all().build().unwrap();
        rt.block_on(async { serve(tokio::net::TcpListener::from_std(gw).unwrap(), routes(addr, &[(8085, addr)])).await });
    });
    let t = Tunnel::start_on(&format!("ws://{gw_addr}"), eph(), &[(8085, eph())]).unwrap();
    let mut c = client(t.auth_addr);
    let mut all = Vec::new();
    c.read_to_end(&mut all).unwrap(); // returns only if the close propagated
    assert_eq!(all, b"bye");
    drop(rig);
}

#[test]
fn client_closing_closes_the_server_side() {
    let l = TcpListener::bind("127.0.0.1:0").unwrap();
    let addr = l.local_addr().unwrap();
    let (tx, rx) = std::sync::mpsc::channel();
    thread::spawn(move || {
        let (mut c, _) = l.accept().unwrap();
        let mut buf = [0u8; 16];
        let n = c.read(&mut buf).unwrap();
        let _ = tx.send(n); // 5 bytes, then 0 when the client hangs up
        let _ = tx.send(c.read(&mut buf).unwrap());
    });
    let gw = std::net::TcpListener::bind("127.0.0.1:0").unwrap();
    let gw_addr = gw.local_addr().unwrap();
    gw.set_nonblocking(true).unwrap();
    thread::spawn(move || {
        let rt = tokio::runtime::Builder::new_multi_thread().worker_threads(1).enable_all().build().unwrap();
        rt.block_on(async { serve(tokio::net::TcpListener::from_std(gw).unwrap(), routes(addr, &[(8085, addr)])).await });
    });
    let t = Tunnel::start_on(&format!("ws://{gw_addr}"), eph(), &[(8085, eph())]).unwrap();
    let mut c = client(t.auth_addr);
    c.write_all(b"hello").unwrap();
    assert_eq!(rx.recv_timeout(Duration::from_secs(10)).unwrap(), 5);
    drop(c);
    assert_eq!(rx.recv_timeout(Duration::from_secs(10)).unwrap(), 0);
}

#[test]
fn status_check_reports_gateway_state() {
    let rig = rig();
    assert!(tunnel::check(&rig.base));
    assert!(!tunnel::check("ws://127.0.0.1:1"));
    assert_eq!(launcher_core::realm::check(&rig.base), launcher_core::realm::Reachability::Online);
    assert_eq!(launcher_core::realm::check("ws://127.0.0.1:1"), launcher_core::realm::Reachability::Unreachable);
}

/// Opens a WebSocket to exactly `url` and reports whether the gateway accepted the upgrade.
fn ws_accepts(url: &str) -> bool {
    let rt = tokio::runtime::Builder::new_current_thread().enable_all().build().unwrap();
    rt.block_on(async { tokio_tungstenite::connect_async(url).await.is_ok() })
}

#[test]
fn only_listed_paths_are_accepted() {
    let rig = rig();
    // Positive controls: the real paths work through the same helper.
    assert!(ws_accepts(&format!("{}/auth", rig.base)));
    assert!(ws_accepts(&format!("{}/world/8085", rig.base)));
    assert!(ws_accepts(&format!("{}/world/8086", rig.base)));
    // Everything else is refused, so the gateway is not an open TCP relay.
    for path in ["/", "/nope", "/world", "/world/", "/world/9999", "/world/abc", "/world/8085/x", "/auth/x"] {
        assert!(!ws_accepts(&format!("{}{path}", rig.base)), "{path} should be refused");
    }
}

#[test]
fn port_clash_is_reported() {
    let held = TcpListener::bind("127.0.0.1:0").unwrap();
    let addr = held.local_addr().unwrap();
    assert!(Tunnel::start_on("ws://127.0.0.1:1", addr, &[(8085, eph())]).is_err());
}
