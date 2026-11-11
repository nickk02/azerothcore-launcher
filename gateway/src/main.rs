//! Usage: azerothcore-gateway [--listen 127.0.0.1:8080] [--auth 127.0.0.1:3724] [--world 127.0.0.1:8085]
//! Point a Cloudflare Tunnel published application (service `http://localhost:8080`) at it.

use gateway::{serve, Routes};

fn arg(name: &str, default: &str) -> String {
    let args: Vec<String> = std::env::args().collect();
    args.iter().position(|a| a == name).and_then(|i| args.get(i + 1).cloned()).unwrap_or_else(|| default.to_string())
}

#[tokio::main]
async fn main() {
    let listen = arg("--listen", "127.0.0.1:8080");
    let routes = Routes { auth: arg("--auth", "127.0.0.1:3724"), world: arg("--world", "127.0.0.1:8085") };
    let listener = tokio::net::TcpListener::bind(&listen).await.unwrap_or_else(|e| {
        eprintln!("cannot listen on {listen}: {e}");
        std::process::exit(1);
    });
    println!("gateway on {listen}: /auth -> {}, /world -> {}", routes.auth, routes.world);
    serve(listener, routes).await;
}
