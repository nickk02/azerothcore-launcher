#![cfg_attr(windows, windows_subsystem = "windows")]

//! The window. Layout, colours and motion follow the original WinUI page
//! (HomePage.xaml): drifting hero art, a left-to-right dark wash, the logo
//! centred on the page, and a 372 px field column on the right.

use eframe::egui::{self, Color32, FontData, FontDefinitions, FontFamily, FontId, RichText};
use launcher_core::{autofill, config::Config, creds, play, realm::{self, Reachability}, update};
use std::path::Path;
use std::sync::mpsc::{channel, Receiver};
use std::time::{Duration, Instant};

const HERO: &[u8] = include_bytes!("../assets/hero.png");
const LOGO: &[u8] = include_bytes!("../assets/logo.png");
const ICON: &[u8] = include_bytes!("../assets/icon.png");

const fn rgba(r: u8, g: u8, b: u8, a: u8) -> Color32 { Color32::from_rgba_premultiplied(r, g, b, a) }
fn tint(r: u8, g: u8, b: u8, a: u8) -> Color32 { Color32::from_rgba_unmultiplied(r, g, b, a) }

const TAN: Color32 = Color32::from_rgb(0xCB, 0xB9, 0x8A);
const GOLD: Color32 = Color32::from_rgb(0xF0, 0xC8, 0x60);
const TEXT: Color32 = Color32::from_rgb(0xDC, 0xE4, 0xF2);
const ERROR: Color32 = Color32::from_rgb(0xFF, 0x9B, 0x9B);
const INK: Color32 = Color32::from_rgb(0x0A, 0x0A, 0x0A);

const WINDOW: [f32; 2] = [1100.0, 720.0];
const PANEL_W: f32 = 372.0;
const SEMIBOLD: &str = "semibold";

fn semibold(size: f32) -> FontId { FontId::new(size, FontFamily::Name(SEMIBOLD.into())) }

fn ease_out_cubic(p: f32) -> f32 { 1.0 - (1.0 - p.clamp(0.0, 1.0)).powi(3) }
fn progress(t: f32, delay: f32, dur: f32) -> f32 { ((t - delay) / dur).clamp(0.0, 1.0) }

fn load_texture(ctx: &egui::Context, name: &str, bytes: &[u8], max_width: Option<u32>) -> egui::TextureHandle {
    let mut img = image::load_from_memory(bytes).expect("bundled image").to_rgba8();
    if let Some(w) = max_width.filter(|&w| img.width() > w) {
        let h = img.height() * w / img.width();
        img = image::imageops::resize(&img, w, h, image::imageops::FilterType::Lanczos3);
    }
    let size = [img.width() as usize, img.height() as usize];
    ctx.load_texture(name, egui::ColorImage::from_rgba_unmultiplied(size, img.as_raw()), egui::TextureOptions::LINEAR)
}

/// Segoe UI on Windows, as the original used. Elsewhere egui's bundled font stands in.
fn setup_fonts(ctx: &egui::Context) {
    let mut fonts = FontDefinitions::default();
    let read = |name: &str| std::fs::read(Path::new(&std::env::var("WINDIR").unwrap_or_default()).join("Fonts").join(name)).ok();
    if cfg!(windows) {
        if let Some(regular) = read("segoeui.ttf") {
            fonts.font_data.insert("segoe".into(), FontData::from_owned(regular));
            fonts.families.get_mut(&FontFamily::Proportional).unwrap().insert(0, "segoe".into());
        }
    }
    let semi = if cfg!(windows) { read("seguisb.ttf") } else { None };
    let (key, fallback) = match semi {
        Some(bytes) => {
            fonts.font_data.insert("segoe-semibold".into(), FontData::from_owned(bytes));
            ("segoe-semibold".to_string(), vec![])
        }
        None => (String::new(), fonts.families[&FontFamily::Proportional].clone()),
    };
    let family = if key.is_empty() { fallback } else { vec![key] };
    fonts.families.insert(FontFamily::Name(SEMIBOLD.into()), family);
    ctx.set_fonts(fonts);
}

