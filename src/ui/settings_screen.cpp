#include "settings_screen.h"

#include <lvgl.h>
#include <cstring>
#include <cstdio>

#ifdef ARDUINO
#  include <Arduino.h>
#endif

#include "ui_theme.h"
#include "ui_fonts.h"
#include "ui_icons.h"
#include "display_config.h"
#include "config/nvs_config.h"
#include "touch/touch_driver.h"
#include "nb/nb_client.h"
#include "sleep/sleep_manager.h"
#include "i18n/i18n.h"
#include "shell.h"

// ----------------------------------------------------------------------------
// Module-private state
// ----------------------------------------------------------------------------

namespace {

static constexpr int SETTINGS_HEADER_H = 44;

// Screen objects
static lv_obj_t* s_screen      = nullptr;
static lv_obj_t* s_prev_screen = nullptr;

// Language buttons (kept for active-state styling)
static lv_obj_t* s_de_btn = nullptr;
static lv_obj_t* s_en_btn = nullptr;

// Brightness slider and its value label
static lv_obj_t* s_brt_slider  = nullptr;
static lv_obj_t* s_brt_val_lbl = nullptr;

// OTA (nano_backbone) section
static lv_obj_t* s_nb_status_lbl  = nullptr;
static lv_obj_t* s_nb_reg_btn     = nullptr;
static lv_obj_t* s_nb_update_btn  = nullptr;

// Battery / mobile mode section (TICKET-025)
static lv_obj_t* s_bat_off_btn   = nullptr;
static lv_obj_t* s_bat_on_btn    = nullptr;
static lv_obj_t* s_tmout_10_btn  = nullptr;
static lv_obj_t* s_tmout_30_btn  = nullptr;
static lv_obj_t* s_tmout_60_btn  = nullptr;

// OTA progress screen (shown while flashing)
static lv_obj_t* s_ota_screen  = nullptr;
static lv_obj_t* s_ota_bar     = nullptr;
static lv_obj_t* s_ota_status  = nullptr;

// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

static void apply_brightness(uint8_t level)
{
#ifdef ARDUINO
    ledcWrite(BL_LEDC_CHANNEL, level);
#else
    (void)level;
#endif
}

static void style_lang_btn(lv_obj_t* btn, bool active)
{
    lv_obj_set_style_bg_color(btn,
        lv_color_hex(active ? UI_COL_NAV_ACTIVE : UI_COL_SURFACE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_TOP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, active ? 2 : 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_hex(UI_COL_ACCENT), LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 4, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    // Pressed feedback
    lv_obj_set_style_bg_color(btn, lv_color_hex(UI_COL_NAV_ACTIVE),
                              LV_PART_MAIN | LV_STATE_PRESSED);
}

// ----------------------------------------------------------------------------
// LVGL event callbacks
// ----------------------------------------------------------------------------

static void on_back_click(lv_event_t* /*e*/)
{
    if (!s_screen || !s_prev_screen) return;
    lv_obj_t* prev = s_prev_screen;
    s_screen       = nullptr;
    s_prev_screen  = nullptr;
    s_de_btn = s_en_btn = nullptr;
    s_brt_slider = s_brt_val_lbl = nullptr;
    s_nb_status_lbl = s_nb_reg_btn = s_nb_update_btn = nullptr;
    s_bat_off_btn = s_bat_on_btn = nullptr;
    s_tmout_10_btn = s_tmout_30_btn = s_tmout_60_btn = nullptr;
    // auto_del=true: LVGL deletes the settings screen after the fade-out
    lv_scr_load_anim(prev, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, true);
}

static void on_de_click(lv_event_t* /*e*/)
{
    if (i18n::get_locale() == Locale::DE) return;
    i18n::set_locale(Locale::DE);  // persists to NVS
    // Null out before shell::create() so this screen can be safely deleted
    // by the fade animation that shell::create() triggers (auto_del=true on
    // lv_scr_load_anim will delete the current active screen — this one).
    s_screen = s_prev_screen = nullptr;
    s_de_btn = s_en_btn = nullptr;
    s_brt_slider = s_brt_val_lbl = nullptr;
    s_nb_status_lbl = s_nb_reg_btn = s_nb_update_btn = nullptr;
    s_bat_off_btn = s_bat_on_btn = nullptr;
    s_tmout_10_btn = s_tmout_30_btn = s_tmout_60_btn = nullptr;
    shell::create();  // rebuilds all UI strings; deletes old shell + fades out this screen
}

static void on_en_click(lv_event_t* /*e*/)
{
    if (i18n::get_locale() == Locale::EN) return;
    i18n::set_locale(Locale::EN);
    s_screen = s_prev_screen = nullptr;
    s_de_btn = s_en_btn = nullptr;
    s_brt_slider = s_brt_val_lbl = nullptr;
    s_nb_status_lbl = s_nb_reg_btn = s_nb_update_btn = nullptr;
    s_bat_off_btn = s_bat_on_btn = nullptr;
    s_tmout_10_btn = s_tmout_30_btn = s_tmout_60_btn = nullptr;
    shell::create();
}

static void on_brightness_changed(lv_event_t* e)
{
    lv_obj_t* slider = lv_event_get_target(e);
    const int val = lv_slider_get_value(slider);
    apply_brightness(static_cast<uint8_t>(val));
    if (s_brt_val_lbl) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", val * 100 / 255);
        lv_label_set_text(s_brt_val_lbl, buf);
    }
}

static void on_brightness_released(lv_event_t* e)
{
    lv_obj_t* slider = lv_event_get_target(e);
    UiSettings s;
    s.brightness = static_cast<uint8_t>(lv_slider_get_value(slider));
    nvs_config::save_ui_settings(s);
}

static void on_nb_register_click(lv_event_t* /*e*/)
{
    // Disable button and show "..." while the blocking HTTP call runs
    if (s_nb_reg_btn)    lv_obj_add_state(s_nb_reg_btn, LV_STATE_DISABLED);
    if (s_nb_status_lbl) lv_label_set_text(s_nb_status_lbl, "...");
    lv_timer_handler();  // flush the UI update before blocking

    const bool ok = nb_client::register_device();

    if (s_nb_status_lbl) {
        lv_label_set_text(s_nb_status_lbl,
            i18n::str(ok ? StrId::NB_STATUS_OK : StrId::NB_STATUS_FAILED));
    }
    if (s_nb_reg_btn) {
        if (ok) {
            lv_obj_add_flag(s_nb_reg_btn, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_state(s_nb_reg_btn, LV_STATE_DISABLED);
        }
    }
}

// OTA progress callback — updates the progress screen's fill bar and label,
// then yields to LVGL so the display stays responsive during flashing.
// s_ota_bar is the inner fill object; its width is set proportionally to
// (SCREEN_WIDTH - 40) pixels to represent 0–100 %.
static constexpr int OTA_BAR_W = SCREEN_WIDTH - 40;

static void ota_progress_cb(int percent)
{
    if (s_ota_bar) {
        const int fill_w = OTA_BAR_W * percent / 100;
        lv_obj_set_width(s_ota_bar, fill_w > 0 ? fill_w : 1);
    }
    if (s_ota_status) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", percent);
        lv_label_set_text(s_ota_status, buf);
    }
    lv_timer_handler();
}

static void on_nb_update_click(lv_event_t* /*e*/)
{
    // Destroy the settings screen — OTA takes over the display.
    // Null all pointers before deleting to prevent stale refs.
    lv_obj_t* stale = s_screen;
    s_screen = s_prev_screen = nullptr;
    s_de_btn = s_en_btn = nullptr;
    s_brt_slider = s_brt_val_lbl = nullptr;
    s_nb_status_lbl = s_nb_reg_btn = s_nb_update_btn = nullptr;
    s_bat_off_btn = s_bat_on_btn = nullptr;
    s_tmout_10_btn = s_tmout_30_btn = s_tmout_60_btn = nullptr;

    // ---- Build OTA progress screen ------------------------------------------
    s_ota_screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(s_ota_screen, lv_color_hex(UI_COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_ota_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_ota_screen, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_ota_screen, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_ota_screen, 0, LV_PART_MAIN);
    lv_obj_clear_flag(s_ota_screen, LV_OBJ_FLAG_SCROLLABLE);

    // Title label
    lv_obj_t* title = lv_label_create(s_ota_screen);
    lv_label_set_text(title, i18n::str(StrId::NB_UPDATE_BTN));
    lv_obj_set_style_text_color(title, lv_color_hex(UI_COL_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -50);

    // Status / percentage label
    s_ota_status = lv_label_create(s_ota_screen);
    lv_label_set_text(s_ota_status, i18n::str(StrId::NB_UPDATING));
    lv_obj_set_style_text_color(s_ota_status, lv_color_hex(UI_COL_TEXT_DIM), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_ota_status, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s_ota_status, LV_ALIGN_CENTER, 0, -16);

    // Progress bar — two plain lv_obj containers (avoids lv_bar BSS overhead).
    // Outer: dark track.  Inner (s_ota_bar): accent fill, width updated by callback.
    lv_obj_t* bar_track = lv_obj_create(s_ota_screen);
    lv_obj_set_size(bar_track, OTA_BAR_W, 16);
    lv_obj_align(bar_track, LV_ALIGN_CENTER, 0, 16);
    lv_obj_set_style_bg_color(bar_track, lv_color_hex(UI_COL_SURFACE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar_track, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar_track, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bar_track, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(bar_track, 4, LV_PART_MAIN);
    lv_obj_clear_flag(bar_track, LV_OBJ_FLAG_SCROLLABLE);

    s_ota_bar = lv_obj_create(bar_track);
    lv_obj_set_size(s_ota_bar, 1, 16);  // starts at 1 px wide (0 is invalid)
    lv_obj_set_pos(s_ota_bar, 0, 0);
    lv_obj_set_style_bg_color(s_ota_bar, lv_color_hex(UI_COL_ACCENT), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_ota_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_ota_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_ota_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_ota_bar, 4, LV_PART_MAIN);
    lv_obj_clear_flag(s_ota_bar, LV_OBJ_FLAG_SCROLLABLE);

    // Switch to the OTA screen; delete the settings screen behind it.
    lv_scr_load(s_ota_screen);
    if (stale) lv_obj_del(stale);
    lv_timer_handler();  // render the progress screen before blocking

    // ---- Perform the OTA update (blocking) ----------------------------------
    const nb_client::OtaResult result = nb_client::start_ota_update(ota_progress_cb);

    // ---- Handle result -------------------------------------------------------
    const char* result_msg;
    switch (result) {
        case nb_client::OtaResult::SUCCESS:
            result_msg = i18n::str(StrId::NB_UPDATE_OK);       break;
        case nb_client::OtaResult::ERR_NO_RELEASE:
            result_msg = i18n::str(StrId::NB_UPDATE_NO_RELEASE); break;
        case nb_client::OtaResult::ERR_CHECKSUM:
            result_msg = i18n::str(StrId::NB_UPDATE_FAIL_HASH); break;
        case nb_client::OtaResult::ERR_FLASH:
            result_msg = i18n::str(StrId::NB_UPDATE_FAIL_FLASH); break;
        default:
            result_msg = i18n::str(StrId::NB_UPDATE_FAIL_NET);  break;
    }

    if (s_ota_status) lv_label_set_text(s_ota_status, result_msg);
    if (s_ota_bar)    lv_obj_set_width(s_ota_bar,
                          result == nb_client::OtaResult::SUCCESS ? OTA_BAR_W : 1);
    lv_timer_handler();

    if (result == nb_client::OtaResult::SUCCESS) {
        // Record that an OTA just ran — enables boot-loop detection on next boot.
        nvs_config::set_update_pending();
        // Brief pause so the success message is readable, then reboot.
#ifdef ARDUINO
        delay(1500);
        ESP.restart();
#endif
        return;
    }

    // On failure: show a back button so the user can return to the main shell.
    lv_obj_t* back_btn = lv_btn_create(s_ota_screen);
    lv_obj_set_size(back_btn, SCREEN_WIDTH - 40, 44);
    lv_obj_align(back_btn, LV_ALIGN_CENTER, 0, 60);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(UI_COL_SURFACE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(back_btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(UI_COL_NAV_ACTIVE),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(back_btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back_btn, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(back_btn, lv_color_hex(UI_COL_BORDER), LV_PART_MAIN);
    lv_obj_set_style_radius(back_btn, 4, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(back_btn, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(back_btn, [](lv_event_t*) {
        // Delete OTA screen and rebuild the main shell.
        lv_obj_t* ota = s_ota_screen;
        s_ota_screen = s_ota_bar = s_ota_status = nullptr;
        if (ota) lv_obj_del(ota);
        shell::create();
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_lbl, lv_color_hex(UI_COL_ACCENT), LV_PART_MAIN);
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(back_lbl, LV_ALIGN_CENTER, 0, 0);

    lv_timer_handler();
}

static void on_recalibrate_click(lv_event_t* /*e*/)
{
    // Async-delete the settings screen before handing control to the
    // calibration routine.  run_calibration() calls lv_timer_handler()
    // internally, which will process the deferred delete safely.
    lv_obj_t* stale = s_screen;
    lv_obj_t* prev  = s_prev_screen;
    s_screen = s_prev_screen = nullptr;
    s_de_btn = s_en_btn = nullptr;
    s_brt_slider = s_brt_val_lbl = nullptr;
    s_nb_status_lbl = s_nb_reg_btn = s_nb_update_btn = nullptr;
    s_bat_off_btn = s_bat_on_btn = nullptr;
    s_tmout_10_btn = s_tmout_30_btn = s_tmout_60_btn = nullptr;

    lv_obj_del_async(stale);  // deferred — safe from inside event callback
    if (prev) lv_obj_del_async(prev);  // stale previous shell screen

    // Blocking calibration UI.  Ends by loading a blank screen.
    touch_driver::run_calibration();

    // Rebuild the full shell UI.  shell::create() deletes any old s_screen
    // it still holds at the start of the function, then loads the new screen
    // (auto_del=true deletes the blank screen from calibration).
    shell::create();
}

// ----------------------------------------------------------------------------
// Battery mode callbacks (TICKET-025)
// ----------------------------------------------------------------------------

static void on_bat_off_click(lv_event_t* /*e*/)
{
    sleep_manager::set_battery_mode(false);
    if (s_bat_off_btn) style_lang_btn(s_bat_off_btn, true);
    if (s_bat_on_btn)  style_lang_btn(s_bat_on_btn, false);
}

static void on_bat_on_click(lv_event_t* /*e*/)
{
    sleep_manager::set_battery_mode(true);
    if (s_bat_off_btn) style_lang_btn(s_bat_off_btn, false);
    if (s_bat_on_btn)  style_lang_btn(s_bat_on_btn, true);
}

static void refresh_tmout_style()
{
    const uint16_t cur = sleep_manager::get_timeout_s();
    if (s_tmout_10_btn) style_lang_btn(s_tmout_10_btn, cur == 10);
    if (s_tmout_30_btn) style_lang_btn(s_tmout_30_btn, cur == 30);
    if (s_tmout_60_btn) style_lang_btn(s_tmout_60_btn, cur == 60);
}

static void on_tmout_10_click(lv_event_t* /*e*/)
{
    sleep_manager::set_timeout_s(10);
    refresh_tmout_style();
}

static void on_tmout_30_click(lv_event_t* /*e*/)
{
    sleep_manager::set_timeout_s(30);
    refresh_tmout_style();
}

static void on_tmout_60_click(lv_event_t* /*e*/)
{
    sleep_manager::set_timeout_s(60);
    refresh_tmout_style();
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

namespace settings_screen {

void open()
{
    if (s_screen) return; // already open

    s_prev_screen = lv_scr_act();
    s_de_btn = s_en_btn = nullptr;
    s_brt_slider = s_brt_val_lbl = nullptr;

    // ---- Screen ------------------------------------------------------------
    s_screen = lv_obj_create(nullptr);
    lv_obj_set_size(s_screen, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(UI_COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_screen, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_screen, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_screen, 0, LV_PART_MAIN);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    // ---- Header ------------------------------------------------------------
    lv_obj_t* header = lv_obj_create(s_screen);
    lv_obj_set_size(header, SCREEN_WIDTH, SETTINGS_HEADER_H);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(UI_COL_HEADER), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(header, lv_color_hex(UI_COL_BORDER), LV_PART_MAIN);
    lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(header, 0, LV_PART_MAIN);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    // Back button (44 × 44 touch target)
    lv_obj_t* back_btn = lv_btn_create(header);
    lv_obj_remove_style_all(back_btn);
    lv_obj_set_size(back_btn, SETTINGS_HEADER_H, SETTINGS_HEADER_H);
    lv_obj_set_pos(back_btn, 0, 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(UI_COL_HEADER), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(back_btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(UI_COL_NAV_ACTIVE),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(back_btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_event_cb(back_btn, on_back_click, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_lbl, lv_color_hex(UI_COL_ACCENT), LV_PART_MAIN);
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(back_lbl, LV_ALIGN_CENTER, 0, 0);

    // Settings title
    lv_obj_t* title_lbl = lv_label_create(header);
    lv_label_set_text(title_lbl, i18n::str(StrId::SETTINGS_TITLE));
    lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(title_lbl, SCREEN_WIDTH - SETTINGS_HEADER_H - 8);
    lv_obj_set_style_text_color(title_lbl, lv_color_hex(UI_COL_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(title_lbl, LV_ALIGN_LEFT_MID, SETTINGS_HEADER_H + 4, 0);

    // ---- Content area (below header, vertically scrollable) ----------------
    const int CONTENT_H = SCREEN_HEIGHT - SETTINGS_HEADER_H;
    lv_obj_t* content = lv_obj_create(s_screen);
    lv_obj_set_size(content, SCREEN_WIDTH, CONTENT_H);
    lv_obj_set_pos(content, 0, SETTINGS_HEADER_H);
    lv_obj_set_style_bg_color(content, lv_color_hex(UI_COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(content, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(content, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(content, 0, LV_PART_MAIN);
    lv_obj_add_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_OFF);

    // ---- Language toggle ---------------------------------------------------
    // Label
    lv_obj_t* lang_lbl = lv_label_create(content);
    lv_label_set_text(lang_lbl, i18n::str(StrId::SETTINGS_LANGUAGE));
    lv_obj_set_style_text_color(lang_lbl, lv_color_hex(UI_COL_TEXT_DIM), LV_PART_MAIN);
    lv_obj_set_style_text_font(lang_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(lang_lbl, 10, 10);

    // Two equal-width buttons: [DE] [gap] [EN]
    // Width per button: (SCREEN_WIDTH - 8 - 8 - 16) / 2 = 144 px
    static constexpr int LANG_BTN_W = 144;
    static constexpr int LANG_BTN_H = 44;
    static constexpr int LANG_BTN_Y = 30;

    const bool is_de = (i18n::get_locale() == Locale::DE);

    s_de_btn = lv_btn_create(content);
    lv_obj_set_size(s_de_btn, LANG_BTN_W, LANG_BTN_H);
    lv_obj_set_pos(s_de_btn, 8, LANG_BTN_Y);
    lv_obj_add_event_cb(s_de_btn, on_de_click, LV_EVENT_CLICKED, nullptr);
    style_lang_btn(s_de_btn, is_de);

    lv_obj_t* de_lbl = lv_label_create(s_de_btn);
    lv_label_set_text(de_lbl, "DE");
    lv_obj_set_style_text_color(de_lbl, lv_color_hex(UI_COL_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_font(de_lbl, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(de_lbl, LV_ALIGN_CENTER, 0, 0);

    s_en_btn = lv_btn_create(content);
    lv_obj_set_size(s_en_btn, LANG_BTN_W, LANG_BTN_H);
    lv_obj_set_pos(s_en_btn, 8 + LANG_BTN_W + 16, LANG_BTN_Y);
    lv_obj_add_event_cb(s_en_btn, on_en_click, LV_EVENT_CLICKED, nullptr);
    style_lang_btn(s_en_btn, !is_de);

    lv_obj_t* en_lbl = lv_label_create(s_en_btn);
    lv_label_set_text(en_lbl, "EN");
    lv_obj_set_style_text_color(en_lbl, lv_color_hex(UI_COL_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_font(en_lbl, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(en_lbl, LV_ALIGN_CENTER, 0, 0);

    // ---- Brightness slider -------------------------------------------------
    // Read current PWM duty as stored setting (or full brightness if none stored)
    UiSettings ui_s;
    if (!nvs_config::load_ui_settings(ui_s)) ui_s.brightness = 255;
    const int cur_brt = static_cast<int>(ui_s.brightness);

    // Label row: "Brightness:"  on left, "XX%" on right
    lv_obj_t* brt_lbl = lv_label_create(content);
    lv_label_set_text(brt_lbl, i18n::str(StrId::SETTINGS_BRIGHTNESS));
    lv_obj_set_style_text_color(brt_lbl, lv_color_hex(UI_COL_TEXT_DIM), LV_PART_MAIN);
    lv_obj_set_style_text_font(brt_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(brt_lbl, 10, 90);

    char brt_buf[8];
    snprintf(brt_buf, sizeof(brt_buf), "%d%%", cur_brt * 100 / 255);
    s_brt_val_lbl = lv_label_create(content);
    lv_label_set_text(s_brt_val_lbl, brt_buf);
    lv_obj_set_style_text_color(s_brt_val_lbl, lv_color_hex(UI_COL_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_brt_val_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_width(s_brt_val_lbl, 50);
    lv_obj_align(s_brt_val_lbl, LV_ALIGN_TOP_RIGHT, -10, 90);

    // Slider
    static constexpr int SLIDER_W = SCREEN_WIDTH - 20;
    static constexpr int SLIDER_H = 20;
    s_brt_slider = lv_slider_create(content);
    lv_obj_set_size(s_brt_slider, SLIDER_W, SLIDER_H);
    lv_obj_set_pos(s_brt_slider, 10, 110);
    lv_slider_set_range(s_brt_slider, 10, 255);
    lv_slider_set_value(s_brt_slider, cur_brt, LV_ANIM_OFF);

    // Track
    lv_obj_set_style_bg_color(s_brt_slider, lv_color_hex(UI_COL_SURFACE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_brt_slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_brt_slider, 4, LV_PART_MAIN);
    // Fill indicator
    lv_obj_set_style_bg_color(s_brt_slider, lv_color_hex(UI_COL_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_brt_slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_brt_slider, 4, LV_PART_INDICATOR);
    // Knob
    lv_obj_set_style_bg_color(s_brt_slider, lv_color_hex(UI_COL_TEXT), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(s_brt_slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_radius(s_brt_slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(s_brt_slider, 6, LV_PART_KNOB);  // larger touch target

    lv_obj_add_event_cb(s_brt_slider, on_brightness_changed, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(s_brt_slider, on_brightness_released, LV_EVENT_RELEASED, nullptr);

    // ---- Touch re-calibration button ---------------------------------------
    static constexpr int RECAL_BTN_W = SCREEN_WIDTH - 20;
    static constexpr int RECAL_BTN_H = 44;
    lv_obj_t* recal_btn = lv_btn_create(content);
    lv_obj_set_size(recal_btn, RECAL_BTN_W, RECAL_BTN_H);
    lv_obj_set_pos(recal_btn, 10, 148);
    lv_obj_set_style_bg_color(recal_btn, lv_color_hex(UI_COL_SURFACE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(recal_btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(recal_btn, lv_color_hex(UI_COL_NAV_ACTIVE),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(recal_btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(recal_btn, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(recal_btn, lv_color_hex(UI_COL_BORDER), LV_PART_MAIN);
    lv_obj_set_style_radius(recal_btn, 4, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(recal_btn, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(recal_btn, on_recalibrate_click, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* recal_lbl = lv_label_create(recal_btn);
    lv_label_set_text(recal_lbl, i18n::str(StrId::SETTINGS_RECALIBRATE));
    lv_obj_set_style_text_color(recal_lbl, lv_color_hex(UI_COL_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_font(recal_lbl, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(recal_lbl, LV_ALIGN_CENTER, 0, 0);

    // ---- Battery / mobile mode section (TICKET-025) -------------------------
    {
        const bool bat_on = sleep_manager::is_battery_mode();
        const uint16_t cur_timeout = sleep_manager::get_timeout_s();

        // Thin separator
        lv_obj_t* bat_sep = lv_obj_create(content);
        lv_obj_set_size(bat_sep, SCREEN_WIDTH - 20, 1);
        lv_obj_set_pos(bat_sep, 10, 202);
        lv_obj_set_style_bg_color(bat_sep, lv_color_hex(UI_COL_BORDER), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bat_sep, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(bat_sep, 0, LV_PART_MAIN);

        lv_obj_t* bat_lbl = lv_label_create(content);
        lv_label_set_text(bat_lbl, i18n::str(StrId::SETTINGS_BATTERY_MODE));
        lv_obj_set_style_text_color(bat_lbl, lv_color_hex(UI_COL_TEXT_DIM), LV_PART_MAIN);
        lv_obj_set_style_text_font(bat_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_pos(bat_lbl, 10, 212);

        // [Off] [On] toggle — same width/style as language buttons
        static constexpr int BAT_BTN_W = 144;
        static constexpr int BAT_BTN_H = 44;
        static constexpr int BAT_BTN_Y = 232;

        s_bat_off_btn = lv_btn_create(content);
        lv_obj_set_size(s_bat_off_btn, BAT_BTN_W, BAT_BTN_H);
        lv_obj_set_pos(s_bat_off_btn, 8, BAT_BTN_Y);
        lv_obj_add_event_cb(s_bat_off_btn, on_bat_off_click, LV_EVENT_CLICKED, nullptr);
        style_lang_btn(s_bat_off_btn, !bat_on);
        { lv_obj_t* l = lv_label_create(s_bat_off_btn);
          lv_label_set_text(l, i18n::str(StrId::SETTINGS_BATTERY_OFF));
          lv_obj_set_style_text_color(l, lv_color_hex(UI_COL_TEXT), LV_PART_MAIN);
          lv_obj_set_style_text_font(l, &lv_font_montserrat_16, LV_PART_MAIN);
          lv_obj_align(l, LV_ALIGN_CENTER, 0, 0); }

        s_bat_on_btn = lv_btn_create(content);
        lv_obj_set_size(s_bat_on_btn, BAT_BTN_W, BAT_BTN_H);
        lv_obj_set_pos(s_bat_on_btn, 8 + BAT_BTN_W + 16, BAT_BTN_Y);
        lv_obj_add_event_cb(s_bat_on_btn, on_bat_on_click, LV_EVENT_CLICKED, nullptr);
        style_lang_btn(s_bat_on_btn, bat_on);
        { lv_obj_t* l = lv_label_create(s_bat_on_btn);
          lv_label_set_text(l, i18n::str(StrId::SETTINGS_BATTERY_ON));
          lv_obj_set_style_text_color(l, lv_color_hex(UI_COL_TEXT), LV_PART_MAIN);
          lv_obj_set_style_text_font(l, &lv_font_montserrat_16, LV_PART_MAIN);
          lv_obj_align(l, LV_ALIGN_CENTER, 0, 0); }

        // Sleep timeout selector: [10s] [30s] [60s]
        lv_obj_t* tmout_lbl = lv_label_create(content);
        lv_label_set_text(tmout_lbl, i18n::str(StrId::SETTINGS_SLEEP_TIMEOUT));
        lv_obj_set_style_text_color(tmout_lbl, lv_color_hex(UI_COL_TEXT_DIM), LV_PART_MAIN);
        lv_obj_set_style_text_font(tmout_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_pos(tmout_lbl, 10, 286);

        // 3 equal-width buttons: (300px available − 2×4px gaps) / 3 = 97px each
        static constexpr int TMOUT_BTN_W = (SCREEN_WIDTH - 20 - 8) / 3;
        static constexpr int TMOUT_BTN_H = 44;
        static constexpr int TMOUT_BTN_Y = 304;

        s_tmout_10_btn = lv_btn_create(content);
        lv_obj_set_size(s_tmout_10_btn, TMOUT_BTN_W, TMOUT_BTN_H);
        lv_obj_set_pos(s_tmout_10_btn, 10, TMOUT_BTN_Y);
        lv_obj_add_event_cb(s_tmout_10_btn, on_tmout_10_click, LV_EVENT_CLICKED, nullptr);
        style_lang_btn(s_tmout_10_btn, cur_timeout == 10);
        { lv_obj_t* l = lv_label_create(s_tmout_10_btn); lv_label_set_text(l, "10s");
          lv_obj_set_style_text_color(l, lv_color_hex(UI_COL_TEXT), LV_PART_MAIN);
          lv_obj_set_style_text_font(l, &lv_font_montserrat_16, LV_PART_MAIN);
          lv_obj_align(l, LV_ALIGN_CENTER, 0, 0); }

        s_tmout_30_btn = lv_btn_create(content);
        lv_obj_set_size(s_tmout_30_btn, TMOUT_BTN_W, TMOUT_BTN_H);
        lv_obj_set_pos(s_tmout_30_btn, 10 + TMOUT_BTN_W + 4, TMOUT_BTN_Y);
        lv_obj_add_event_cb(s_tmout_30_btn, on_tmout_30_click, LV_EVENT_CLICKED, nullptr);
        style_lang_btn(s_tmout_30_btn, cur_timeout == 30);
        { lv_obj_t* l = lv_label_create(s_tmout_30_btn); lv_label_set_text(l, "30s");
          lv_obj_set_style_text_color(l, lv_color_hex(UI_COL_TEXT), LV_PART_MAIN);
          lv_obj_set_style_text_font(l, &lv_font_montserrat_16, LV_PART_MAIN);
          lv_obj_align(l, LV_ALIGN_CENTER, 0, 0); }

        s_tmout_60_btn = lv_btn_create(content);
        lv_obj_set_size(s_tmout_60_btn, TMOUT_BTN_W, TMOUT_BTN_H);
        lv_obj_set_pos(s_tmout_60_btn, 10 + (TMOUT_BTN_W + 4) * 2, TMOUT_BTN_Y);
        lv_obj_add_event_cb(s_tmout_60_btn, on_tmout_60_click, LV_EVENT_CLICKED, nullptr);
        style_lang_btn(s_tmout_60_btn, cur_timeout == 60);
        { lv_obj_t* l = lv_label_create(s_tmout_60_btn); lv_label_set_text(l, "60s");
          lv_obj_set_style_text_color(l, lv_color_hex(UI_COL_TEXT), LV_PART_MAIN);
          lv_obj_set_style_text_font(l, &lv_font_montserrat_16, LV_PART_MAIN);
          lv_obj_align(l, LV_ALIGN_CENTER, 0, 0); }
    }

    // ---- nano_backbone OTA section -----------------------------------------
    // Only rendered when nb_url is configured; always shown when configured
    // so the user can see registration status and re-trigger if needed.
    {
        const nb_client::Status nb_status = nb_client::get_status();
        const bool bat_on = sleep_manager::is_battery_mode();

        // Thin separator line
        lv_obj_t* sep = lv_obj_create(content);
        lv_obj_set_size(sep, SCREEN_WIDTH - 20, 1);
        lv_obj_set_pos(sep, 10, 356);
        lv_obj_set_style_bg_color(sep, lv_color_hex(UI_COL_BORDER), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(sep, 0, LV_PART_MAIN);

        // Status label
        const char* status_str;
        switch (nb_status) {
            case nb_client::Status::REGISTERED:     status_str = i18n::str(StrId::NB_STATUS_OK);      break;
            case nb_client::Status::UNREGISTERED:   status_str = i18n::str(StrId::NB_STATUS_UNREG);   break;
            case nb_client::Status::REG_FAILED:     status_str = i18n::str(StrId::NB_STATUS_FAILED);  break;
            default:                                status_str = i18n::str(StrId::NB_STATUS_NOT_CFG); break;
        }

        s_nb_status_lbl = lv_label_create(content);
        lv_label_set_text(s_nb_status_lbl, status_str);
        lv_obj_set_style_text_color(s_nb_status_lbl,
            lv_color_hex(nb_status == nb_client::Status::REGISTERED
                         ? UI_COL_OK : UI_COL_TEXT_DIM),
            LV_PART_MAIN);
        lv_obj_set_style_text_font(s_nb_status_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_pos(s_nb_status_lbl, 10, 366);

        // Helper: create a standard OTA action button at the given y position
        auto make_ota_btn = [&](int y) -> lv_obj_t* {
            lv_obj_t* btn = lv_btn_create(content);
            lv_obj_set_size(btn, SCREEN_WIDTH - 20, 44);
            lv_obj_set_pos(btn, 10, y);
            lv_obj_set_style_bg_color(btn, lv_color_hex(UI_COL_SURFACE), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_bg_color(btn, lv_color_hex(UI_COL_NAV_ACTIVE),
                                      LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
            lv_obj_set_style_border_color(btn, lv_color_hex(UI_COL_BORDER), LV_PART_MAIN);
            lv_obj_set_style_radius(btn, 4, LV_PART_MAIN);
            lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
            return btn;
        };

        int next_btn_y = 388;

        // "Register OTA" button — shown when configured but not registered (or failed)
        if (nb_status == nb_client::Status::UNREGISTERED ||
            nb_status == nb_client::Status::REG_FAILED) {
            s_nb_reg_btn = make_ota_btn(next_btn_y);
            lv_obj_add_event_cb(s_nb_reg_btn, on_nb_register_click, LV_EVENT_CLICKED, nullptr);

            lv_obj_t* nb_lbl = lv_label_create(s_nb_reg_btn);
            lv_label_set_text(nb_lbl, i18n::str(StrId::NB_REGISTER_BTN));
            lv_obj_set_style_text_color(nb_lbl, lv_color_hex(UI_COL_TEXT), LV_PART_MAIN);
            lv_obj_set_style_text_font(nb_lbl, &lv_font_montserrat_16, LV_PART_MAIN);
            lv_obj_align(nb_lbl, LV_ALIGN_CENTER, 0, 0);
            next_btn_y += 52;
        }

        // "Install Update" button — shown when registered and an update is available.
        // When battery mode is on: replaced with a hint label (OTA gate).
        if (nb_status == nb_client::Status::REGISTERED && nb_client::is_update_available()) {
            if (bat_on) {
                lv_obj_t* hint = lv_label_create(content);
                lv_label_set_text(hint, i18n::str(StrId::NB_UPDATE_BATT_HINT));
                lv_obj_set_style_text_color(hint, lv_color_hex(UI_COL_TEXT_DIM), LV_PART_MAIN);
                lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, LV_PART_MAIN);
                lv_obj_set_width(hint, SCREEN_WIDTH - 20);
                lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
                lv_obj_set_pos(hint, 10, next_btn_y);
            } else {
                s_nb_update_btn = make_ota_btn(next_btn_y);
                lv_obj_add_event_cb(s_nb_update_btn, on_nb_update_click, LV_EVENT_CLICKED, nullptr);

                lv_obj_t* upd_lbl = lv_label_create(s_nb_update_btn);
                lv_label_set_text(upd_lbl, i18n::str(StrId::NB_UPDATE_BTN));
                lv_obj_set_style_text_color(upd_lbl, lv_color_hex(UI_COL_ACCENT), LV_PART_MAIN);
                lv_obj_set_style_text_font(upd_lbl, &lv_font_montserrat_16, LV_PART_MAIN);
                lv_obj_align(upd_lbl, LV_ALIGN_CENTER, 0, 0);
            }
        }
    }

    // Fade in; don't auto-delete the shell screen behind us — we need it
    // for the back button's lv_scr_load_anim target.
    lv_scr_load_anim(s_screen, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, false);
}

bool is_open()
{
    return s_screen != nullptr;
}

} // namespace settings_screen
