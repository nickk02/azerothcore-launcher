#![cfg_attr(windows, windows_subsystem = "windows")]

use eframe::egui::{self, Color32, RichText};
use launcher_core::{autofill, config::Config, creds, play, realm::{self, Reachability}, update};
use std::sync::mpsc::{channel, Receiver};
use std::time::{Duration, Instant};

const HERO: &[u8] = include_bytes!("../assets/hero.png");
const LOGO: &[u8] = include_bytes!("../assets/logo.png");
const ICON: &[u8] = include_bytes!("../assets/icon.png");

const GOLD: Color32 = Color32::from_rgb(0xF0, 0xC8, 0x60);
const TAN: Color32 = Color32::from_rgb(0xCB, 0xB9, 0x8A);
const TEXT: Color32 = Color32::from_rgb(0xDC, 0xE4, 0xF2);
const GREEN: Color32 = Color32::from_rgb(0x5C, 0xD0, 0x7A);
const RED: Color32 = Color32::from_rgb(0xFF, 0x9B, 0x9B);
const PANEL_W: f32 = 372.0;

fn load_texture(ctx: &egui::Context, name: &str, bytes: &[u8], max_width: Option<u32>) -> egui::TextureHandle {
    let mut img = image::load_from_memory(bytes).expect("bundled image").to_rgba8();
    if let Some(w) = max_width.filter(|&w| img.width() > w) {
        let h = img.height() * w / img.width();
        img = image::imageops::resize(&img, w, h, image::imageops::FilterType::Triangle);
    }
    let size = [img.width() as usize, img.height() as usize];
    ctx.load_texture(name, egui::ColorImage::from_rgba_unmultiplied(size, img.as_raw()), egui::TextureOptions::LINEAR)
}

struct App {
    hero: egui::TextureHandle,
    logo: egui::TextureHandle,
    started: Instant,
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
    autofill_rx: Option<Receiver<bool>>,
    last_edit: Instant,
    checked_realm: Option<String>,
}