fn style(ctx: &egui::Context) {
    let mut v = egui::Visuals::dark();
    v.override_text_color = Some(TEXT);
    v.panel_fill = INK;
    v.extreme_bg_color = tint(0x14, 0x14, 0x14, 0xB3);
    v.selection.bg_fill = TAN;
    v.selection.stroke = egui::Stroke::new(1.0_f32, GOLD); // focused border
    let r = egui::Rounding::same(4.0);
    for w in [&mut v.widgets.inactive, &mut v.widgets.hovered, &mut v.widgets.active, &mut v.widgets.noninteractive] {
        w.rounding = r;
    }
    v.widgets.inactive.bg_stroke = egui::Stroke::new(1.0_f32, tint(0xCB, 0xB9, 0x8A, 0x66));
    v.widgets.hovered.bg_stroke = egui::Stroke::new(1.0_f32, tint(0xCB, 0xB9, 0x8A, 0xA6));
    v.widgets.hovered.bg_fill = tint(0x1C, 0x1C, 0x1C, 0xCC);
    v.widgets.active.bg_stroke = egui::Stroke::new(1.0_f32, GOLD);
    ctx.set_visuals(v);
}

struct ButtonStyle {
    fg: [Color32; 3],     // rest, hover, pressed
    fill: [Color32; 3],
    border: [Color32; 3],
}

const PLAY: ButtonStyle = ButtonStyle {
    fg: [INK, INK, INK],
    fill: [TAN, GOLD, Color32::from_rgb(0xA8, 0x97, 0x68)],
    border: [GOLD, Color32::from_rgb(0xFF, 0xE9, 0xA6), TAN],
};
const GHOST: ButtonStyle = ButtonStyle {
    fg: [TAN, GOLD, GOLD],
    fill: [Color32::TRANSPARENT, rgba(0x0A, 0x09, 0x07, 0x33), rgba(0x12, 0x10, 0x0C, 0x59)],
    border: [rgba(0x28, 0x24, 0x1B, 0x66), GOLD, TAN],
};

fn button(ui: &mut egui::Ui, text: &str, font: FontId, size: egui::Vec2, s: &ButtonStyle) -> egui::Response {
    let (rect, resp) = ui.allocate_exact_size(size, egui::Sense::click());
    let i = if resp.is_pointer_button_down_on() { 2 } else if resp.hovered() { 1 } else { 0 };
    ui.painter().rect(rect, 3.0, s.fill[i], egui::Stroke::new(1.0_f32, s.border[i]));
    ui.painter().text(rect.center(), egui::Align2::CENTER_CENTER, text, font, s.fg[i]);
    if resp.hovered() { ui.ctx().set_cursor_icon(egui::CursorIcon::PointingHand); }
    resp
}

fn checkbox(ui: &mut egui::Ui, value: &mut bool, label: &str) -> bool {
    let (rect, resp) = ui.allocate_exact_size(egui::vec2(ui.available_width(), 24.0), egui::Sense::click());
    let hot = resp.hovered();
    let boxr = egui::Rect::from_min_size(rect.min + egui::vec2(0.0, 2.0), egui::vec2(20.0, 20.0));
    let (fill, stroke) = match (*value, hot) {
        (true, false) => (TAN, GOLD),
        (true, true) => (GOLD, GOLD),
        (false, false) => (tint(0x14, 0x14, 0x14, 0x66), tint(0xCB, 0xB9, 0x8A, 0x88)),
        (false, true) => (tint(0xCB, 0xB9, 0x8A, 0x33), GOLD),
    };
    ui.painter().rect(boxr, 4.0, fill, egui::Stroke::new(1.0_f32, stroke));
    if *value {
        let c = boxr.center();
        ui.painter().add(egui::Shape::line(
            vec![c + egui::vec2(-5.0, 0.5), c + egui::vec2(-1.5, 4.0), c + egui::vec2(5.0, -4.0)],
            egui::Stroke::new(2.0_f32, INK),
        ));
    }
    ui.painter().text(egui::pos2(boxr.right() + 8.0, rect.center().y), egui::Align2::LEFT_CENTER, label, FontId::proportional(14.0), if hot { GOLD } else { TEXT });
    if resp.clicked() { *value = !*value; return true; }
    false
}

fn field_label(ui: &mut egui::Ui, text: &str) {
    ui.label(RichText::new(text).font(semibold(12.0)).color(TAN));
    ui.add_space(4.0);
}

fn field<'t>(text: &'t mut String, hint: &str, password: bool) -> egui::TextEdit<'t> {
    egui::TextEdit::singleline(text)
        .hint_text(RichText::new(hint).color(tint(0xDC, 0xE4, 0xF2, 0x66)))
        .password(password)
        .font(FontId::proportional(14.0))
        .margin(egui::vec2(8.0, 8.0))
        .desired_width(f32::INFINITY)
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
    realm_status: String,
    error: String,
    version_text: String,
    update: update::UpdateInfo,
    update_rx: Receiver<update::UpdateInfo>,
    status_rx: Option<Receiver<(String, Reachability)>>,
    install_rx: Option<Receiver<Result<(), String>>>,
    autofill_rx: Option<Receiver<bool>>,
    last_edit: Instant,
    checked_realm: Option<String>,
    content_h: f32,
    loading: bool,
}

