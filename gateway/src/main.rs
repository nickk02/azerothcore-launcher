//! Usage: azerothcore-gateway [--listen 127.0.0.1:8080] [--auth 127.0.0.1:3724]
//!                           [--world 8085=127.0.0.1:8085] [--world 8086=127.0.0.1:8086] ...
//! `--world PUBLIC_PORT=HOST:PORT` is repeated once per realm; the default is one realm,
//! `8085=127.0.0.1:8085`. Point a Cloudflare Tunnel published application (service
//! `http://localhost:8080`) at it.

use gateway::{serve, Routes};
use std::collections::BTreeMap;

fn args() -> Vec<String> { std::env::args().skip(1).collect() }

fn values(name: &str) -> Vec<String> {
    let a = args();
    a.iter().enumerate().filter(|(_, x)| *x == name).filter_map(|(i, _)| a.get(i + 1).cloned()).collect()
}

fn value(name: &str, default: &str) -> String {
    values(name).into_iter().next().unwrap_or_else(|| default.to_string())
}

fn fail(msg: String) -> ! {
    eprintln!("{msg}");
    std::process::exit(2);
}

#[tokio::main]
async fn main() {
    let listen = value("--listen", "127.0.0.1:8080");
    let mut worlds = BTreeMap::new();
    let mut specs = values("--world");
    if specs.is_empty() {
        specs.push("8085=127.0.0.1:8085".into());
    }
    for spec in specs {
        let (port, target) = spec.split_once('=').unwrap_or_else(|| fail(format!("--world wants PORT=HOST:PORT, got {spec}")));
        let port: u16 = port.parse().unwrap_or_else(|_| fail(format!("bad port in --world {spec}")));
        worlds.insert(port, target.to_string());
    }
    let routes = Routes { auth: value("--auth", "127.0.0.1:3724"), worlds };
    let listener = tokio::net::TcpListener::bind(&listen).await.unwrap_or_else(|e| fail(format!("cannot listen on {listen}: {e}")));
    println!("gateway on {listen}: /auth -> {}", routes.auth);
    for (p, t) in &routes.worlds { println!("  /world/{p} -> {t}"); }
    serve(listener, routes).await;
}
