#![cfg_attr(windows, windows_subsystem = "windows")]

use eframe::egui;
use launcher_core::{config::Config, creds, play, realm::{self, Reachability}, update};
use std::sync::mpsc::{channel, Receiver};
use std::time::{Duration, Instant};

struct App {
    wow_path: String,
    realm: String,
    account: String,
    password: String,
    remember: bool,
    reachability: Reachability,
    message: String,
    update: update::UpdateInfo,
    update_rx: Receiver<update::UpdateInfo>,
    status_rx: Option<Receiver<(String, Reachability)>>,
    install_rx: Option<Receiver<Result<(), String>>>,
    last_edit: Option<Instant>,
    checked_realm: String,
}

impl App {
    fn new() -> Self {
        let cfg = Config::load();
        let (mut account, mut password) = (String::new(), String::new());
        if cfg.remember_login {
            if let Some((a, p)) = creds::get() {
                account = a;
                password = p;
            }
        }
        let (tx, update_rx) = channel();
        std::thread::spawn(move || { let _ = tx.send(update::check()); });
        Self {
            wow_path: cfg.wow_path,
            realm: cfg.realm_address,
            account,
            password,
            remember: cfg.remember_login,
            reachability: Reachability::Unconfigured,
            message: String::new(),
            update: Default::default(),
            update_rx,
            status_rx: None,
            install_rx: None,
            last_edit: Some(Instant::now() - Duration::from_secs(1)),
            checked_realm: String::from("\u{0}"),
        }
    }

    fn play(&mut self) {
        self.message.clear();
        let cfg = Config {
            wow_path: self.wow_path.clone(),
            realm_address: self.realm.trim().to_string(),
            remember_login: self.remember,
        };
        let _ = cfg.save();
        if self.remember && !self.account.is_empty() {
            let _ = creds::store(&self.account, &self.password);
        } else {
            creds::clear();
        }
        if let Err(e) = play::launch(&cfg.wow_path, &cfg.realm_address) {
            self.message = e;
        }
    }
}

impl eframe::App for App {
    fn update(&mut self, ctx: &egui::Context, _: &mut eframe::Frame) {
        if let Ok(info) = self.update_rx.try_recv() { self.update = info; }
        if let Some(rx) = &self.install_rx {
            if let Ok(result) = rx.try_recv() {
                self.message = result.err().unwrap_or_default();
                self.install_rx = None;
            }
        }
        // Re-check the realm 500 ms after typing stops.
        if self.realm != self.checked_realm && self.status_rx.is_none() {
            if self.last_edit.is_some_and(|t| t.elapsed() >= Duration::from_millis(500)) {
                let (tx, rx) = channel();
                let addr = self.realm.clone();
                self.checked_realm = addr.clone();
                std::thread::spawn(move || { let r = realm::check(&addr); let _ = tx.send((addr, r)); });
                self.status_rx = Some(rx);
            }
        }
        if let Some(rx) = &self.status_rx {
            if let Ok((addr, r)) = rx.try_recv() {
                if addr == self.realm { self.reachability = r; }
                self.status_rx = None;
            }
        }
        ctx.request_repaint_after(Duration::from_millis(100));

        egui::CentralPanel::default().show(ctx, |ui| {
            ui.heading("AzerothCore");
            if self.update.available {
                ui.horizontal(|ui| {
                    ui.label(format!("Version {} is available.", self.update.latest));
                    if ui.add_enabled(self.install_rx.is_none(), egui::Button::new("Update")).clicked() {
                        let (tx, rx) = channel();
                        let info = self.update.clone();
                        std::thread::spawn(move || { let _ = tx.send(update::install(&info)); });
                        self.install_rx = Some(rx);
                    }
                });
            }
            ui.add_space(8.0);
            egui::Grid::new("form").num_columns(2).spacing([12.0, 8.0]).show(ui, |ui| {
                ui.label("WoW client");
                ui.horizontal(|ui| {
                    ui.add(egui::TextEdit::singleline(&mut self.wow_path).hint_text("Path to Wow.exe").desired_width(240.0));
                    if ui.button("Browse…").clicked() {
                        if let Some(p) = rfd::FileDialog::new().set_title("Choose Wow.exe from your 3.3.5a client folder").pick_file() {
                            self.wow_path = p.display().to_string();
                        }
                    }
                });
                ui.end_row();

                ui.label("Realm address");
                if ui.add(egui::TextEdit::singleline(&mut self.realm).hint_text("logon.example.com").desired_width(320.0)).changed() {
                    self.last_edit = Some(Instant::now());
                }
                ui.end_row();

                ui.label("Status");
                let (text, color) = match self.reachability {
                    Reachability::Unconfigured => ("No realm configured", egui::Color32::GRAY),
                    Reachability::Online => ("Online", egui::Color32::from_rgb(60, 180, 75)),
                    Reachability::Unreachable => ("Unreachable", egui::Color32::from_rgb(220, 60, 60)),
                };
                ui.colored_label(color, text);
                ui.end_row();

                ui.label("Account");
                ui.add(egui::TextEdit::singleline(&mut self.account).desired_width(320.0));
                ui.end_row();

                ui.label("Password");
                ui.add(egui::TextEdit::singleline(&mut self.password).password(true).desired_width(320.0));
                ui.end_row();

                ui.label("");
                ui.checkbox(&mut self.remember, "Remember me");
                ui.end_row();
            });
            ui.add_space(8.0);
            ui.horizontal(|ui| {
                if ui.add_sized([80.0, 32.0], egui::Button::new("Play")).clicked() { self.play(); }
                ui.colored_label(egui::Color32::from_rgb(220, 60, 60), &self.message);
            });
            ui.add_space(4.0);
            ui.weak(format!("Version {}", update::CURRENT_VERSION));
        });
    }
}

fn main() -> eframe::Result<()> {
    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default().with_inner_size([520.0, 360.0]).with_resizable(false),
        ..Default::default()
    };
    eframe::run_native("AzerothCore", options, Box::new(|_| Ok(Box::new(App::new()))))
}