impl App {
    fn new(ctx: &egui::Context) -> Self {
        let cfg = Config::load();
        // Only the account name is filled in. The password never goes into a visible control.
        let account = if cfg.remember_login { creds::get().map(|(a, _)| a).unwrap_or_default() } else { String::new() };
        let (tx, update_rx) = channel();
        std::thread::spawn(move || { let _ = tx.send(update::check()); });
        Self {
            hero: load_texture(ctx, "hero", HERO, Some(2048)),
            logo: load_texture(ctx, "logo", LOGO, None),
            started: Instant::now(),
            wow_path: cfg.wow_path,
            realm: cfg.realm_address,
            account,
            password: String::new(),
            remember: cfg.remember_login,
            realm_status: "No realm configured".into(),
            error: String::new(),
            version_text: format!("AzerothCore v{}", update::CURRENT_VERSION),
            update: Default::default(),
            update_rx,
            status_rx: None,
            install_rx: None,
            autofill_rx: None,
            last_edit: Instant::now() - Duration::from_secs(1),
            checked_realm: None,
            content_h: 560.0,
            loading: false,
        }
    }

    fn save(&mut self) {
        let cfg = Config { wow_path: self.wow_path.clone(), realm_address: self.realm.trim().to_string(), remember_login: self.remember };
        if cfg.save().is_err() { self.error = "Failed to save settings".into(); } else { self.error.clear(); }
    }

    fn browse(&mut self) {
        let Some(p) = rfd::FileDialog::new().set_title("Choose Wow.exe from your 3.3.5a client folder").add_filter("Wow.exe", &["exe"]).pick_file() else { return };
        if play::is_valid_wow_exe(&p) {
            self.wow_path = p.display().to_string();
            self.save();
        } else {
            self.error = "That isn't a 3.3.5a Wow.exe - pick the Wow.exe in your client folder".into();
        }
    }

    fn play(&mut self) {
        if self.wow_path.trim().is_empty() {
            self.error = "No WoW install set - use Browse below to point at your Wow.exe".into();
            return;
        }
        if self.remember && !self.account.is_empty() && !self.password.is_empty() {
            let _ = creds::store(&self.account, &self.password);
        }
        self.save();
        let cfg_realm = self.realm.trim().to_string();
        if let Err(e) = play::launch(&self.wow_path, &cfg_realm) {
            self.error = e;
            return;
        }
        // Windows types the login into the game's login screen. Prefer what was just typed.
        if cfg!(windows) {
            let login = if !self.password.is_empty() { Some((self.account.clone(), self.password.clone())) }
                else if self.remember { creds::get() } else { None };
            if let Some((account, password)) = login {
                let (tx, rx) = channel();
                std::thread::spawn(move || { let _ = tx.send(autofill::type_login(&account, &password)); });
                self.autofill_rx = Some(rx);
            }
        }
    }

    fn start_install(&mut self) {
        let (tx, rx) = channel();
        let info = self.update.clone();
        self.version_text = format!("Updating to v{}...", info.latest);
        std::thread::spawn(move || { let _ = tx.send(update::install(&info)); });
        self.install_rx = Some(rx);
    }

    fn poll(&mut self, ctx: &egui::Context) {
        if let Ok(info) = self.update_rx.try_recv() {
            self.update = info;
            // Windows updates itself at startup, as the original did.
            if cfg!(windows) && self.update.available { self.start_install(); }
        }
        if let Some(rx) = &self.install_rx {
            if let Ok(result) = rx.try_recv() {
                self.install_rx = None;
                match result {
                    // The installer cannot replace a running exe, so Windows exits.
                    Ok(()) if cfg!(windows) => ctx.send_viewport_cmd(egui::ViewportCommand::Close),
                    Ok(()) => self.version_text = format!("AzerothCore v{}", update::CURRENT_VERSION),
                    Err(e) => {
                        let why = if e.contains("checksum") { "failed to verify" } else { "failed" };
                        self.version_text = format!("AzerothCore v{} (update v{} {})", update::CURRENT_VERSION, self.update.latest, why);
                        self.update.available = false;
                    }
                }
            }
        }
        if let Some(rx) = &self.autofill_rx {
            if let Ok(ok) = rx.try_recv() {
                self.autofill_rx = None;
                if !ok { self.error = "Not signed in - credentials not saved".into(); }
            }
        }
        // Re-check the realm shortly after typing stops.
        if self.status_rx.is_none() && !self.loading
            && self.checked_realm.as_deref() != Some(self.realm.as_str())
            && self.last_edit.elapsed() >= Duration::from_millis(400)
        {
            let addr = self.realm.clone();
            self.checked_realm = Some(addr.clone());
            if addr.trim().is_empty() {
                self.realm_status = "No realm configured".into();
            } else {
                self.realm_status = "Checking...".into();
                let (tx, rx) = channel();
                std::thread::spawn(move || { let r = realm::check(&addr); let _ = tx.send((addr, r)); });
                self.status_rx = Some(rx);
            }
        }
        if let Some(rx) = &self.status_rx {
            if let Ok((addr, r)) = rx.try_recv() {
                self.status_rx = None;
                if addr == self.realm {
                    self.realm_status = match r {
                        Reachability::Online => "Online",
                        Reachability::Unreachable => "Unreachable",
                        Reachability::Unconfigured => "No realm configured",
                    }.into();
                }
            }
        }
    }

