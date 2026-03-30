#include "shell.h"

#include <Arduino.h>
#include <lvgl.h>

#include "ui_theme.h"
#include "display_config.h"
#include "ha/area_cache.h"
#include "ha_area.h"

// ----------------------------------------------------------------------------
// Module-private state
// ----------------------------------------------------------------------------

namespace {

// Maximum groups the nav bar can hold (MAX_AREAS named + 1 Other)
static constexpr size_t UI_MAX_GROUPS = MAX_AREAS + 1;

// Shell LVGL objects
static lv_obj_t* s_screen     = nullptr;
static lv_obj_t* s_room_label = nullptr;
static lv_obj_t* s_wifi_dot   = nullptr;
static lv_obj_t* s_ha_dot     = nullptr;
static lv_obj_t* s_content    = nullptr;
static lv_obj_t* s_nav_bar    = nullptr;

static lv_obj_t* s_tab_btns[UI_MAX_GROUPS];
static size_t    s_group_count = 0;
static size_t    s_active_idx  = 0;

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

    // Update tab button highlight
    for (size_t i = 0; i < s_group_count; ++i) {
        if (s_tab_btns[i]) apply_tab_style(s_tab_btns[i], i == idx);
    }

    // Update header room name
    const area_cache::EntityGroup* g = area_cache::get_group(idx);
    if (g && s_room_label) {
        lv_label_set_text(s_room_label, (g->name[0] != '\0') ? g->name : "Home");
    }

    // Scroll active tab into view (no-op if already visible)
    if (s_nav_bar && s_tab_btns[idx]) {
        lv_obj_scroll_to_view(s_tab_btns[idx], LV_ANIM_ON);
    }

    s_active_idx = idx;
}

static void on_tab_click(lv_event_t* e)
{
    lv_obj_t* btn = lv_event_get_target(e);
    // Index stored as integer in user_data pointer slot
    size_t idx = static_cast<size_t>(
        reinterpret_cast<uintptr_t>(lv_obj_get_user_data(btn)));
    set_active_group(idx);
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
    lv_label_set_text(title, "Home Remote");
    lv_obj_set_style_text_color(title, lv_color_hex(UI_COL_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t* sub = lv_label_create(scr);
    lv_label_set_text(sub, "Connecting to Home Assistant...");
    lv_obj_set_style_text_color(sub, lv_color_hex(UI_COL_TEXT_DIM), LV_PART_MAIN);
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, 12);

    lv_scr_load(scr);
}

void create()
{
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

    // Room name — left-aligned, vertically centered, clipped if too long
    s_room_label = lv_label_create(header);
    lv_label_set_long_mode(s_room_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(s_room_label, SCREEN_WIDTH - 50); // reserve space for dots
    lv_obj_set_style_text_color(s_room_label, lv_color_hex(UI_COL_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_room_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(s_room_label, LV_ALIGN_LEFT_MID, 8, 0);
    lv_label_set_text(s_room_label, "Home");

    // WiFi status dot (red until update_status is called)
    s_wifi_dot = lv_obj_create(header);
    lv_obj_set_size(s_wifi_dot, UI_DOT_SIZE, UI_DOT_SIZE);
    lv_obj_set_style_radius(s_wifi_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_wifi_dot, lv_color_hex(UI_COL_ERR), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_wifi_dot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_wifi_dot, 0, LV_PART_MAIN);
    lv_obj_align(s_wifi_dot, LV_ALIGN_RIGHT_MID, -(UI_DOT_SIZE + 6) - 4, 0);

    // HA status dot
    s_ha_dot = lv_obj_create(header);
    lv_obj_set_size(s_ha_dot, UI_DOT_SIZE, UI_DOT_SIZE);
    lv_obj_set_style_radius(s_ha_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_ha_dot, lv_color_hex(UI_COL_ERR), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_ha_dot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_ha_dot, 0, LV_PART_MAIN);
    lv_obj_align(s_ha_dot, LV_ALIGN_RIGHT_MID, -4, 0);

    // ---- Content area -------------------------------------------------------
    s_content = lv_obj_create(s_screen);
    lv_obj_set_size(s_content, SCREEN_WIDTH, UI_CONTENT_H);
    lv_obj_set_pos(s_content, 0, UI_HEADER_H);
    style_container(s_content, lv_color_hex(UI_COL_BG));
    // TICKET-009 will add tile widgets here

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
    s_group_count = area_cache::group_count();

    if (s_group_count == 0) {
        lv_obj_t* hint = lv_label_create(s_content);
        lv_label_set_text(hint, "No rooms found in\nHome Assistant");
        lv_obj_set_style_text_color(hint, lv_color_hex(UI_COL_TEXT_DIM), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_align(hint, LV_ALIGN_CENTER, 0, 0);
        lv_scr_load_anim(s_screen, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, true);
        return;
    }

    // Tab width: spread evenly for ≤ 4 groups, fixed 76 px (scrollable) for more
    const int tab_w = (s_group_count <= 4)
        ? SCREEN_WIDTH / static_cast<int>(s_group_count)
        : UI_NAV_TAB_W;

    for (size_t i = 0; i < s_group_count && i < UI_MAX_GROUPS; ++i) {
        const area_cache::EntityGroup* g = area_cache::get_group(i);
        const char* label = (g && g->name[0] != '\0') ? g->name : "Other";

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
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(lbl, tab_w - 8);
        lv_obj_set_style_text_color(lbl, lv_color_hex(UI_COL_TEXT), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

        s_tab_btns[i] = btn;
    }

    // Activate first group (updates header label and tab highlight)
    set_active_group(0);

    // Fade in over the loading screen; auto-delete the loading screen when done
    lv_scr_load_anim(s_screen, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, true);
}

void update_status(bool wifi_connected, bool ha_connected)
{
    if (!s_wifi_dot || !s_ha_dot) return;
    lv_obj_set_style_bg_color(s_wifi_dot,
        lv_color_hex(wifi_connected ? UI_COL_OK : UI_COL_ERR), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_ha_dot,
        lv_color_hex(ha_connected ? UI_COL_OK : UI_COL_ERR), LV_PART_MAIN);
}

lv_obj_t* get_content()
{
    return s_content;
}

size_t active_group()
{
    return s_active_idx;
}

} // namespace shell
