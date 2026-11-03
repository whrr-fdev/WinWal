use glib::ControlFlow;
use gtk4::gdk::prelude::*;
use gtk4::prelude::*;
use gtk4::{
    Align, Application, ApplicationWindow, Box as GtkBox, CssProvider, DrawingArea, Label,
    Orientation,
};
use gtk4_layer_shell::{Edge, KeyboardMode, Layer, LayerShell};
use std::cell::RefCell;
use std::collections::VecDeque;
use std::fs;
use std::process::Command;
use std::rc::Rc;
use std::time::Duration;

const APP_ID: &str = "com.whrr.SysInfoOverlay";
const HISTORY_LEN: usize = 60;

#[derive(Clone, PartialEq)]
struct StyleConfig {
    show_cpu: bool,
    show_ram: bool,
    show_net: bool,
    show_temp: bool,
    show_extra: bool,
    top_layer: bool,
    text_color: String,
    graph_color: String,
    opacity: f64,
    scale: f64,
    title_size: i32,
    extra_size: i32,
    margin_x: i32,
    margin_y: i32,
    position: String,
}

impl Default for StyleConfig {
    fn default() -> Self {
        Self {
            show_cpu: true,
            show_ram: true,
            show_net: true,
            show_temp: true,
            show_extra: true,
            top_layer: true,
            text_color: String::from("#ffffff"),
            graph_color: String::from("#ffffff"),
            opacity: 0.85,
            scale: 1.0,
            title_size: 13,
            extra_size: 14,
            margin_x: 24,
            margin_y: 44,
            position: String::from("top-right"),
        }
    }
}

struct AppState {
    cfg: StyleConfig,
    cpu_hist: VecDeque<f64>,
    ram_hist: VecDeque<f64>,
    net_rx_hist: VecDeque<f64>,
    net_tx_hist: VecDeque<f64>,
    temp_hist: VecDeque<f64>,
    prev_cpu_total: u64,
    prev_cpu_idle: u64,
    prev_net_rx: u64,
    prev_net_tx: u64,
    tick_count: u64,
    cached_disk: String,
    cached_kernel: String,
}

fn style_conf_path() -> String {
    let home = std::env::var("HOME").unwrap_or_else(|_| String::from("/root"));
    format!("{}/.config/hypr/scripts/sysinfo_style.conf", home)
}

fn parse_bool(val: &str) -> bool {
    matches!(val, "1" | "true" | "True" | "yes")
}

fn parse_hex_color(hex: &str) -> (u8, u8, u8) {
    let s = hex.trim().trim_start_matches('#');
    if s.len() >= 6 {
        let r = u8::from_str_radix(&s[0..2], 16).unwrap_or(255);
        let g = u8::from_str_radix(&s[2..4], 16).unwrap_or(255);
        let b = u8::from_str_radix(&s[4..6], 16).unwrap_or(255);
        (r, g, b)
    } else {
        (255, 255, 255)
    }
}

fn load_config() -> StyleConfig {
    let mut cfg = StyleConfig::default();
    if let Ok(text) = fs::read_to_string(style_conf_path()) {
        for line in text.lines() {
            if let Some((k, v)) = line.split_once('=') {
                let val = v.trim();
                match k.trim() {
                    "show_cpu" => cfg.show_cpu = parse_bool(val),
                    "show_ram" => cfg.show_ram = parse_bool(val),
                    "show_net" => cfg.show_net = parse_bool(val),
                    "show_temp" => cfg.show_temp = parse_bool(val),
                    "show_extra" => cfg.show_extra = parse_bool(val),
                    "top_layer" => cfg.top_layer = parse_bool(val),
                    "text_color" => {
                        if !val.is_empty() {
                            cfg.text_color = val.to_string();
                        }
                    }
                    "graph_color" => {
                        if !val.is_empty() {
                            cfg.graph_color = val.to_string();
                        }
                    }
                    "opacity" => cfg.opacity = val.parse::<f64>().unwrap_or(0.85).clamp(0.1, 1.0),
                    "scale" => cfg.scale = val.parse::<f64>().unwrap_or(1.0).clamp(0.4, 2.5),
                    "title_size" => cfg.title_size = val.parse::<f64>().unwrap_or(13.0).round().clamp(6.0, 36.0) as i32,
                    "extra_size" => cfg.extra_size = val.parse::<f64>().unwrap_or(14.0).round().clamp(6.0, 36.0) as i32,
                    "margin_x" => cfg.margin_x = val.parse::<f64>().unwrap_or(24.0).round() as i32,
                    "margin_y" => cfg.margin_y = val.parse::<f64>().unwrap_or(44.0).round() as i32,
                    "position" => {
                        if !val.is_empty() {
                            cfg.position = val.to_string();
                        }
                    }
                    _ => {}
                }
            }
        }
    }
    cfg
}

