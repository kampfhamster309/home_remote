#include "shell.h"

#include <Arduino.h>
#include <lvgl.h>
#include <cstring>

#include "ui_theme.h"
#include "ui_fonts.h"
#include "ui_icons.h"
#include "display_config.h"
#include "ha/ha_client.h"
#include "ha/area_cache.h"
#include "ha/entity_cache.h"
#include "ha/weather_cache.h"
#include "ha_area.h"
#include "tile_widget.h"
#include "detail_screen.h"
#include "weather_screen.h"
#include "settings_screen.h"
#include "i18n/i18n.h"

// ----------------------------------------------------------------------------
// Module-private state
// ----------------------------------------------------------------------------

namespace {

// Maximum groups the nav bar can hold (one per named area)
static constexpr size_t UI_MAX_GROUPS = MAX_AREAS;


// Shell LVGL objects
static lv_obj_t* s_screen     = nullptr;
static lv_obj_t* s_room_label = nullptr;
static lv_obj_t* s_wifi_dot   = nullptr;
static lv_obj_t* s_ha_dot     = nullptr;
static lv_obj_t* s_content    = nullptr;
static lv_obj_t* s_nav_bar    = nullptr;

static lv_obj_t* s_tab_btns[UI_MAX_GROUPS];
static size_t    s_group_count      = 0;
static size_t    s_active_idx       = 0;
// Weather tab — only present when weather_cache::has_weather() is true.
// s_active_idx == s_group_count means the weather tab is active.
static lv_obj_t* s_weather_tab_btn  = nullptr;
// Settings gear button in the header.
static lv_obj_t* s_settings_btn     = nullptr;
// Update available badge — small accent dot on the gear button top-right corner.
static lv_obj_t* s_update_badge     = nullptr;
// Battery mode icon — shown in header when battery mode is active.
static lv_obj_t* s_battery_icon     = nullptr;

// Error banner — semi-transparent overlay over the content area.
// Created on demand by update_status(); deleted when connectivity is restored.
static lv_obj_t*   s_error_banner       = nullptr;
// Pointer to the currently displayed banner message (from i18n string table).
// Pointer comparison is valid because i18n::str() always returns the same
// static char* for a given StrId + locale.
static const char* s_current_banner_msg = nullptr;

// ----------------------------------------------------------------------------
// Null all object pointers — called at the start of create() so a second
// invocation (locale change, recalibration) does not hold stale references.
// ----------------------------------------------------------------------------
static void reset_static_ptrs()
{
    s_screen              = nullptr;
    s_room_label          = nullptr;
    s_wifi_dot            = nullptr;
    s_ha_dot              = nullptr;
    s_content             = nullptr;
    s_nav_bar             = nullptr;
    s_weather_tab_btn     = nullptr;
    s_settings_btn        = nullptr;
    s_update_badge        = nullptr;  // deleted with s_screen by lv_obj_del
    s_battery_icon        = nullptr;  // deleted with s_screen by lv_obj_del
    s_error_banner        = nullptr;  // deleted with s_screen by lv_obj_del
    s_current_banner_msg  = nullptr;  // force re-evaluation after next create()
    for (size_t i = 0; i < UI_MAX_GROUPS; ++i) s_tab_btns[i] = nullptr;
    s_group_count = 0;
    s_active_idx  = 0;
}

// ----------------------------------------------------------------------------
// Forward declaration (defined later in this file)
// ----------------------------------------------------------------------------

static void build_tile_grid(size_t group_idx);

// ----------------------------------------------------------------------------
// Nav tab helpers
// ----------------------------------------------------------------------------

static void apply_tab_style(lv_obj_t* btn, bool active)
{
    lv_obj_set_style_bg_color(btn,
        lv_color_hex(active ? UI_COL_NAV_ACTIVE : UI_COL_NAV), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    // Accent top border on the active tab
    lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_TOP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, active ? 2 : 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_hex(UI_COL_ACCENT), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 0, LV_PART_MAIN);

    // Pressed-state style to match the active background
    lv_obj_set_style_bg_color(btn,
        lv_color_hex(UI_COL_NAV_ACTIVE), LV_PART_MAIN | LV_STATE_PRESSED);
}

static void set_active_group(size_t idx)
{
    if (idx >= s_group_count) return;

    // Deactivate weather tab if switching away from it
    if (s_weather_tab_btn) apply_tab_style(s_weather_tab_btn, false);

    // Update tab button highlight
    for (size_t i = 0; i < s_group_count; ++i) {
        if (s_tab_btns[i]) apply_tab_style(s_tab_btns[i], i == idx);
    }

    // Update header room name
    const area_cache::EntityGroup* g = area_cache::get_group(idx);
    if (g && s_room_label) {
        lv_label_set_text(s_room_label,
            (g->name[0] != '\0') ? g->name : "Home");
    }

    // Scroll active tab into view (no-op if already visible)
    if (s_nav_bar && s_tab_btns[idx]) {
        lv_obj_scroll_to_view(s_tab_btns[idx], LV_ANIM_ON);
    }

    s_active_idx = idx;

    // Rebuild the tile grid for the newly selected group
    build_tile_grid(idx);
}

static void set_weather_tab_active()
{
    // Deactivate all room tabs
    for (size_t i = 0; i < s_group_count; ++i) {
        if (s_tab_btns[i]) apply_tab_style(s_tab_btns[i], false);
    }
    if (s_weather_tab_btn) apply_tab_style(s_weather_tab_btn, true);

    if (s_room_label) lv_label_set_text(s_room_label, i18n::str(StrId::WEATHER_TAB));

    if (s_nav_bar && s_weather_tab_btn) {
        lv_obj_scroll_to_view(s_weather_tab_btn, LV_ANIM_ON);
    }

    s_active_idx = s_group_count; // sentinel: weather tab

    weather_screen::create(s_content);
}

static void on_settings_click(lv_event_t* /*e*/)
{
    settings_screen::open();
}

static void on_tab_click(lv_event_t* e)
{
    lv_obj_t* btn = lv_event_get_target(e);
    // Index stored as integer in user_data pointer slot
    size_t idx = static_cast<size_t>(
        reinterpret_cast<uintptr_t>(lv_obj_get_user_data(btn)));
    set_active_group(idx);
}

static void on_weather_tab_click(lv_event_t* /*e*/)
{
    set_weather_tab_active();
}

// ----------------------------------------------------------------------------
// Shared style: invisible container (no padding, no border, no scroll)
// ----------------------------------------------------------------------------

static void style_container(lv_obj_t* obj, lv_color_t bg)
{
    lv_obj_set_style_bg_color(obj, bg, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, 0, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

// ----------------------------------------------------------------------------
// Tile grid
//
// Layout rules (content area = SCREEN_WIDTH × UI_CONTENT_H):
//   n=1      → 1 tile, full area
//   n=2      → 2 tiles, side by side (1 row)
//   n=3,4    → 2×2 grid
//   n=5      → 2 columns × 3 rows; 5th tile centred in the last row
//
// PAD pixels of spacing between tiles (and between tiles and content edges).
// ----------------------------------------------------------------------------

static constexpr int TILE_PAD = 2;

static void build_tile_grid(size_t group_idx)
{
    if (!s_content) return;

    // Remove all previous tiles (fires LV_EVENT_DELETE → frees TileUD)
    lv_obj_clean(s_content);

    const area_cache::EntityGroup* g = area_cache::get_group(group_idx);
    const size_t n = g ? g->count : 0;

    if (n == 0) {
        lv_obj_t* hint = lv_label_create(s_content);
        lv_label_set_text(hint, i18n::str(StrId::NO_DEVICES));
        lv_obj_set_style_text_color(hint, lv_color_hex(UI_COL_TEXT_DIM), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_align(hint, LV_ALIGN_CENTER, 0, 0);
        return;
    }

    const int W    = SCREEN_WIDTH;
    const int H    = UI_CONTENT_H;
    const int P    = TILE_PAD;
    const int cols = (n == 1) ? 1 : 2;
    const int rows = (n <= 2) ? 1 : (n <= 4) ? 2 : 3;
    // tile width:  W = (cols+1)*P + cols*tw  →  tw = (W - (cols+1)*P) / cols
    const int tw = (W - (cols + 1) * P) / cols;
    // tile height: H = (rows+1)*P + rows*th  →  th = (H - (rows+1)*P) / rows
    const int th = (H - (rows + 1) * P) / rows;

    for (size_t i = 0; i < n; ++i) {
        const HaEntity* entity = entity_cache::get(g->entity_indices[i]);
        if (!entity) continue;

        int x, y;
        if (n == 5 && i == 4) {
            // 5th tile: centred horizontally in the last row
            x = (W - tw) / 2;
            y = P + 2 * (th + P);
        } else {
            const int col = static_cast<int>(i % 2);
            const int row = static_cast<int>(i / 2);
            x = P + col * (tw + P);
            y = P + row * (th + P);
        }

        tile_widget::create(s_content, *entity, x, y, tw, th);
    }
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

namespace shell {

void show_loading()
{
    lv_obj_t* scr = lv_obj_create(nullptr);
    style_container(scr, lv_color_hex(UI_COL_BG));

    lv_obj_t* title = lv_label_create(scr);
    lv_label_set_text(title, i18n::str(StrId::APP_NAME));
    lv_obj_set_style_text_color(title, lv_color_hex(UI_COL_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t* sub = lv_label_create(scr);
    lv_label_set_text(sub, i18n::str(StrId::CONNECTING_HA));
    lv_obj_set_style_text_color(sub, lv_color_hex(UI_COL_TEXT_DIM), LV_PART_MAIN);
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, 12);

    lv_scr_load(scr);
}

void create()
{
    // ---- Clean up any previous shell before rebuilding ---------------------
    // Three invocation paths:
    //   1. First boot:       s_screen == nullptr → nothing to delete.
    //   2. Locale change / recalibration:
    //        The settings screen or blank cal screen is currently active.
    //        s_screen is NOT the active screen → delete it now; the active
    //        screen is cleaned up by lv_scr_load_anim(auto_del=true) below.
    //   3. HA reconnect after sleep:
    //        s_screen IS the currently active screen. Calling lv_obj_del() on
    //        the active screen leaves disp->act_scr as a dangling pointer and
    //        the next lv_scr_load_anim dereferences it → LoadProhibited crash.
    //        Skip the explicit delete; lv_scr_load_anim(auto_del=true) will
    //        delete the old screen safely once the new one has faded in.
    if (s_screen && lv_scr_act() != s_screen) {
        lv_obj_del(s_screen);
    }
    reset_static_ptrs();

    // ---- Build new shell screen ---------------------------------------------
    s_screen = lv_obj_create(nullptr);
    style_container(s_screen, lv_color_hex(UI_COL_BG));

    // ---- Header bar ---------------------------------------------------------
    lv_obj_t* header = lv_obj_create(s_screen);
    lv_obj_set_size(header, SCREEN_WIDTH, UI_HEADER_H);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(UI_COL_HEADER), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(header, lv_color_hex(UI_COL_BORDER), LV_PART_MAIN);
    lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(header, 0, LV_PART_MAIN);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    // Room name — left-aligned, vertically centered, clipped if too long.
    // Width reserves space for: battery icon (20px) + gap (4px) + gear button (44px)
    //                           + gap (4px) + 2 dots + margins.
    // Right reservation: 4(margin) + 10(ha) + 6(gap) + 10(wifi) + 4(gap) + 44(gear)
    //                    + 4(gap) + 20(battery) = 102px
    s_room_label = lv_label_create(header);
    lv_label_set_long_mode(s_room_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(s_room_label, SCREEN_WIDTH - 8 - 102 - 4);  // ~206 px
    lv_obj_set_style_text_color(s_room_label, lv_color_hex(UI_COL_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_room_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(s_room_label, LV_ALIGN_LEFT_MID, 8, 0);
    lv_label_set_text(s_room_label, "Home");

    // WiFi status dot — use lv_led for correct circle rendering
    s_wifi_dot = lv_led_create(header);
    lv_obj_set_size(s_wifi_dot, UI_DOT_SIZE, UI_DOT_SIZE);
    lv_led_set_color(s_wifi_dot, lv_color_hex(UI_COL_ERR));
    lv_led_on(s_wifi_dot);
    lv_obj_align(s_wifi_dot, LV_ALIGN_RIGHT_MID, -(UI_DOT_SIZE + 6) - 4, 0);

    // HA status dot
    s_ha_dot = lv_led_create(header);
    lv_obj_set_size(s_ha_dot, UI_DOT_SIZE, UI_DOT_SIZE);
    lv_led_set_color(s_ha_dot, lv_color_hex(UI_COL_ERR));
    lv_led_on(s_ha_dot);
    lv_obj_align(s_ha_dot, LV_ALIGN_RIGHT_MID, -4, 0);

    // Settings gear button — to the left of the status dots.
    // Placement: right edge 4px past wifi_dot's left edge.
    // wifi_dot left edge is at RIGHT_MID - 20 - 10 = -30 from right → 290.
    // gear right edge at 290 - 4 = 286 → RIGHT_MID, -34.
    s_settings_btn = lv_btn_create(header);
    lv_obj_remove_style_all(s_settings_btn);
    lv_obj_set_size(s_settings_btn, UI_HEADER_H, UI_HEADER_H);  // full-height touch target
    lv_obj_align(s_settings_btn, LV_ALIGN_RIGHT_MID, -34, 0);
    lv_obj_set_style_bg_color(s_settings_btn, lv_color_hex(UI_COL_HEADER), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_settings_btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_settings_btn, lv_color_hex(UI_COL_NAV_ACTIVE),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(s_settings_btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_event_cb(s_settings_btn, on_settings_click, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* gear_lbl = lv_label_create(s_settings_btn);
    lv_label_set_text(gear_lbl, UI_ICON_COG);
    lv_obj_set_style_text_font(gear_lbl, &lv_font_icons_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(gear_lbl, lv_color_hex(UI_COL_TEXT_DIM), LV_PART_MAIN);
    lv_obj_align(gear_lbl, LV_ALIGN_CENTER, 0, 0);

    // Update available badge — 6 px accent dot, top-right corner of gear button.
    // Hidden by default; revealed via show_update_indicator(true).
    s_update_badge = lv_obj_create(s_settings_btn);
    lv_obj_set_size(s_update_badge, 6, 6);
    lv_obj_align(s_update_badge, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(s_update_badge, lv_color_hex(UI_COL_ACCENT), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_update_badge, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_update_badge, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_update_badge, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_add_flag(s_update_badge, LV_OBJ_FLAG_HIDDEN);

    // Battery mode icon — positioned left of the gear button.
    // Gear button right edge is at (320-34)=286, left at 242.
    // Battery icon: 20 px wide, right edge at 242-4=238 → RIGHT_MID offset = -(320-238) = -82.
    // Hidden by default; revealed via show_battery_indicator(true).
    s_battery_icon = lv_label_create(header);
    lv_obj_set_width(s_battery_icon, 20);
    lv_label_set_long_mode(s_battery_icon, LV_LABEL_LONG_CLIP);
    lv_label_set_text(s_battery_icon, UI_ICON_BATTERY);
    lv_obj_set_style_text_font(s_battery_icon, &lv_font_icons_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_battery_icon, lv_color_hex(UI_COL_TEXT_DIM), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_battery_icon, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(s_battery_icon, LV_ALIGN_RIGHT_MID, -82, 0);
    lv_obj_add_flag(s_battery_icon, LV_OBJ_FLAG_HIDDEN);

    // ---- Content area -------------------------------------------------------
    s_content = lv_obj_create(s_screen);
    lv_obj_set_size(s_content, SCREEN_WIDTH, UI_CONTENT_H);
    lv_obj_set_pos(s_content, 0, UI_HEADER_H);
    style_container(s_content, lv_color_hex(UI_COL_BG));

    // ---- Nav bar ------------------------------------------------------------
    s_nav_bar = lv_obj_create(s_screen);
    lv_obj_set_size(s_nav_bar, SCREEN_WIDTH, UI_NAV_H);
    lv_obj_set_pos(s_nav_bar, 0, SCREEN_HEIGHT - UI_NAV_H);
    lv_obj_set_style_bg_color(s_nav_bar, lv_color_hex(UI_COL_NAV), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_nav_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_side(s_nav_bar, LV_BORDER_SIDE_TOP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_nav_bar, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_nav_bar, lv_color_hex(UI_COL_BORDER), LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_nav_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_nav_bar, 0, LV_PART_MAIN);
    lv_obj_add_flag(s_nav_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_nav_bar, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(s_nav_bar, LV_SCROLLBAR_MODE_OFF);

    // ---- Tab buttons --------------------------------------------------------
    s_group_count       = area_cache::group_count();
    s_weather_tab_btn   = nullptr;

    const bool has_weather = weather_cache::has_weather();

    if (s_group_count == 0 && !has_weather) {
        lv_obj_t* hint = lv_label_create(s_content);
        lv_label_set_text(hint, i18n::str(StrId::NO_ROOMS));
        lv_obj_set_style_text_color(hint, lv_color_hex(UI_COL_TEXT_DIM), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_align(hint, LV_ALIGN_CENTER, 0, 0);
        lv_scr_load_anim(s_screen, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, true);
        return;
    }

    // Tab width: spread evenly when ≤ 4 total tabs, fixed 76 px (scrollable) for more.
    // Weather tab counts as one extra tab.
    const size_t total_tabs = s_group_count + (has_weather ? 1 : 0);
    const int tab_w = (total_tabs <= 4)
        ? SCREEN_WIDTH / static_cast<int>(total_tabs)
        : UI_NAV_TAB_W;

    for (size_t i = 0; i < s_group_count && i < UI_MAX_GROUPS; ++i) {
        const area_cache::EntityGroup* g = area_cache::get_group(i);
        const char* label = (g && g->name[0] != '\0') ? g->name : "?";

        lv_obj_t* btn = lv_btn_create(s_nav_bar);
        lv_obj_set_size(btn, tab_w, UI_NAV_H);
        lv_obj_set_pos(btn, static_cast<int>(i) * tab_w, 0);
        lv_obj_set_style_pad_all(btn, 4, LV_PART_MAIN);
        lv_obj_set_user_data(btn,
            reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
        lv_obj_add_event_cb(btn, on_tab_click, LV_EVENT_CLICKED, nullptr);
        apply_tab_style(btn, i == 0);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, label);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_size(lbl, tab_w - 8,
                        static_cast<lv_coord_t>(lv_font_montserrat_14.line_height));
        lv_obj_set_style_text_color(lbl, lv_color_hex(UI_COL_TEXT), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

        s_tab_btns[i] = btn;
    }

    // ---- Weather tab (rightmost, only when a weather entity exists) ----------
    if (has_weather) {
        const int wx = static_cast<int>(s_group_count) * tab_w;

        lv_obj_t* btn = lv_btn_create(s_nav_bar);
        lv_obj_set_size(btn, tab_w, UI_NAV_H);
        lv_obj_set_pos(btn, wx, 0);
        lv_obj_set_style_pad_all(btn, 4, LV_PART_MAIN);
        lv_obj_add_event_cb(btn, on_weather_tab_click, LV_EVENT_CLICKED, nullptr);
        apply_tab_style(btn, false);

        // Use the icon font for the weather tab label if available;
        // fall back to the localised text label.
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, i18n::str(StrId::WEATHER_TAB));
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_size(lbl, tab_w - 8,
                        static_cast<lv_coord_t>(lv_font_montserrat_14.line_height));
        lv_obj_set_style_text_color(lbl, lv_color_hex(UI_COL_TEXT), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

        s_weather_tab_btn = btn;
    }

    // Activate first group if rooms exist, otherwise show weather tab
    if (s_group_count > 0) {
        set_active_group(0);
    } else {
        set_weather_tab_active();
    }

    // Fade in over the loading screen; auto-delete the loading screen when done
    lv_scr_load_anim(s_screen, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, true);
}

void update_status(bool wifi_connected, ha_client::ConnectionState ha_state)
{
    // Update status dots (safe to call before shell is built)
    if (s_wifi_dot) {
        lv_led_set_color(s_wifi_dot,
            lv_color_hex(wifi_connected ? UI_COL_OK : UI_COL_ERR));
    }
    if (s_ha_dot) {
        lv_led_set_color(s_ha_dot,
            lv_color_hex(ha_state == ha_client::ConnectionState::CONNECTED
                         ? UI_COL_OK : UI_COL_ERR));
    }

    // Banner is only shown once the shell exists (not on boot screens)
    if (!s_screen) return;

    // Determine the error message for the current state.
    // Priority: auth failure > no wifi > HA unreachable.
    const char* msg = nullptr;
    if (ha_state == ha_client::ConnectionState::AUTH_FAILED) {
        msg = i18n::str(StrId::ERR_AUTH_FAILED);
    } else if (!wifi_connected) {
        msg = i18n::str(StrId::WIFI_NO_WIFI);
    } else if (ha_state != ha_client::ConnectionState::CONNECTED) {
        msg = i18n::str(StrId::ERR_HA_UNREACHABLE);
    }

    // Skip rebuild if the message has not changed
    if (msg == s_current_banner_msg) return;
    s_current_banner_msg = msg;

    if (!msg) {
        // Connectivity restored — remove the banner
        if (s_error_banner) {
            lv_obj_del(s_error_banner);
            s_error_banner = nullptr;
        }
        return;
    }

    // Create or reuse the banner overlay
    if (!s_error_banner) {
        s_error_banner = lv_obj_create(s_screen);
        lv_obj_set_size(s_error_banner, SCREEN_WIDTH, UI_CONTENT_H);
        lv_obj_set_pos(s_error_banner, 0, UI_HEADER_H);
        lv_obj_set_style_bg_color(s_error_banner, lv_color_hex(UI_COL_SURFACE), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_error_banner, 215, LV_PART_MAIN); // ~84 % opaque
        lv_obj_set_style_border_width(s_error_banner, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(s_error_banner, 8, LV_PART_MAIN);
        lv_obj_set_style_radius(s_error_banner, 0, LV_PART_MAIN);
        lv_obj_clear_flag(s_error_banner, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lbl = lv_label_create(s_error_banner);
        lv_obj_set_style_text_color(lbl, lv_color_hex(UI_COL_ERR), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_width(lbl, SCREEN_WIDTH - 16);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
        lv_label_set_text(lbl, msg);
    } else {
        // Banner already visible — update text and bring to foreground
        lv_obj_t* lbl = lv_obj_get_child(s_error_banner, 0);
        if (lbl) lv_label_set_text(lbl, msg);
        lv_obj_move_foreground(s_error_banner);
    }
}

lv_obj_t* get_content()
{
    return s_content;
}

size_t active_group()
{
    return s_active_idx;
}

void refresh_weather()
{
    if (!s_content) return;
    // Only redraw if weather tab is the active view
    if (s_active_idx != s_group_count || !s_weather_tab_btn) return;
    weather_screen::refresh(s_content);
}

void show_update_indicator(bool visible)
{
    if (!s_update_badge) return;
    if (visible) {
        lv_obj_clear_flag(s_update_badge, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_update_badge, LV_OBJ_FLAG_HIDDEN);
    }
}

void show_battery_indicator(bool visible)
{
    if (!s_battery_icon) return;
    if (visible) {
        lv_obj_clear_flag(s_battery_icon, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_battery_icon, LV_OBJ_FLAG_HIDDEN);
    }
}

void on_entity_changed(const HaEntity& entity)
{
    // Forward to the detail screen if it is open for this entity
    detail_screen::on_entity_changed(entity);

    if (!s_content) return;

    // Walk the content area's direct children; each is a tile widget.
    // The "No devices" hint label has no TileUD so entity_id() returns nullptr — skip it.
    const uint32_t child_cnt = lv_obj_get_child_cnt(s_content);
    for (uint32_t i = 0; i < child_cnt; ++i) {
        lv_obj_t* child = lv_obj_get_child(s_content, i);
        const char* eid = tile_widget::entity_id(child);
        if (eid && strcmp(eid, entity.entity_id) == 0) {
            tile_widget::update(child, entity);
            break;
        }
    }
}

} // namespace shell