impl App {
    fn new(ctx: &egui::Context) -> Self {
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
            hero: load_texture(ctx, "hero", HERO, Some(1280)),
            logo: load_texture(ctx, "logo", LOGO, None),
            started: Instant::now(),
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
            autofill_rx: None,
            last_edit: Instant::now() - Duration::from_secs(1),
            checked_realm: None,
        }
    }

    fn play(&mut self) {
        self.message.clear();
        if self.wow_path.trim().is_empty() {
            self.message = "No WoW install set. Use Browse to point at your Wow.exe.".into();
            return;
        }
        let cfg = Config {
            wow_path: self.wow_path.trim().to_string(),
            realm_address: self.realm.trim().to_string(),
            remember_login: self.remember,
        };
        if cfg.save().is_err() {
            self.message = "Failed to save settings.".into();
        }
        if self.remember && !self.account.is_empty() && !self.password.is_empty() {
            let _ = creds::store(&self.account, &self.password);
        } else if !self.remember {
            creds::clear();
        }
        if let Err(e) = play::launch(&cfg.wow_path, &cfg.realm_address) {
            self.message = e;
            return;
        }
        // Windows types the login into the game's login screen.
        if cfg!(windows) {
            let login = if !self.password.is_empty() {
                Some((self.account.clone(), self.password.clone()))
            } else if self.remember {
                creds::get()
            } else {
                None
            };
            if let Some((account, password)) = login {
                let (tx, rx) = channel();
                std::thread::spawn(move || { let _ = tx.send(autofill::type_login(&account, &password)); });
                self.autofill_rx = Some(rx);
            }
        }
    }

    fn poll(&mut self, ctx: &egui::Context) {
        if let Ok(info) = self.update_rx.try_recv() { self.update = info; }
        if let Some(rx) = &self.install_rx {
            if let Ok(result) = rx.try_recv() {
                self.install_rx = None;
                match result {
                    // The installer cannot replace a running exe, so Windows exits.
                    Ok(()) if cfg!(windows) => ctx.send_viewport_cmd(egui::ViewportCommand::Close),
                    Ok(()) => self.message.clear(),
                    Err(e) => self.message = e,
                }
            }
        }
        if let Some(rx) = &self.autofill_rx {
            if let Ok(ok) = rx.try_recv() {
                self.autofill_rx = None;
                if !ok { self.message = "Could not type the login into the game.".into(); }
            }
        }
        // Re-check the realm 500 ms after typing stops.
        if self.status_rx.is_none()
            && self.checked_realm.as_deref() != Some(self.realm.as_str())
            && self.last_edit.elapsed() >= Duration::from_millis(500)
        {
            let (tx, rx) = channel();
            let addr = self.realm.clone();
            self.checked_realm = Some(addr.clone());
            std::thread::spawn(move || { let r = realm::check(&addr); let _ = tx.send((addr, r)); });
            self.status_rx = Some(rx);
        }
        if let Some(rx) = &self.status_rx {
            if let Ok((addr, r)) = rx.try_recv() {
                if addr == self.realm.trim() || addr == self.realm { self.reachability = r; }
                self.status_rx = None;
            }
        }
    }

    fn paint_hero(&self, ui: &egui::Ui, rect: egui::Rect) {
        // Cover-fit with a slow zoom drift so the page looks alive, not animated.
        let t = self.started.elapsed().as_secs_f32();
        let zoom = 1.0 + 0.09 * (0.5 - 0.5 * (t / 26.0 * std::f32::consts::PI).cos());
        let tex = self.hero.size_vec2();
        let (rw, rh) = (rect.width() / rect.height(), tex.x / tex.y);
        let (mut u, mut v) = if rh > rw { (rw / rh, 1.0) } else { (1.0, rh / rw) };
        u /= zoom;
        v /= zoom;
        let uv = egui::Rect::from_min_max(egui::pos2(0.5 - u / 2.0, 0.5 - v / 2.0), egui::pos2(0.5 + u / 2.0, 0.5 + v / 2.0));
        ui.painter().image(self.hero.id(), rect, uv, Color32::WHITE);
        // Darken toward the panel so the text stays readable.
        let panel = egui::Rect::from_min_max(egui::pos2(rect.right() - PANEL_W, rect.top()), rect.max);
        ui.painter().rect_filled(panel, 0.0, Color32::from_rgba_unmultiplied(8, 8, 10, 205));
    }
}

fn label(ui: &mut egui::Ui, text: &str) {
    ui.label(RichText::new(text).color(TAN).size(12.0).strong());
}