fn build_css(cfg: &StyleConfig) -> String {
    let (r, g, b) = parse_hex_color(&cfg.text_color);
    let alpha = cfg.opacity;
    let sub_alpha = (cfg.opacity * 0.92).clamp(0.1, 1.0);
    let pad = (10.0 * cfg.scale).round().max(4.0) as i32;
    let title_sz = (cfg.title_size as f64 * cfg.scale).round().max(5.0) as i32;
    let extra_sz = (cfg.extra_size as f64 * cfg.scale).round().max(5.0) as i32;

    format!(
        "window {{ background-color: transparent; min-width: 0px; min-height: 0px; }}
         .sys-root {{ padding: {}px; background-color: transparent; min-width: 0px; min-height: 0px; }}
         .metric-title {{ font-family: 'JetBrainsMono Nerd Font', monospace, sans-serif; font-size: {}px; font-weight: bold; color: rgba({}, {}, {}, {:.2}); text-shadow: 0px 1px 4px rgba(0,0,0,0.45); }}
         .metric-value {{ font-family: 'JetBrainsMono Nerd Font', monospace, sans-serif; font-size: {}px; font-weight: bold; color: rgba({}, {}, {}, {:.2}); text-shadow: 0px 1px 4px rgba(0,0,0,0.45); }}
         .extra-label {{ font-family: 'JetBrainsMono Nerd Font', monospace, sans-serif; font-size: {}px; font-weight: bold; color: rgba({}, {}, {}, {:.2}); text-shadow: 0px 1px 4px rgba(0,0,0,0.45); }}",
        pad,
        title_sz, r, g, b, alpha,
        title_sz, r, g, b, alpha,
        extra_sz, r, g, b, sub_alpha
    )
}

fn apply_window_placement(window: &ApplicationWindow, cfg: &StyleConfig) {
    window.set_layer(if cfg.top_layer {
        Layer::Overlay
    } else {
        Layer::Bottom
    });

    let pos = cfg.position.as_str();
    let top = pos.starts_with("top");
    let bottom = pos.starts_with("bottom");
    let left = pos.ends_with("left");
    let right = pos.ends_with("right");

    window.set_anchor(Edge::Top, top);
    window.set_anchor(Edge::Bottom, bottom);
    window.set_anchor(Edge::Left, left);
    window.set_anchor(Edge::Right, right);

    window.set_margin(Edge::Top, if top { cfg.margin_y } else { 0 });
    window.set_margin(Edge::Bottom, if bottom { cfg.margin_y } else { 0 });
    window.set_margin(Edge::Left, if left { cfg.margin_x } else { 0 });
    window.set_margin(Edge::Right, if right { cfg.margin_x } else { 0 });
}

fn apply_scale_dimensions(
    window: &ApplicationWindow,
    root_box: &GtkBox,
    sections: &[&GtkBox],
    headers: &[&GtkBox],
    areas: &[&DrawingArea],
    extra_box: &GtkBox,
    cfg: &StyleConfig,
) {
    let scale = cfg.scale;
    let max_font = cfg.title_size.max(cfg.extra_size) as f64;
    let font_factor = (max_font / 12.0).max(0.85);
    let gw = (260.0 * scale * font_factor).round().max(90.0) as i32;
    let gh = (36.0 * scale).round().max(14.0) as i32;
    let root_spacing = (10.0 * scale).round().max(2.0) as i32;
    let sec_spacing = (4.0 * scale).round().max(1.0) as i32;
    let hdr_spacing = (6.0 * scale).round().max(2.0) as i32;
    let extra_spacing = (3.5 * scale).round().max(1.0) as i32;

    root_box.set_spacing(root_spacing);
    extra_box.set_spacing(extra_spacing);
    extra_box.set_size_request(gw, -1);

    for sec in sections {
        sec.set_spacing(sec_spacing);
        sec.set_size_request(gw, -1);
    }
    for hdr in headers {
        hdr.set_spacing(hdr_spacing);
        hdr.set_size_request(gw, -1);
    }
    for area in areas {
        area.set_content_width(gw);
        area.set_content_height(gh);
        area.set_size_request(gw, gh);
    }

    window.set_default_size(1, 1);
    root_box.queue_resize();
}

