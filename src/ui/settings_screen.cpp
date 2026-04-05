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
static lv_obj_t* s_nb_status_lbl = nullptr;
static lv_obj_t* s_nb_reg_btn    = nullptr;

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
    s_nb_status_lbl = s_nb_reg_btn = nullptr;
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
    s_nb_status_lbl = s_nb_reg_btn = nullptr;
    shell::create();  // rebuilds all UI strings; deletes old shell + fades out this screen
}

static void on_en_click(lv_event_t* /*e*/)
{
    if (i18n::get_locale() == Locale::EN) return;
    i18n::set_locale(Locale::EN);
    s_screen = s_prev_screen = nullptr;
    s_de_btn = s_en_btn = nullptr;
    s_brt_slider = s_brt_val_lbl = nullptr;
    s_nb_status_lbl = s_nb_reg_btn = nullptr;
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
    s_nb_status_lbl = s_nb_reg_btn = nullptr;

    lv_obj_del_async(stale);  // deferred — safe from inside event callback
    if (prev) lv_obj_del_async(prev);  // stale previous shell screen

    // Blocking calibration UI.  Ends by loading a blank screen.
    touch_driver::run_calibration();

    // Rebuild the full shell UI.  shell::create() deletes any old s_screen
    // it still holds at the start of the function, then loads the new screen
    // (auto_del=true deletes the blank screen from calibration).
    shell::create();
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

    // ---- nano_backbone OTA section -----------------------------------------
    // Only rendered when nb_url is configured; always shown when configured
    // so the user can see registration status and re-trigger if needed.
    {
        const nb_client::Status nb_status = nb_client::get_status();

        // Thin separator line
        lv_obj_t* sep = lv_obj_create(content);
        lv_obj_set_size(sep, SCREEN_WIDTH - 20, 1);
        lv_obj_set_pos(sep, 10, 200);
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
        lv_obj_set_pos(s_nb_status_lbl, 10, 210);

        // "Register OTA" button — shown when configured but not registered (or failed)
        if (nb_status == nb_client::Status::UNREGISTERED ||
            nb_status == nb_client::Status::REG_FAILED) {
            static constexpr int NB_BTN_W = SCREEN_WIDTH - 20;
            static constexpr int NB_BTN_H = 44;
            s_nb_reg_btn = lv_btn_create(content);
            lv_obj_set_size(s_nb_reg_btn, NB_BTN_W, NB_BTN_H);
            lv_obj_set_pos(s_nb_reg_btn, 10, 232);
            lv_obj_set_style_bg_color(s_nb_reg_btn, lv_color_hex(UI_COL_SURFACE), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(s_nb_reg_btn, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_bg_color(s_nb_reg_btn, lv_color_hex(UI_COL_NAV_ACTIVE),
                                      LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_bg_opa(s_nb_reg_btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_border_width(s_nb_reg_btn, 1, LV_PART_MAIN);
            lv_obj_set_style_border_color(s_nb_reg_btn, lv_color_hex(UI_COL_BORDER), LV_PART_MAIN);
            lv_obj_set_style_radius(s_nb_reg_btn, 4, LV_PART_MAIN);
            lv_obj_set_style_shadow_width(s_nb_reg_btn, 0, LV_PART_MAIN);
            lv_obj_add_event_cb(s_nb_reg_btn, on_nb_register_click, LV_EVENT_CLICKED, nullptr);

            lv_obj_t* nb_lbl = lv_label_create(s_nb_reg_btn);
            lv_label_set_text(nb_lbl, i18n::str(StrId::NB_REGISTER_BTN));
            lv_obj_set_style_text_color(nb_lbl, lv_color_hex(UI_COL_TEXT), LV_PART_MAIN);
            lv_obj_set_style_text_font(nb_lbl, &lv_font_montserrat_16, LV_PART_MAIN);
            lv_obj_align(nb_lbl, LV_ALIGN_CENTER, 0, 0);
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
