#include "wifi_manager.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <lvgl.h>

#include "display_config.h"
#include "ui_fonts.h"
#include "net_validate.h"
#include "../config/nvs_config.h"
#include "../i18n/i18n.h"

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------

static constexpr char     AP_SSID[]            = "HomeRemote-Setup";
static constexpr uint8_t  DNS_PORT             = 53;
static constexpr uint32_t CONNECT_TIMEOUT_MS   = 15000UL;
static constexpr uint32_t RECONNECT_INTERVAL_MS = 30000UL;

// ----------------------------------------------------------------------------
// Module state
// ----------------------------------------------------------------------------

static NetworkConfig s_config{};
static bool          s_has_config      = false;
static WebServer     s_server(80);
static DNSServer     s_dns;
static unsigned long s_last_reconnect_ms = 0;

// ----------------------------------------------------------------------------
// Captive portal HTML
// Stored as a raw string literal. ~1 KB — fits comfortably in heap.
// ----------------------------------------------------------------------------

static const char PORTAL_HTML[] = R"html(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Home Remote Setup</title>
  <style>
    *{box-sizing:border-box;margin:0;padding:0}
    body{font-family:sans-serif;background:#0d0d1a;color:#ddd;padding:20px;max-width:480px;margin:auto}
    h1{color:#fff;font-size:1.4em;margin-bottom:18px}
    label{display:block;margin-top:14px;font-size:.85em;color:#aaa;letter-spacing:.03em}
    input,textarea{width:100%;padding:10px;margin-top:5px;background:#16213e;border:1px solid #334;color:#fff;border-radius:6px;font-size:1em}
    textarea{height:90px;resize:vertical}
    .hint{font-size:.75em;color:#556;margin-top:4px}
    .err{color:#ff6b6b;font-size:.85em;margin-top:10px;padding:8px;background:#2a1020;border-radius:4px}
    button{margin-top:22px;width:100%;padding:14px;background:#1a4a8a;color:#fff;border:none;border-radius:6px;font-size:1.05em;cursor:pointer}
    button:active{background:#0f3060}
  </style>
</head>
<body>
  <h1>Home Remote Setup</h1>
  <form method="POST" action="/save">
    <label>Wi-Fi Network (SSID)</label>
    <input type="text" name="ssid" maxlength="32" required value="%SSID%">

    <label>Wi-Fi Password</label>
    <input type="password" name="password" maxlength="63" placeholder="Leave empty for open networks">

    <label>Home Assistant URL</label>
    <input type="text" name="ha_url" required value="%HA_URL%" placeholder="http://192.168.1.100:8123">
    <p class="hint">Include http:// or https:// and the port number.</p>

    <label>HA Long-Lived Access Token</label>
    <textarea name="ha_token" required placeholder="Paste your token here"></textarea>
    <p class="hint">HA &rarr; Profile &rarr; Long-Lived Access Tokens &rarr; Create Token</p>

    <button type="submit">Save &amp; Restart</button>
    %ERROR%
  </form>
</body>
</html>
)html";

// ----------------------------------------------------------------------------
// LVGL boot screen helpers
// All screens follow the same create → load → use → cleanup pattern.
// ----------------------------------------------------------------------------

static lv_obj_t* make_boot_screen(const char* title, uint32_t title_col)
{
    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0D0D1A), LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl = lv_label_create(scr);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_color(lbl, lv_color_hex(title_col), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 14);

    lv_scr_load(scr);
    return scr;
}

static void add_body_label(lv_obj_t* scr, const char* text, uint32_t color, int16_t y_ofs)
{
    lv_obj_t* lbl = lv_label_create(scr);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(lbl, SCREEN_WIDTH - 20);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, y_ofs);
}

static void dismiss_boot_screen(lv_obj_t* scr)
{
    lv_obj_t* blank = lv_obj_create(nullptr);
    lv_scr_load(blank);
    lv_obj_del(scr);
}

// ----------------------------------------------------------------------------
// Portal HTML helpers
// ----------------------------------------------------------------------------

static String build_portal_html(const char* ssid_val,
                                 const char* ha_url_val,
                                 const char* error_msg)
{
    String html(PORTAL_HTML);
    html.replace("%SSID%",   ssid_val  ? ssid_val  : "");
    html.replace("%HA_URL%", ha_url_val ? ha_url_val : "");

    if (error_msg && error_msg[0] != '\0') {
        String err_html = "<p class=\"err\">";
        err_html += error_msg;
        err_html += "</p>";
        html.replace("%ERROR%", err_html);
    } else {
        html.replace("%ERROR%", "");
    }

    return html;
}

// ----------------------------------------------------------------------------
// HTTP handlers (captive portal)
// ----------------------------------------------------------------------------

static void handle_root()
{
    s_server.send(200, "text/html", build_portal_html("", "", ""));
}

static void handle_save()
{
    String ssid     = s_server.arg("ssid");
    String password = s_server.arg("password");
    String ha_url   = s_server.arg("ha_url");
    String ha_token = s_server.arg("ha_token");

    // Trim whitespace that may have crept in (e.g. trailing newline in textarea)
    ssid.trim();
    ha_url.trim();
    ha_token.trim();

    // Validate
    if (!net_ssid_valid(ssid.c_str())) {
        s_server.send(200, "text/html",
            build_portal_html(ssid.c_str(), ha_url.c_str(),
                              "Wi-Fi SSID must be 1–32 characters."));
        return;
    }
    if (!net_url_valid(ha_url.c_str())) {
        s_server.send(200, "text/html",
            build_portal_html(ssid.c_str(), ha_url.c_str(),
                              "HA URL must start with http:// or https://."));
        return;
    }
    if (!net_token_valid(ha_token.c_str())) {
        s_server.send(200, "text/html",
            build_portal_html(ssid.c_str(), ha_url.c_str(),
                              "HA token appears too short — did you paste it correctly?"));
        return;
    }

    // Save to NVS
    NetworkConfig cfg{};
    ssid.toCharArray(cfg.ssid, sizeof(cfg.ssid));
    password.toCharArray(cfg.password, sizeof(cfg.password));
    ha_url.toCharArray(cfg.ha_url, sizeof(cfg.ha_url));
    ha_token.toCharArray(cfg.ha_token, sizeof(cfg.ha_token));
    nvs_config::save_net_config(cfg);

    Serial.println("[wifi] Credentials saved. Rebooting...");

    s_server.send(200, "text/html",
        "<html><body style='font-family:sans-serif;background:#0d0d1a;color:#fff;"
        "display:flex;align-items:center;justify-content:center;height:100vh;margin:0'>"
        "<div style='text-align:center'>"
        "<h2 style='color:#4caf50'>Saved!</h2>"
        "<p>Home Remote is restarting...</p>"
        "</div></body></html>");

    delay(1500);
    ESP.restart();
}

static void handle_not_found()
{
    // Redirect everything else to the setup form (triggers captive portal popup)
    s_server.sendHeader("Location", "http://192.168.4.1/", true);
    s_server.send(302, "text/plain", "");
}

// ----------------------------------------------------------------------------
// Portal display screen
// ----------------------------------------------------------------------------

static lv_obj_t* show_portal_screen()
{
    lv_obj_t* scr = make_boot_screen(i18n::str(StrId::WIFI_SETUP_MODE), 0xFFFFFF);

    add_body_label(scr, i18n::str(StrId::WIFI_SETUP_CONNECT), 0xFFDD44, -20);

    add_body_label(scr, i18n::str(StrId::WIFI_SETUP_BROWSER), 0x888888, 30);

    lv_timer_handler();
    return scr;
}

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

namespace wifi_manager {

void init()
{
    s_has_config = nvs_config::load_net_config(s_config);
    Serial.printf("[wifi] Config %s\n",
                  s_has_config ? "loaded from NVS" : "not found — portal needed");
}

bool has_config()  { return s_has_config; }
bool is_connected(){ return WiFi.status() == WL_CONNECTED; }

const NetworkConfig& config() { return s_config; }

void start_portal()
{
    Serial.println("[wifi] Starting captive portal...");

    lv_obj_t* portal_scr = show_portal_screen();
    (void)portal_scr;  // portal never returns — no need to clean up

    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID);
    Serial.printf("[wifi] AP IP: %s\n", WiFi.softAPIP().toString().c_str());

    s_dns.start(DNS_PORT, "*", WiFi.softAPIP());

    s_server.on("/",      HTTP_GET,  handle_root);
    s_server.on("/save",  HTTP_POST, handle_save);
    s_server.onNotFound(handle_not_found);
    s_server.begin();

    Serial.println("[wifi] Portal running — waiting for setup...");

    // Blocking loop — handle_save() calls ESP.restart(), we never return
    while (true) {
        s_dns.processNextRequest();
        s_server.handleClient();
        lv_timer_handler();
        delay(5);
    }
}

bool connect()
{
    if (!s_has_config) return false;

    Serial.printf("[wifi] Connecting to \"%s\"...\n", s_config.ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(s_config.ssid, s_config.password);
    WiFi.setAutoReconnect(true);

    // ---- Connecting screen ----
    lv_obj_t* scr = make_boot_screen(i18n::str(StrId::WIFI_CONNECTING), 0xFFFFFF);

    char line[80];
    snprintf(line, sizeof(line), i18n::str(StrId::WIFI_SSID_FMT), s_config.ssid);
    add_body_label(scr, line, 0xCCCCCC, -20);

    lv_obj_t* spinner = lv_spinner_create(scr, 1000, 60);
    lv_obj_set_size(spinner, 40, 40);
    lv_obj_align(spinner, LV_ALIGN_CENTER, 0, 30);

    lv_timer_handler();

    // ---- Wait for connection ----
    const unsigned long deadline = millis() + CONNECT_TIMEOUT_MS;
    while (WiFi.status() != WL_CONNECTED && millis() < deadline) {
        lv_timer_handler();
        delay(50);
    }

    dismiss_boot_screen(scr);

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[wifi] Connected — IP: %s\n",
                      WiFi.localIP().toString().c_str());

        lv_obj_t* ok_scr = make_boot_screen(i18n::str(StrId::WIFI_CONNECTED), 0x44DD44);
        snprintf(line, sizeof(line), i18n::str(StrId::WIFI_IP_FMT), WiFi.localIP().toString().c_str());
        add_body_label(ok_scr, line, 0xAAAAAA, 10);
        lv_timer_handler();
        delay(1500);
        dismiss_boot_screen(ok_scr);

        s_last_reconnect_ms = millis();
        return true;
    } else {
        Serial.println("[wifi] Connection failed — continuing offline");

        lv_obj_t* fail_scr = make_boot_screen(i18n::str(StrId::WIFI_NO_WIFI), 0xFF5555);
        add_body_label(fail_scr, i18n::str(StrId::WIFI_FAIL), 0xAAAAAA, 10);
        lv_timer_handler();
        delay(2000);
        dismiss_boot_screen(fail_scr);

        return false;
    }
}

void enter_setup_mode()
{
    Serial.println("[wifi] Entering setup mode...");
    nvs_config::clear_net_config();
    s_has_config = false;
    WiFi.disconnect(true);
    start_portal();  // does not return
}

void tick()
{
    if (WiFi.status() == WL_CONNECTED) {
        s_last_reconnect_ms = millis();
        return;
    }

    if (!s_has_config) return;

    const unsigned long now = millis();
    if (now - s_last_reconnect_ms < RECONNECT_INTERVAL_MS) return;

    s_last_reconnect_ms = now;
    Serial.println("[wifi] Disconnected — attempting reconnect...");
    WiFi.reconnect();
}

}  // namespace wifi_manager