fn read_cpu_raw() -> (u64, u64) {
    if let Ok(text) = fs::read_to_string("/proc/stat") {
        if let Some(line) = text.lines().next() {
            let parts: Vec<u64> = line
                .split_whitespace()
                .skip(1)
                .filter_map(|s| s.parse::<u64>().ok())
                .collect();
            if parts.len() >= 4 {
                let total: u64 = parts.iter().sum();
                let idle: u64 = parts[3] + parts.get(4).copied().unwrap_or(0);
                return (total, idle);
            }
        }
    }
    (0, 0)
}

fn read_cpu_freq_ghz() -> f64 {
    if let Ok(text) = fs::read_to_string("/proc/cpuinfo") {
        let mut sum = 0.0;
        let mut count = 0;
        for line in text.lines() {
            if line.starts_with("cpu MHz") {
                if let Some((_, v)) = line.split_once(':') {
                    if let Ok(mhz) = v.trim().parse::<f64>() {
                        sum += mhz;
                        count += 1;
                    }
                }
            }
        }
        if count > 0 {
            return (sum / count as f64) / 1000.0;
        }
    }
    0.0
}

fn read_ram_stats() -> (f64, f64, f64) {
    let mut total_kb = 0u64;
    let mut avail_kb = 0u64;
    if let Ok(text) = fs::read_to_string("/proc/meminfo") {
        for line in text.lines() {
            if line.starts_with("MemTotal:") {
                total_kb = line
                    .split_whitespace()
                    .nth(1)
                    .and_then(|s| s.parse().ok())
                    .unwrap_or(0);
            } else if line.starts_with("MemAvailable:") {
                avail_kb = line
                    .split_whitespace()
                    .nth(1)
                    .and_then(|s| s.parse().ok())
                    .unwrap_or(0);
            }
        }
    }
    if total_kb == 0 {
        return (0.0, 0.0, 0.0);
    }
    let used_kb = total_kb.saturating_sub(avail_kb);
    let used_gb = used_kb as f64 / 1048576.0;
    let total_gb = total_kb as f64 / 1048576.0;
    let pct = (used_kb as f64 / total_kb as f64) * 100.0;
    (used_gb, total_gb, pct)
}

fn read_net_bytes() -> (u64, u64) {
    let mut rx_sum = 0u64;
    let mut tx_sum = 0u64;
    if let Ok(text) = fs::read_to_string("/proc/net/dev") {
        for line in text.lines().skip(2) {
            if let Some((iface, data)) = line.split_once(':') {
                if iface.trim() == "lo" {
                    continue;
                }
                let cols: Vec<&str> = data.split_whitespace().collect();
                if cols.len() >= 9 {
                    rx_sum += cols[0].parse::<u64>().unwrap_or(0);
                    tx_sum += cols[8].parse::<u64>().unwrap_or(0);
                }
            }
        }
    }
    (rx_sum, tx_sum)
}

fn format_speed(bytes_per_sec: f64) -> String {
    if bytes_per_sec >= 1_048_576.0 {
        format!("{:.1} MB/s", bytes_per_sec / 1_048_576.0)
    } else {
        format!("{:.0} KB/s", bytes_per_sec / 1024.0)
    }
}