impl eframe::App for App {
    fn update(&mut self, ctx: &egui::Context, _: &mut eframe::Frame) {
        self.poll(ctx);
        ctx.request_repaint_after(Duration::from_millis(33));

        egui::CentralPanel::default().frame(egui::Frame::none()).show(ctx, |ui| {
            let full = ui.max_rect();
            self.paint_hero(ui, full);

            let panel = egui::Rect::from_min_max(egui::pos2(full.right() - PANEL_W + 24.0, full.top() + 18.0), egui::pos2(full.right() - 24.0, full.bottom() - 12.0));
            ui.allocate_new_ui(egui::UiBuilder::new().max_rect(panel), |ui| {
                ui.spacing_mut().item_spacing.y = 6.0;
                let w = ui.available_width();
                let logo_h = w * self.logo.size_vec2().y / self.logo.size_vec2().x;
                ui.vertical_centered(|ui| ui.image((self.logo.id(), egui::vec2(w, logo_h.min(150.0)))));

                if self.update.available {
                    ui.horizontal(|ui| {
                        ui.label(RichText::new(format!("Version {} is available.", self.update.latest)).color(GOLD));
                        if ui.add_enabled(self.install_rx.is_none(), egui::Button::new("Update")).clicked() {
                            let (tx, rx) = channel();
                            let info = self.update.clone();
                            std::thread::spawn(move || { let _ = tx.send(update::install(&info)); });
                            self.install_rx = Some(rx);
                        }
                    });
                }

                let (text, color) = match self.reachability {
                    Reachability::Unconfigured => ("No realm configured", Color32::GRAY),
                    Reachability::Online => ("Online", GREEN),
                    Reachability::Unreachable => ("Unreachable", RED),
                };
                ui.horizontal(|ui| {
                    let (dot, _) = ui.allocate_exact_size(egui::vec2(10.0, 14.0), egui::Sense::hover());
                    ui.painter().circle_filled(dot.center(), 4.0, color);
                    ui.label(RichText::new(text).color(color));
                });
                ui.add_space(4.0);

                label(ui, "ACCOUNT NAME");
                ui.add(egui::TextEdit::singleline(&mut self.account).desired_width(w).margin(egui::vec2(8.0, 6.0)));
                label(ui, "PASSWORD");
                let pw = ui.add(egui::TextEdit::singleline(&mut self.password).password(true).desired_width(w).margin(egui::vec2(8.0, 6.0)));
                ui.checkbox(&mut self.remember, RichText::new("Remember me").color(TEXT));
                ui.add_space(4.0);

                let play = egui::Button::new(RichText::new("PLAY").size(18.0).strong().color(Color32::from_rgb(0x0A, 0x0A, 0x0A)))
                    .fill(GOLD)
                    .min_size(egui::vec2(w, 44.0));
                let enter = pw.lost_focus() && ui.input(|i| i.key_pressed(egui::Key::Enter));
                if ui.add(play).clicked() || enter { self.play(); }
                if !self.message.is_empty() {
                    ui.label(RichText::new(&self.message).color(RED));
                }

                ui.add_space(10.0);
                label(ui, "WOW INSTALL PATH");
                ui.horizontal(|ui| {
                    ui.add(egui::TextEdit::singleline(&mut self.wow_path).hint_text("Not set").desired_width(w - 80.0).margin(egui::vec2(8.0, 6.0)));
                    if ui.button("Browse").clicked() {
                        if let Some(p) = rfd::FileDialog::new().set_title("Choose Wow.exe from your 3.3.5a client folder").add_filter("Wow.exe", &["exe"]).pick_file() {
                            self.wow_path = p.display().to_string();
                        }
                    }
                });
                label(ui, "REALM ADDRESS");
                if ui.add(egui::TextEdit::singleline(&mut self.realm).hint_text("e.g. 192.168.1.100").desired_width(w).margin(egui::vec2(8.0, 6.0))).changed() {
                    self.last_edit = Instant::now();
                }
                ui.add_space(2.0);
                ui.label(RichText::new(format!("Version {}", update::CURRENT_VERSION)).color(Color32::GRAY).size(11.0));
            });
        });
    }
}

fn style(ctx: &egui::Context) {
    let mut v = egui::Visuals::dark();
    v.override_text_color = Some(TEXT);
    v.extreme_bg_color = Color32::from_rgba_unmultiplied(20, 20, 20, 230);
    v.widgets.inactive.bg_stroke = egui::Stroke::new(1.0_f32, Color32::from_rgba_unmultiplied(0xCB, 0xB9, 0x8A, 0x66));
    v.widgets.hovered.bg_stroke = egui::Stroke::new(1.0_f32, TAN);
    v.selection.bg_fill = Color32::from_rgb(0x8A, 0x78, 0x40);
    v.selection.stroke = egui::Stroke::new(1.0_f32, GOLD);
    ctx.set_visuals(v);
}

fn main() -> eframe::Result<()> {
    let icon = image::load_from_memory(ICON).ok().map(|i| {
        let i = i.to_rgba8();
        egui::IconData { width: i.width(), height: i.height(), rgba: i.into_raw() }
    });
    let mut viewport = egui::ViewportBuilder::default().with_inner_size([980.0, 600.0]).with_resizable(false).with_title("AzerothCore");
    if let Some(icon) = icon {
        viewport = viewport.with_icon(icon);
    }
    let options = eframe::NativeOptions { viewport, ..Default::default() };
    eframe::run_native("AzerothCore", options, Box::new(|cc| {
        style(&cc.egui_ctx);
        Ok(Box::new(App::new(&cc.egui_ctx)))
    }))
}
