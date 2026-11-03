use chrono::Local;
use glib::ControlFlow;
use gtk4::prelude::*;
use gtk4::{Application, ApplicationWindow, Box as GtkBox, CssProvider, Label, Orientation};
use gtk4_layer_shell::{Layer, LayerShell};
use std::cell::RefCell;
use std::fs;
use std::rc::Rc;
use std::time::Duration;

const APP_ID: &str = "com.whrr.ClockOverlay";

fn style_conf_path() -> String {
    let home = std::env::var("HOME").unwrap_or_else(|_| String::from("/root"));
    format!("{}/.config/hypr/scripts/clock_style.conf", home)
}

fn build_clock_css() -> String {
    let mut opacity: f64 = 0.45;
    let mut day_size: i32 = 63;
    let mut date_size: i32 = 14;
    let mut time_size: i32 = 15;
    let mut spacing: i32 = 12;
    let mut pos_x: i32 = 0;
    let mut pos_y: i32 = 450;
    let mut shadow: f64 = 0.30;
    let mut r: u8 = 255;
    let mut g: u8 = 255;
    let mut b: u8 = 255;

    if let Ok(text) = fs::read_to_string(style_conf_path()) {
        for line in text.lines() {
            if let Some((k, v)) = line.split_once('=') {
                let val = v.trim();
                match k.trim() {
                    "opacity" => opacity = val.parse::<f64>().unwrap_or(0.45).clamp(0.05, 1.0),
                    "day_size" => day_size = val.parse::<f64>().unwrap_or(63.0).round() as i32,
                    "date_size" => date_size = val.parse::<f64>().unwrap_or(14.0).round() as i32,
                    "time_size" => time_size = val.parse::<f64>().unwrap_or(15.0).round() as i32,
                    "letter_spacing" => spacing = val.parse::<f64>().unwrap_or(12.0).round() as i32,
                    "pos_x" => pos_x = val.parse::<f64>().unwrap_or(0.0).round() as i32,
                    "pos_y" => pos_y = val.parse::<f64>().unwrap_or(450.0).round() as i32,
                    "shadow_alpha" => shadow = val.parse::<f64>().unwrap_or(0.30).clamp(0.0, 1.0),
                    "color_r" => r = val.parse::<f64>().unwrap_or(255.0).round() as u8,
                    "color_g" => g = val.parse::<f64>().unwrap_or(255.0).round() as u8,
                    "color_b" => b = val.parse::<f64>().unwrap_or(255.0).round() as u8,
                    _ => {}
                }
            }
        }
    }

    let (ml, mr) = if pos_x >= 0 { (pos_x, 0) } else { (0, -pos_x) };
    let (mb, mt) = if pos_y >= 0 { (pos_y, 0) } else { (0, -pos_y) };

    format!(
        "window {{ background-color: transparent; }}
         .main-box {{ margin-left: {}px; margin-right: {}px; margin-top: {}px; margin-bottom: {}px; }}
         .day-label {{ font-family: 'Anurati', 'Blanka', sans-serif; font-size: {}px; font-weight: bold; color: rgba({}, {}, {}, {:.2}); letter-spacing: {}px; text-shadow: 0px 4px 10px rgba(0,0,0,{:.2}); }}
         .date-label {{ font-family: sans-serif; font-size: {}px; font-weight: bold; color: rgba({}, {}, {}, {:.2}); margin-top: 8px; letter-spacing: 2px; text-shadow: 0px 2px 5px rgba(0,0,0,{:.2}); }}
         .time-label {{ font-family: sans-serif; font-size: {}px; font-weight: bold; color: rgba({}, {}, {}, {:.2}); margin-top: 8px; letter-spacing: 3px; text-shadow: 0px 2px 5px rgba(0,0,0,{:.2}); }}",
        ml, mr, mt, mb,
        day_size, r, g, b, opacity, spacing, shadow,
        date_size, r, g, b, opacity, shadow,
        time_size, r, g, b, opacity, shadow
    )
}

fn main() {
    let app = Application::builder().application_id(APP_ID).build();
    app.connect_activate(build_ui);
    app.run();
}

fn build_ui(app: &Application) {
    let window = ApplicationWindow::builder().application(app).title("Clock").build();

    window.init_layer_shell();
    window.set_layer(Layer::Bottom);
    window.set_namespace(Some("clock_overlay"));

    let vbox = GtkBox::new(Orientation::Vertical, 0);
    vbox.set_valign(gtk4::Align::Center);
    vbox.set_halign(gtk4::Align::Center);
    vbox.add_css_class("main-box");

    let day_label = Label::new(None);
    day_label.add_css_class("day-label");

    let date_label = Label::new(None);
    date_label.add_css_class("date-label");

    let time_label = Label::new(None);
    time_label.add_css_class("time-label");

    vbox.append(&day_label);
    vbox.append(&date_label);
    vbox.append(&time_label);
    window.set_child(Some(&vbox));

    let css_provider = CssProvider::new();
    let initial_css = build_clock_css();
    css_provider.load_from_data(&initial_css);

    gtk4::style_context_add_provider_for_display(
        &gtk4::gdk::Display::default().expect("Could not connect to a display."),
        &css_provider,
        gtk4::STYLE_PROVIDER_PRIORITY_APPLICATION,
    );

    // Отслеживаем изменения конфига стилей каждые 80 мс для плавной настройки ползунками
    let last_raw = Rc::new(RefCell::new(
        fs::read_to_string(style_conf_path()).unwrap_or_default(),
    ));
    let provider_clone = css_provider.clone();
    glib::timeout_add_local(Duration::from_millis(80), move || {
        let current_raw = fs::read_to_string(style_conf_path()).unwrap_or_default();
        if current_raw != *last_raw.borrow() {
            *last_raw.borrow_mut() = current_raw;
            provider_clone.load_from_data(&build_clock_css());
        }
        ControlFlow::Continue
    });

    let update_time = {
        let day_label = day_label.clone();
        let date_label = date_label.clone();
        let time_label = time_label.clone();
        move || {
            let now = Local::now();
            day_label.set_text(&now.format("%A").to_string().to_uppercase());
            date_label.set_text(&now.format("%d %B, %Y.").to_string().to_uppercase());
            time_label.set_text(&now.format("- %H:%M -").to_string());
            ControlFlow::Continue
        }
    };

    update_time();
    glib::timeout_add_seconds_local(1, update_time);
    window.present();
}