fn read_temperature() -> f64 {
    if let Ok(entries) = fs::read_dir("/sys/class/hwmon") {
        let mut fallback = 0.0;
        for entry in entries.flatten() {
            let path = entry.path();
            let name = fs::read_to_string(path.join("name"))
                .unwrap_or_default()
                .trim()
                .to_string();
            if let Ok(raw) = fs::read_to_string(path.join("temp1_input")) {
                if let Ok(milli) = raw.trim().parse::<f64>() {
                    let c = milli / 1000.0;
                    if c > 0.0 && c < 130.0 {
                        if name.contains("k10temp")
                            || name.contains("coretemp")
                            || name.contains("cpu")
                            || name.contains("zenpower")
                        {
                            return c;
                        }
                        if fallback == 0.0 {
                            fallback = c;
                        }
                    }
                }
            }
        }
        if fallback > 0.0 {
            return fallback;
        }
    }
    if let Ok(raw) = fs::read_to_string("/sys/class/thermal/thermal_zone0/temp") {
        if let Ok(milli) = raw.trim().parse::<f64>() {
            return milli / 1000.0;
        }
    }
    0.0
}

fn read_disk_root() -> String {
    if let Ok(out) = Command::new("df").args(["-k", "/"]).output() {
        let s = String::from_utf8_lossy(&out.stdout);
        if let Some(line) = s.lines().nth(1) {
            let cols: Vec<&str> = line.split_whitespace().collect();
            if cols.len() >= 5 {
                let total_kb = cols[1].parse::<f64>().unwrap_or(0.0);
                let used_kb = cols[2].parse::<f64>().unwrap_or(0.0);
                if total_kb > 0.0 {
                    let used_gb = used_kb / 1_048_576.0;
                    let total_gb = total_kb / 1_048_576.0;
                    let pct = (used_kb / total_kb) * 100.0;
                    return format!("DISK (/): {:.1} / {:.1} GB ({:.0}%)", used_gb, total_gb, pct);
                }
            }
        }
    }
    String::from("DISK (/): N/A")
}

fn read_uptime_and_load() -> String {
    let mut up_secs = 0u64;
    if let Ok(text) = fs::read_to_string("/proc/uptime") {
        if let Some(first) = text.split_whitespace().next() {
            up_secs = first.parse::<f64>().unwrap_or(0.0) as u64;
        }
    }
    let hours = up_secs / 3600;
    let mins = (up_secs % 3600) / 60;

    let mut load_str = String::from("0.00 0.00");
    if let Ok(text) = fs::read_to_string("/proc/loadavg") {
        let parts: Vec<&str> = text.split_whitespace().collect();
        if parts.len() >= 2 {
            load_str = format!("{} {}", parts[0], parts[1]);
        }
    }
    format!("UPTIME:   {:02}h {:02}m  •  LOAD: {}", hours, mins, load_str)
}

fn draw_single_graph(
    cr: &gtk4::cairo::Context,
    w: i32,
    h: i32,
    data: &VecDeque<f64>,
    max_val: f64,
    color_hex: &str,
    opacity: f64,
    scale: f64,
) {
    let wf = w as f64;
    let hf = h as f64;
    let (r8, g8, b8) = parse_hex_color(color_hex);
    let (r, g, b) = (r8 as f64 / 255.0, g8 as f64 / 255.0, b8 as f64 / 255.0);

    cr.set_source_rgba(r, g, b, 0.22 * opacity);
    cr.set_line_width(1.0 * scale.clamp(0.6, 2.0));
    cr.move_to(0.0, hf - 1.0);
    cr.line_to(wf, hf - 1.0);
    let _ = cr.stroke();

    if data.len() < 2 {
        return;
    }

    let eff_max = max_val.max(1.0);
    let step = wf / (HISTORY_LEN.saturating_sub(1) as f64);

    cr.move_to(0.0, hf - 1.0);
    for (i, &val) in data.iter().enumerate() {
        let x = i as f64 * step;
        let norm = (val / eff_max).clamp(0.0, 1.0);
        let y = (hf - 2.0) - norm * (hf - 4.0);
        cr.line_to(x, y);
    }
    let last_x = (data.len().saturating_sub(1) as f64) * step;
    cr.line_to(last_x, hf - 1.0);
    cr.close_path();
    cr.set_source_rgba(r, g, b, 0.16 * opacity);
    let _ = cr.fill();

    for (i, &val) in data.iter().enumerate() {
        let x = i as f64 * step;
        let norm = (val / eff_max).clamp(0.0, 1.0);
        let y = (hf - 2.0) - norm * (hf - 4.0);
        if i == 0 {
            cr.move_to(x, y);
        } else {
            cr.line_to(x, y);
        }
    }
    cr.set_source_rgba(r, g, b, 0.95 * opacity);
    cr.set_line_width(1.8 * scale.clamp(0.6, 2.0));
    let _ = cr.stroke();
}