    /// Hero art: UniformToFill in a box that overhangs the left edge by 80 px, drifting
    /// (scale 1 -> 1.09, shift 0 -> 26 px, 26 s, auto-reversing, sine in-out).
    fn paint_hero(&self, ui: &egui::Ui, full: egui::Rect, t: f32) {
        let phase = (t / 26.0) % 2.0;
        let p = if phase <= 1.0 { phase } else { 2.0 - phase };
        let e = 0.5 - 0.5 * (std::f32::consts::PI * p).cos();
        let scale = 1.0 + 0.09 * e;
        let shift = 26.0 * e;

        let b = egui::Rect::from_min_max(egui::pos2(full.left() - 80.0, full.top()), full.max);
        let tex = self.hero.size_vec2();
        let (box_a, img_a) = (b.width() / b.height(), tex.x / tex.y);
        let (u, v) = if img_a > box_a { (box_a / img_a, 1.0) } else { (1.0, img_a / box_a) };
        let uv = egui::Rect::from_min_max(egui::pos2(0.5 - u / 2.0, 0.5 - v / 2.0), egui::pos2(0.5 + u / 2.0, 0.5 + v / 2.0));
        let dest = egui::Rect::from_center_size(b.center() + egui::vec2(shift, 0.0), b.size() * scale);
        ui.painter().image(self.hero.id(), dest, uv, Color32::WHITE);

        // The wash: darkens the art left to right but never covers it.
        let stops = [(0.00, 0x00), (0.32, 0x00), (0.52, 0x8A), (0.68, 0xB8), (1.00, 0xB8)];
        let mut mesh = egui::Mesh::default();
        for (i, (x, a)) in stops.iter().enumerate() {
            let px = full.left() + full.width() * x;
            let c = Color32::from_black_alpha(*a);
            mesh.colored_vertex(egui::pos2(px, full.top()), c);
            mesh.colored_vertex(egui::pos2(px, full.bottom()), c);
            if i > 0 {
                let k = (i * 2) as u32;
                mesh.add_triangle(k - 2, k - 1, k);
                mesh.add_triangle(k - 1, k, k + 1);
            }
        }
        ui.painter().add(egui::Shape::mesh(mesh));
    }
}

