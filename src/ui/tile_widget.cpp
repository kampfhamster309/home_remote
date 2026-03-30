#include "tile_widget.h"

#include <Arduino.h>
#include <lvgl.h>
#include <cstring>

#include "ui_theme.h"
#include "ha/entity_cache.h"
#include "ha/ha_client.h"

// ----------------------------------------------------------------------------
// Module internals
// ----------------------------------------------------------------------------

namespace {

// Per-tile heap record — freed in on_tile_delete via LV_EVENT_DELETE.
struct TileUD {
    char       entity_id[64];
    lv_obj_t*  icon_lbl;
    lv_obj_t*  name_lbl;
    lv_obj_t*  state_lbl;  // may be nullptr for very small tiles
    bool       pending;    // optimistic pending state between tap and confirmed push
};

// ---- Domain helpers --------------------------------------------------------

static const char* domain_icon(EntityDomain d)
{
    switch (d) {
        case EntityDomain::LIGHT:         return LV_SYMBOL_CHARGE;
        case EntityDomain::SWITCH:        return LV_SYMBOL_POWER;
        case EntityDomain::COVER:         return LV_SYMBOL_UP;
        case EntityDomain::CLIMATE:       return LV_SYMBOL_SETTINGS;
        case EntityDomain::SENSOR:        return LV_SYMBOL_EYE_OPEN;
        case EntityDomain::BINARY_SENSOR: return LV_SYMBOL_EYE_OPEN;
        case EntityDomain::AUTOMATION:    return LV_SYMBOL_LOOP;
        case EntityDomain::SCRIPT:        return LV_SYMBOL_PLAY;
        case EntityDomain::SCENE:         return LV_SYMBOL_IMAGE;
        case EntityDomain::INPUT_BOOLEAN: return LV_SYMBOL_POWER;
        case EntityDomain::MEDIA_PLAYER:  return LV_SYMBOL_AUDIO;
        case EntityDomain::FAN:           return LV_SYMBOL_LOOP;
        case EntityDomain::LOCK:          return LV_SYMBOL_CLOSE;
        default:                          return LV_SYMBOL_HOME;
    }
}

static bool is_on(const HaEntity& e)
{
    // Treat common "active" state strings as on
    return (strcmp(e.state, "on")       == 0
         || strcmp(e.state, "open")     == 0
         || strcmp(e.state, "unlocked") == 0
         || strcmp(e.state, "playing")  == 0
         || strcmp(e.state, "home")     == 0);
}

static bool is_unavailable(const HaEntity& e)
{
    return (strcmp(e.state, "unavailable") == 0
         || strcmp(e.state, "unknown")     == 0);
}

// Sensors and climate don't have a simple on/off action.
static bool is_display_only(const HaEntity& e)
{
    return (e.domain == EntityDomain::SENSOR
         || e.domain == EntityDomain::BINARY_SENSOR
         || e.domain == EntityDomain::CLIMATE);
}

static lv_color_t tile_bg_color(const HaEntity& e, bool pending)
{
    if (pending)               return lv_color_hex(UI_COL_TILE_PENDING);
    if (is_unavailable(e))     return lv_color_hex(UI_COL_TILE_UNAVAIL);
    if (e.domain == EntityDomain::SENSOR
     || e.domain == EntityDomain::BINARY_SENSOR)
                               return lv_color_hex(UI_COL_TILE_SENSOR);
    if (is_on(e))              return lv_color_hex(UI_COL_TILE_ON);
    return lv_color_hex(UI_COL_TILE_OFF);
}

// For sensors / climate write the current value; for on/off colour says it all.
static void get_state_text(char* dst, size_t max, const HaEntity& e)
{
    if (e.domain == EntityDomain::SENSOR) {
        strncpy(dst, e.state, max - 1);
        dst[max - 1] = '\0';
    } else if (e.domain == EntityDomain::CLIMATE && e.attrs.has_temperature) {
        // Format float without snprintf to avoid pulling in FP printf (~1KB libc overhead)
        int whole = static_cast<int>(e.attrs.temperature);
        int frac  = static_cast<int>((e.attrs.temperature - whole) * 10.0f);
        if (frac < 0) frac = -frac;
        size_t n = 0;
        if (whole < 0) { if (n+1<max) dst[n++] = '-'; whole = -whole; }
        // write integer part
        char tmp[8]; int tl = 0;
        do { tmp[tl++] = '0' + (whole % 10); whole /= 10; } while (whole && tl < 7);
        while (tl > 0 && n+1 < max) dst[n++] = tmp[--tl];
        if (n+1<max) dst[n++] = '.';
        if (n+1<max) dst[n++] = '0' + frac;
        dst[n] = '\0';
    } else {
        dst[0] = '\0';
    }
}

// ---- Service call ----------------------------------------------------------

static void call_toggle(const HaEntity& e)
{
    switch (e.domain) {
        case EntityDomain::LIGHT:
            ha_client::call_service("light",        "toggle",                    e.entity_id);
            break;
        case EntityDomain::SWITCH:
            ha_client::call_service("switch",       "toggle",                    e.entity_id);
            break;
        case EntityDomain::COVER:
            ha_client::call_service("cover",
                is_on(e) ? "close_cover" : "open_cover",                         e.entity_id);
            break;
        case EntityDomain::INPUT_BOOLEAN:
            ha_client::call_service("input_boolean","toggle",                    e.entity_id);
            break;
        case EntityDomain::AUTOMATION:
            ha_client::call_service("automation",   "toggle",                    e.entity_id);
            break;
        case EntityDomain::FAN:
            ha_client::call_service("fan",          "toggle",                    e.entity_id);
            break;
        case EntityDomain::LOCK:
            ha_client::call_service("lock",
                (strcmp(e.state, "locked") == 0) ? "unlock" : "lock",           e.entity_id);
            break;
        case EntityDomain::MEDIA_PLAYER:
            ha_client::call_service("media_player", "toggle",                    e.entity_id);
            break;
        case EntityDomain::SCRIPT:
            ha_client::call_service("script",       "turn_on",                   e.entity_id);
            break;
        case EntityDomain::SCENE:
            ha_client::call_service("scene",        "turn_on",                   e.entity_id);
            break;
        default:
            break;
    }
}

// ---- LVGL event callbacks --------------------------------------------------

static void on_tile_click(lv_event_t* e)
{
    lv_obj_t* tile = lv_event_get_target(e);
    TileUD* ud = static_cast<TileUD*>(lv_obj_get_user_data(tile));
    if (!ud) return;

    const HaEntity* entity = entity_cache::find(ud->entity_id);
    if (!entity || is_display_only(*entity) || is_unavailable(*entity)) return;

    // Optimistic feedback: show pending colour immediately
    ud->pending = true;
    lv_obj_set_style_bg_color(tile, lv_color_hex(UI_COL_TILE_PENDING), LV_PART_MAIN);
    // Dim icon to signal pending
    if (ud->icon_lbl) {
        lv_obj_set_style_text_color(ud->icon_lbl,
            lv_color_hex(UI_COL_TEXT_DIM), LV_PART_MAIN);
    }

    call_toggle(*entity);
}

static void on_tile_delete(lv_event_t* e)
{
    lv_obj_t* tile = lv_event_get_target(e);
    TileUD* ud = static_cast<TileUD*>(lv_obj_get_user_data(tile));
    delete ud;
    lv_obj_set_user_data(tile, nullptr);
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

namespace tile_widget {

lv_obj_t* create(lv_obj_t* parent, const HaEntity& entity, int x, int y, int w, int h)
{
    lv_obj_t* tile = lv_btn_create(parent);
    lv_obj_remove_style_all(tile);  // reset LVGL default button styles
    lv_obj_set_size(tile, w, h);
    lv_obj_set_pos(tile, x, y);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);

    // Background
    lv_obj_set_style_bg_color(tile, tile_bg_color(entity, false), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, LV_PART_MAIN);
    // Subtle pressed feedback
    lv_obj_set_style_bg_color(tile, lv_color_hex(UI_COL_TILE_PENDING),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    // Border
    lv_obj_set_style_border_color(tile, lv_color_hex(UI_COL_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_width(tile, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(tile, 4, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(tile, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(tile, 0, LV_PART_MAIN);

    // Per-tile data
    TileUD* ud = new TileUD{};
    strncpy(ud->entity_id, entity.entity_id, sizeof(ud->entity_id) - 1);
    ud->entity_id[sizeof(ud->entity_id) - 1] = '\0';
    ud->pending    = false;
    ud->icon_lbl   = nullptr;
    ud->name_lbl   = nullptr;
    ud->state_lbl  = nullptr;
    lv_obj_set_user_data(tile, ud);

    lv_obj_add_event_cb(tile, on_tile_click,  LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(tile, on_tile_delete, LV_EVENT_DELETE,  nullptr);

    // ---- Icon (top-left, montserrat_20) ------------------------------------
    ud->icon_lbl = lv_label_create(tile);
    lv_label_set_text(ud->icon_lbl, domain_icon(entity.domain));
    lv_obj_set_style_text_font(ud->icon_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(ud->icon_lbl,
        is_on(entity) ? lv_color_hex(UI_COL_ACCENT) : lv_color_hex(UI_COL_TEXT_DIM),
        LV_PART_MAIN);
    lv_obj_align(ud->icon_lbl, LV_ALIGN_TOP_LEFT, 5, 4);

    // ---- Friendly name (below icon, montserrat_14, clipped) ----------------
    ud->name_lbl = lv_label_create(tile);
    lv_label_set_text(ud->name_lbl,
        entity.friendly_name[0] ? entity.friendly_name : entity.entity_id);
    lv_label_set_long_mode(ud->name_lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(ud->name_lbl, w - 10);
    lv_obj_set_style_text_font(ud->name_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(ud->name_lbl, lv_color_hex(UI_COL_TEXT), LV_PART_MAIN);
    // Place below icon: icon ~22 px tall + 4 px top offset + 2 px gap = y=28
    lv_obj_align(ud->name_lbl, LV_ALIGN_TOP_LEFT, 5, 28);

    // ---- State text (bottom-left, sensors/climate only) --------------------
    char state_buf[24];
    get_state_text(state_buf, sizeof(state_buf), entity);

    if (state_buf[0] != '\0' && h >= 56) {
        ud->state_lbl = lv_label_create(tile);
        lv_label_set_text(ud->state_lbl, state_buf);
        lv_label_set_long_mode(ud->state_lbl, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(ud->state_lbl, w - 10);
        lv_obj_set_style_text_font(ud->state_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(ud->state_lbl,
            lv_color_hex(UI_COL_TEXT_DIM), LV_PART_MAIN);
        lv_obj_align(ud->state_lbl, LV_ALIGN_BOTTOM_LEFT, 5, -3);
    }

    return tile;
}

void update(lv_obj_t* tile, const HaEntity& entity)
{
    TileUD* ud = static_cast<TileUD*>(lv_obj_get_user_data(tile));
    if (!ud) return;

    // State confirmed — clear pending
    ud->pending = false;

    lv_obj_set_style_bg_color(tile, tile_bg_color(entity, false), LV_PART_MAIN);

    // Update icon colour based on new state
    if (ud->icon_lbl) {
        lv_obj_set_style_text_color(ud->icon_lbl,
            is_on(entity) ? lv_color_hex(UI_COL_ACCENT) : lv_color_hex(UI_COL_TEXT_DIM),
            LV_PART_MAIN);
    }

    // Refresh state text if the state label exists
    if (ud->state_lbl) {
        char state_buf[24];
        get_state_text(state_buf, sizeof(state_buf), entity);
        lv_label_set_text(ud->state_lbl, state_buf);
    }
}

const char* entity_id(lv_obj_t* tile)
{
    const TileUD* ud = static_cast<const TileUD*>(lv_obj_get_user_data(tile));
    return ud ? ud->entity_id : nullptr;
}

} // namespace tile_widget