fn main() {
    let app = Application::builder().application_id(APP_ID).build();
    app.connect_activate(build_ui);
    app.run();
}

fn make_metric_section(title: &str) -> (GtkBox, GtkBox, Label, DrawingArea) {
    let section = GtkBox::new(Orientation::Vertical, 4);
    section.set_halign(Align::Start);
    section.set_valign(Align::Start);
    section.set_hexpand(false);

    let header = GtkBox::new(Orientation::Horizontal, 6);
    header.set_halign(Align::Fill);

    let title_lbl = Label::new(Some(title));
    title_lbl.add_css_class("metric-title");
    title_lbl.set_halign(Align::Start);
    title_lbl.set_hexpand(true);

    let val_lbl = Label::new(Some("..."));
    val_lbl.add_css_class("metric-value");
    val_lbl.set_halign(Align::End);

    header.append(&title_lbl);
    header.append(&val_lbl);

    let area = DrawingArea::new();
    area.set_halign(Align::Start);
    area.set_valign(Align::Start);
    area.set_hexpand(false);
    area.set_vexpand(false);

    section.append(&header);
    section.append(&area);
    (section, header, val_lbl, area)
}

fn build_ui(app: &Application) {
    let initial_cfg = load_config();
    let (cpu_tot, cpu_idl) = read_cpu_raw();
    let (net_rx, net_tx) = read_net_bytes();
    let kernel = fs::read_to_string("/proc/sys/kernel/osrelease")
        .unwrap_or_else(|_| String::from("Linux"))
        .trim()
        .to_string();

    let state = Rc::new(RefCell::new(AppState {
        cfg: initial_cfg.clone(),
        cpu_hist: VecDeque::from(vec![0.0; HISTORY_LEN]),
        ram_hist: VecDeque::from(vec![0.0; HISTORY_LEN]),
        net_rx_hist: VecDeque::from(vec![0.0; HISTORY_LEN]),
        net_tx_hist: VecDeque::from(vec![0.0; HISTORY_LEN]),
        temp_hist: VecDeque::from(vec![0.0; HISTORY_LEN]),
        prev_cpu_total: cpu_tot,
        prev_cpu_idle: cpu_idl,
        prev_net_rx: net_rx,
        prev_net_tx: net_tx,
        tick_count: 0,
        cached_disk: read_disk_root(),
        cached_kernel: format!("KERNEL:   {}", kernel),
    }));

    let window = ApplicationWindow::builder()
        .application(app)
        .title("SysInfo Overlay")
        .default_width(1)
        .default_height(1)
        .build();

    window.init_layer_shell();
    window.set_namespace(Some("sysinfo_overlay"));
    window.set_keyboard_mode(KeyboardMode::None);
    window.set_exclusive_zone(-1);
    window.set_can_target(false);
    apply_window_placement(&window, &initial_cfg);

    window.connect_realize(|w| {
        if let Some(surface) = w.surface() {
            let empty_region = gtk4::cairo::Region::create();
            surface.set_input_region(Some(&empty_region));
        }
    });

    let root_box = GtkBox::new(Orientation::Vertical, 10);
    root_box.add_css_class("sys-root");
    root_box.set_halign(Align::Start);
    root_box.set_valign(Align::Start);
    root_box.set_hexpand(false);
    root_box.set_vexpand(false);

    let (cpu_box, cpu_hdr, cpu_val, cpu_area) = make_metric_section("󰻠  CPU");
    let (ram_box, ram_hdr, ram_val, ram_area) = make_metric_section("󰍛  RAM");
    let (net_box, net_hdr, net_val, net_area) = make_metric_section("󰛳  NET");
    let (temp_box, temp_hdr, temp_val, temp_area) = make_metric_section("  TEMP");

    let extra_box = GtkBox::new(Orientation::Vertical, 4);
    extra_box.set_halign(Align::Start);
    extra_box.set_valign(Align::Start);
    extra_box.set_hexpand(false);

    let disk_lbl = Label::new(Some(&state.borrow().cached_disk));
    disk_lbl.add_css_class("extra-label");
    disk_lbl.set_halign(Align::Start);

    let up_lbl = Label::new(Some(&read_uptime_and_load()));
    up_lbl.add_css_class("extra-label");
    up_lbl.set_halign(Align::Start);

    let kern_lbl = Label::new(Some(&state.borrow().cached_kernel));
    kern_lbl.add_css_class("extra-label");
    kern_lbl.set_halign(Align::Start);

    extra_box.append(&disk_lbl);
    extra_box.append(&up_lbl);
    extra_box.append(&kern_lbl);

    apply_scale_dimensions(
        &window,
        &root_box,
        &[&cpu_box, &ram_box, &net_box, &temp_box],
        &[&cpu_hdr, &ram_hdr, &net_hdr, &temp_hdr],
        &[&cpu_area, &ram_area, &net_area, &temp_area],
        &extra_box,
        &initial_cfg,
    );

    cpu_box.set_visible(initial_cfg.show_cpu);
    ram_box.set_visible(initial_cfg.show_ram);
    net_box.set_visible(initial_cfg.show_net);
    temp_box.set_visible(initial_cfg.show_temp);
    extra_box.set_visible(initial_cfg.show_extra);

    root_box.append(&cpu_box);
    root_box.append(&ram_box);
    root_box.append(&net_box);
    root_box.append(&temp_box);
    root_box.append(&extra_box);
    window.set_child(Some(&root_box));

    {
        let st = state.clone();
        cpu_area.set_draw_func(move |_, cr, w, h| {
            let s = st.borrow();
            draw_single_graph(cr, w, h, &s.cpu_hist, 100.0, &s.cfg.graph_color, s.cfg.opacity, s.cfg.scale);
        });
    }
    {
        let st = state.clone();
        ram_area.set_draw_func(move |_, cr, w, h| {
            let s = st.borrow();
            draw_single_graph(cr, w, h, &s.ram_hist, 100.0, &s.cfg.graph_color, s.cfg.opacity, s.cfg.scale);
        });
    }
    {
        let st = state.clone();
        net_area.set_draw_func(move |_, cr, w, h| {
            let s = st.borrow();
            let max_rx = s.net_rx_hist.iter().cloned().fold(10240.0_f64, f64::max);
            let max_tx = s.net_tx_hist.iter().cloned().fold(10240.0_f64, f64::max);
            let scale_val = max_rx.max(max_tx);
            draw_single_graph(cr, w, h, &s.net_rx_hist, scale_val, &s.cfg.graph_color, s.cfg.opacity, s.cfg.scale);
            draw_single_graph(
                cr,
                w,
                h,
                &s.net_tx_hist,
                scale_val,
                &s.cfg.graph_color,
                s.cfg.opacity * 0.45,
                s.cfg.scale,
            );
        });
    }
    {
        let st = state.clone();
        temp_area.set_draw_func(move |_, cr, w, h| {
            let s = st.borrow();
            draw_single_graph(cr, w, h, &s.temp_hist, 100.0, &s.cfg.graph_color, s.cfg.opacity, s.cfg.scale);
        });
    }

    let css_provider = CssProvider::new();
    css_provider.load_from_data(&build_css(&initial_cfg));
    gtk4::style_context_add_provider_for_display(
        &gtk4::gdk::Display::default().expect("Could not connect to a display."),
        &css_provider,
        gtk4::STYLE_PROVIDER_PRIORITY_APPLICATION,
    );

    let last_raw = Rc::new(RefCell::new(
        fs::read_to_string(style_conf_path()).unwrap_or_default(),
    ));
    {
        let st = state.clone();
        let provider = css_provider.clone();
        let win = window.clone();
        let r_root = root_box.clone();
        let c_box = cpu_box.clone();
        let r_box = ram_box.clone();
        let n_box = net_box.clone();
        let t_box = temp_box.clone();
        let c_hdr = cpu_hdr.clone();
        let r_hdr = ram_hdr.clone();
        let n_hdr = net_hdr.clone();
        let t_hdr = temp_hdr.clone();
        let e_box = extra_box.clone();
        let c_area = cpu_area.clone();
        let r_area = ram_area.clone();
        let n_area = net_area.clone();
        let t_area = temp_area.clone();

        glib::timeout_add_local(Duration::from_millis(80), move || {
            let current_raw = fs::read_to_string(style_conf_path()).unwrap_or_default();
            if current_raw != *last_raw.borrow() {
                *last_raw.borrow_mut() = current_raw;
                let new_cfg = load_config();
                provider.load_from_data(&build_css(&new_cfg));
                apply_window_placement(&win, &new_cfg);

                c_box.set_visible(new_cfg.show_cpu);
                r_box.set_visible(new_cfg.show_ram);
                n_box.set_visible(new_cfg.show_net);
                t_box.set_visible(new_cfg.show_temp);
                e_box.set_visible(new_cfg.show_extra);

                apply_scale_dimensions(
                    &win,
                    &r_root,
                    &[&c_box, &r_box, &n_box, &t_box],
                    &[&c_hdr, &r_hdr, &n_hdr, &t_hdr],
                    &[&c_area, &r_area, &n_area, &t_area],
                    &e_box,
                    &new_cfg,
                );

                st.borrow_mut().cfg = new_cfg;
                c_area.queue_draw();
                r_area.queue_draw();
                n_area.queue_draw();
                t_area.queue_draw();
            }
            ControlFlow::Continue
        });
    }

    {
        let st = state.clone();
        glib::timeout_add_seconds_local(1, move || {
            let mut s = st.borrow_mut();
            s.tick_count += 1;

            let (tot, idl) = read_cpu_raw();
            let d_tot = tot.saturating_sub(s.prev_cpu_total);
            let d_idl = idl.saturating_sub(s.prev_cpu_idle);
            s.prev_cpu_total = tot;
            s.prev_cpu_idle = idl;
            let cpu_pct = if d_tot > 0 {
                ((d_tot.saturating_sub(d_idl)) as f64 / d_tot as f64) * 100.0
            } else {
                0.0
            };
            s.cpu_hist.pop_front();
            s.cpu_hist.push_back(cpu_pct);
            let ghz = read_cpu_freq_ghz();
            cpu_val.set_text(&format!("{:.1}%  •  {:.2} GHz", cpu_pct, ghz));

            let (used_gb, tot_gb, ram_pct) = read_ram_stats();
            s.ram_hist.pop_front();
            s.ram_hist.push_back(ram_pct);
            ram_val.set_text(&format!("{:.1} / {:.1} GB ({:.0}%)", used_gb, tot_gb, ram_pct));

            let (rx, tx) = read_net_bytes();
            let d_rx = rx.saturating_sub(s.prev_net_rx) as f64;
            let d_tx = tx.saturating_sub(s.prev_net_tx) as f64;
            s.prev_net_rx = rx;
            s.prev_net_tx = tx;
            s.net_rx_hist.pop_front();
            s.net_rx_hist.push_back(d_rx);
            s.net_tx_hist.pop_front();
            s.net_tx_hist.push_back(d_tx);
            net_val.set_text(&format!("↓ {}   ↑ {}", format_speed(d_rx), format_speed(d_tx)));

            let temp_c = read_temperature();
            s.temp_hist.pop_front();
            s.temp_hist.push_back(temp_c);
            temp_val.set_text(&format!("{:.1} °C", temp_c));

            if s.tick_count % 5 == 0 {
                s.cached_disk = read_disk_root();
                disk_lbl.set_text(&s.cached_disk);
            }
            up_lbl.set_text(&read_uptime_and_load());

            cpu_area.queue_draw();
            ram_area.queue_draw();
            net_area.queue_draw();
            temp_area.queue_draw();

            ControlFlow::Continue
        });
    }

    window.present();
}