impl eframe::App for App {
    fn update(&mut self, ctx: &egui::Context, _: &mut eframe::Frame) {
        self.poll(ctx);
        ctx.request_repaint_after(Duration::from_millis(33));
        let t = self.started.elapsed().as_secs_f32();

        egui::CentralPanel::default().frame(egui::Frame::none().fill(INK)).show(ctx, |ui| {
            let full = ui.max_rect();
            self.paint_hero(ui, full, t);

            // Logo: 140 px tall, centred on the page, 28 px from the top. Fades in and drops 18 px.
            let logo_a = progress(t, 0.0, 0.5);
            let logo_dy = -18.0 * (1.0 - ease_out_cubic(progress(t, 0.0, 0.6)));
            let tex = self.logo.size_vec2();
            let size = egui::vec2(140.0 * tex.x / tex.y, 140.0);
            let logo_rect = egui::Rect::from_min_size(egui::pos2(full.center().x - size.x / 2.0, full.top() + 28.0 + logo_dy), size);
            ui.painter().image(self.logo.id(), logo_rect, egui::Rect::from_min_max(egui::pos2(0.0, 0.0), egui::pos2(1.0, 1.0)), Color32::from_white_alpha((logo_a * 255.0) as u8));

            // Field column: 372 wide, 34 px left and 26 px right margins, slides in from the right.
            let panel_a = progress(t, 0.12, 0.5);
            let panel_dx = 34.0 * (1.0 - ease_out_cubic(progress(t, 0.12, 0.6)));
            let col = egui::Rect::from_min_max(
                egui::pos2(full.right() - PANEL_W + 34.0 + panel_dx, full.top()),
                egui::pos2(full.right() - 26.0 + panel_dx, full.bottom()),
            );

            // Version text sits bottom right, 16 px up.
            let galley = ui.painter().layout_no_wrap(self.version_text.clone(), FontId::proportional(11.0), Color32::WHITE);
            let vpos = egui::pos2(col.right() - galley.size().x, full.bottom() - 16.0 - galley.size().y);
            ui.painter().text(vpos, egui::Align2::LEFT_TOP, &self.version_text, FontId::proportional(11.0), tint(0xDC, 0xE4, 0xF2, (0x66 as f32 * panel_a) as u8));
            if !cfg!(windows) && self.update.available && self.install_rx.is_none() {
                let r = egui::Rect::from_min_size(egui::pos2(col.right() - 150.0, vpos.y - 30.0), egui::vec2(150.0, 24.0));
                if button(ui, &format!("Update to v{}", self.update.latest), FontId::proportional(12.0), r.size(), &GHOST).clicked() {
                    self.start_install();
                }
            }

            let top = full.top() + ((full.height() - self.content_h) / 2.0).max(0.0);
            let area = egui::Rect::from_min_max(egui::pos2(col.left(), top), col.right_bottom());
            let mut child = ui.new_child(egui::UiBuilder::new().max_rect(area).layout(egui::Layout::top_down(egui::Align::Min)));
            child.set_opacity(panel_a);
            child.spacing_mut().item_spacing = egui::vec2(0.0, 0.0);
            let w = col.width();
            let start_y = child.cursor().top();

            child.label(RichText::new(&self.realm_status).font(semibold(13.0)).color(TAN));
            child.add_space(14.0);

            field_label(&mut child, "ACCOUNT NAME");
            child.add(field(&mut self.account, "", false));
            child.add_space(14.0);

            field_label(&mut child, "PASSWORD");
            let pw = child.add(field(&mut self.password, "", true));
            let enter = pw.lost_focus() && child.input(|i| i.key_pressed(egui::Key::Enter));
            child.add_space(14.0);

            if checkbox(&mut child, &mut self.remember, "Remember me") && !self.loading {
                if !self.remember { creds::clear(); }
                self.save();
            }
            child.add_space(14.0);

            if button(&mut child, "Play", semibold(18.0), egui::vec2(w, 50.0), &PLAY).clicked() || enter {
                self.play();
            }
            if !self.error.is_empty() {
                child.add_space(14.0);
                child.add(egui::Label::new(RichText::new(&self.error).size(12.0).color(ERROR)).wrap());
            }
            child.add_space(28.0);

            field_label(&mut child, "WOW INSTALL PATH");
            child.horizontal(|ui| {
                ui.spacing_mut().item_spacing.x = 8.0;
                let mut shown = self.wow_path.clone();
                ui.add(field(&mut shown, "Not set", false).desired_width(w - 8.0 - 76.0 - 16.0));
                if button(ui, "Browse", FontId::proportional(14.0), egui::vec2(76.0, 36.0), &GHOST).clicked() {
                    self.browse();
                }
            });
            child.add_space(10.0);

            field_label(&mut child, "REALM ADDRESS");
            if child.add(field(&mut self.realm, "e.g. 192.168.1.100", false)).changed() {
                self.last_edit = Instant::now();
                self.save();
            }
            self.content_h = child.cursor().top() - start_y;
        });
    }
}

fn main() -> eframe::Result<()> {
    let icon = image::load_from_memory(ICON).ok().map(|i| {
        let i = i.to_rgba8();
        egui::IconData { width: i.width(), height: i.height(), rgba: i.into_raw() }
    });
    let mut viewport = egui::ViewportBuilder::default()
        .with_inner_size(WINDOW)
        .with_resizable(false)
        .with_maximize_button(false)
        .with_title("AzerothCore");
    if let Some(icon) = icon { viewport = viewport.with_icon(icon); }
    let options = eframe::NativeOptions { viewport, centered: true, ..Default::default() };
    eframe::run_native("AzerothCore", options, Box::new(|cc| {
        setup_fonts(&cc.egui_ctx);
        style(&cc.egui_ctx);
        Ok(Box::new(App::new(&cc.egui_ctx)))
    }))
}
