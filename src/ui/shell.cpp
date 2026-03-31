#include "shell.h"

#include <Arduino.h>
#include <lvgl.h>
#include <cstring>

#include "ui_theme.h"
#include "ui_fonts.h"
#include "display_config.h"
#include "ha/area_cache.h"
#include "ha/entity_cache.h"
#include "ha_area.h"
#include "tile_widget.h"
#include "detail_screen.h"
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
static size_t    s_group_count = 0;
static size_t    s_active_idx  = 0;

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

    // Update tab button highlight
    for (size_t i = 0; i < s_group_count; ++i) {
        if (s_tab_btns[i]) apply_tab_style(s_tab_btns[i], i == idx);
    }

    // Update header room name (full transliterated name)
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
    s_group_count = area_cache::group_count();

    if (s_group_count == 0) {
        lv_obj_t* hint = lv_label_create(s_content);
        lv_label_set_text(hint, i18n::str(StrId::NO_ROOMS));
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
    lv_led_set_color(s_wifi_dot, lv_color_hex(wifi_connected ? UI_COL_OK : UI_COL_ERR));
    lv_led_set_color(s_ha_dot,   lv_color_hex(ha_connected  ? UI_COL_OK : UI_COL_ERR));
}

lv_obj_t* get_content()
{
    return s_content;
}

size_t active_group()
{
    return s_active_idx;
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
