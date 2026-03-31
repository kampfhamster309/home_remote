#include "detail_screen.h"

#include <Arduino.h>
#include <lvgl.h>
#include <ArduinoJson.h>
#include <cstring>

#include "ui_theme.h"
#include "display_config.h"
#include "ha/entity_cache.h"
#include "ha/ha_client.h"
#include "i18n/i18n.h"
#include "ui_icons.h"

// ----------------------------------------------------------------------------
// Module-private state
// ----------------------------------------------------------------------------

namespace {

// Header height for the detail screen.
// Must be ≥ 44 px: the back button is a touch target.
static constexpr int DETAIL_HEADER_H = 44;

// Which HA attribute a slider controls.
enum class SliderTarget : uint8_t {
    BRIGHTNESS,   // light.turn_on  { brightness: 0–255 }
    COLOR_TEMP,   // light.turn_on  { color_temp: 153–500 mireds }
    TARGET_TEMP,  // climate.set_temperature { temperature: 10–35 °C }
    POSITION,     // cover.set_cover_position { position: 0–100 % }
};

// Heap record attached to each slider via lv_obj_set_user_data.
// Freed in on_slider_delete (LV_EVENT_DELETE).
struct SliderUD {
    char         entity_id[64];
    SliderTarget target;
    lv_obj_t*    value_lbl;  // label displaying the current numeric value
};

static lv_obj_t* s_screen       = nullptr;
static lv_obj_t* s_prev_screen  = nullptr;
static char      s_entity_id[64] = {};

// Up to 2 sliders per detail screen (brightness + color_temp for lights)
static lv_obj_t* s_sliders[2]   = { nullptr, nullptr };
static int       s_slider_count = 0;

// ----------------------------------------------------------------------------
// Value formatting — integer arithmetic only (no snprintf %f / %d overhead)
// ----------------------------------------------------------------------------

static void format_value(char* dst, size_t max, SliderTarget t, int val)
{
    // Write decimal digits of |val| into tmp[], then reverse into dst
    auto write_uint = [&](int v, size_t& n) {
        char tmp[8]; int tl = 0;
        if (v == 0) { if (n + 1 < max) dst[n++] = '0'; return; }
        while (v > 0 && tl < 7) { tmp[tl++] = static_cast<char>('0' + v % 10); v /= 10; }
        while (tl > 0 && n + 1 < max) dst[n++] = tmp[--tl];
    };

    size_t n = 0;
    switch (t) {
        case SliderTarget::BRIGHTNESS: {
            int pct = val * 100 / 255;
            write_uint(pct, n);
            if (n + 1 < max) dst[n++] = '%';
            break;
        }
        case SliderTarget::COLOR_TEMP: {
            write_uint(val, n);
            // no unit suffix — context is clear from label
            break;
        }
        case SliderTarget::TARGET_TEMP: {
            if (val < 0) { if (n + 1 < max) dst[n++] = '-'; val = -val; }
            write_uint(val, n);
            // ASCII-safe degree symbol: just append 'C' (TICKET-011 will improve)
            if (n + 1 < max) dst[n++] = 'C';
            break;
        }
        case SliderTarget::POSITION: {
            write_uint(val, n);
            if (n + 1 < max) dst[n++] = '%';
            break;
        }
    }
    dst[n] = '\0';
}

// ----------------------------------------------------------------------------
// HA service calls
// ----------------------------------------------------------------------------

static void send_slider_value(SliderTarget t, const char* entity_id, int val)
{
    StaticJsonDocument<64> doc;
    switch (t) {
        case SliderTarget::BRIGHTNESS:
            doc["brightness"] = val;
            ha_client::call_service_ex("light", "turn_on", entity_id,
                                       doc.as<JsonObject>());
            break;
        case SliderTarget::COLOR_TEMP:
            doc["color_temp"] = val;
            ha_client::call_service_ex("light", "turn_on", entity_id,
                                       doc.as<JsonObject>());
            break;
        case SliderTarget::TARGET_TEMP:
            doc["temperature"] = val;
            ha_client::call_service_ex("climate", "set_temperature", entity_id,
                                       doc.as<JsonObject>());
            break;
        case SliderTarget::POSITION:
            doc["position"] = val;
            ha_client::call_service_ex("cover", "set_cover_position", entity_id,
                                       doc.as<JsonObject>());
            break;
    }
}

// ----------------------------------------------------------------------------
// LVGL event callbacks
// ----------------------------------------------------------------------------

// Update value label while dragging (live feedback, no HA call yet)
static void on_slider_changed(lv_event_t* e)
{
    lv_obj_t* slider = lv_event_get_target(e);
    const SliderUD* ud = static_cast<const SliderUD*>(lv_obj_get_user_data(slider));
    if (!ud || !ud->value_lbl) return;
    char buf[16];
    format_value(buf, sizeof(buf), ud->target, lv_slider_get_value(slider));
    lv_label_set_text(ud->value_lbl, buf);
}

// Send final value to HA when finger lifts (debounce: one call per gesture)
static void on_slider_released(lv_event_t* e)
{
    lv_obj_t* slider = lv_event_get_target(e);
    const SliderUD* ud = static_cast<const SliderUD*>(lv_obj_get_user_data(slider));
    if (!ud) return;
    send_slider_value(ud->target, ud->entity_id, lv_slider_get_value(slider));
}

static void on_slider_delete(lv_event_t* e)
{
    lv_obj_t* slider = lv_event_get_target(e);
    SliderUD* ud = static_cast<SliderUD*>(lv_obj_get_user_data(slider));
    delete ud;
    lv_obj_set_user_data(slider, nullptr);
}

static void on_back_click(lv_event_t* /*e*/)
{
    if (!s_screen || !s_prev_screen) return;
    lv_obj_t* prev = s_prev_screen;
    s_screen      = nullptr;
    s_prev_screen = nullptr;
    s_sliders[0] = s_sliders[1] = nullptr;
    s_slider_count = 0;
    s_entity_id[0] = '\0';
    // auto_del=true: LVGL deletes the current (detail) screen after the animation
    lv_scr_load_anim(prev, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, true);
}

// ----------------------------------------------------------------------------
// Slider row builder
// ----------------------------------------------------------------------------

// Creates a label row (name on left, value on right) and a slider below it.
// Returns the slider object.
static lv_obj_t* add_slider_row(lv_obj_t* parent,
                                  const char* label_text,
                                  int y,
                                  int min_val, int max_val, int cur_val,
                                  const char* entity_id, SliderTarget target)
{
    // Row label (e.g. "Brightness")
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(UI_COL_TEXT_DIM), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(lbl, 10, y);

    // Value label (right side, updates live while dragging)
    lv_obj_t* val_lbl = lv_label_create(parent);
    char buf[16];
    format_value(buf, sizeof(buf), target, cur_val);
    lv_label_set_text(val_lbl, buf);
    lv_obj_set_style_text_color(val_lbl, lv_color_hex(UI_COL_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_font(val_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_width(val_lbl, 60);
    lv_obj_set_pos(val_lbl, SCREEN_WIDTH - 70, y);

    // Slider
    const int SLIDER_W = SCREEN_WIDTH - 20;
    const int SLIDER_H = 20;
    lv_obj_t* slider = lv_slider_create(parent);
    lv_obj_set_size(slider, SLIDER_W, SLIDER_H);
    lv_obj_set_pos(slider, 10, y + 22);
    lv_slider_set_range(slider, min_val, max_val);
    lv_slider_set_value(slider, cur_val, LV_ANIM_OFF);

    // Track
    lv_obj_set_style_bg_color(slider, lv_color_hex(UI_COL_SURFACE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, 4, LV_PART_MAIN);
    // Filled indicator
    lv_obj_set_style_bg_color(slider, lv_color_hex(UI_COL_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, 4, LV_PART_INDICATOR);
    // Knob — larger padding for resistive touch target
    lv_obj_set_style_bg_color(slider, lv_color_hex(UI_COL_TEXT), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 6, LV_PART_KNOB);

    // Per-slider heap record
    SliderUD* ud = new SliderUD{};
    strncpy(ud->entity_id, entity_id, sizeof(ud->entity_id) - 1);
    ud->entity_id[sizeof(ud->entity_id) - 1] = '\0';
    ud->target    = target;
    ud->value_lbl = val_lbl;
    lv_obj_set_user_data(slider, ud);

    lv_obj_add_event_cb(slider, on_slider_changed,  LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(slider, on_slider_released,  LV_EVENT_RELEASED,      nullptr);
    lv_obj_add_event_cb(slider, on_slider_delete,    LV_EVENT_DELETE,        nullptr);

    return slider;
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

namespace detail_screen {

void open(const char* entity_id)
{
    const HaEntity* entity = entity_cache::find(entity_id);
    if (!entity || !has_detail(*entity)) return;
    if (s_screen) return; // already open (guard against double long-press)

    s_prev_screen = lv_scr_act();
    strncpy(s_entity_id, entity_id, sizeof(s_entity_id) - 1);
    s_entity_id[sizeof(s_entity_id) - 1] = '\0';
    s_slider_count = 0;
    s_sliders[0] = s_sliders[1] = nullptr;

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
    lv_obj_set_size(header, SCREEN_WIDTH, DETAIL_HEADER_H);
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
    lv_obj_set_size(back_btn, DETAIL_HEADER_H, DETAIL_HEADER_H);
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

    // Entity friendly name
    lv_obj_t* name_lbl = lv_label_create(header);
    lv_label_set_text(name_lbl,
        entity->friendly_name[0] ? entity->friendly_name : entity->entity_id);
    lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(name_lbl, SCREEN_WIDTH - DETAIL_HEADER_H - 8);
    lv_obj_set_style_text_color(name_lbl, lv_color_hex(UI_COL_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(name_lbl, LV_ALIGN_LEFT_MID, DETAIL_HEADER_H + 4, 0);

    // ---- Content area (below header) ---------------------------------------
    const int CONTENT_H = SCREEN_HEIGHT - DETAIL_HEADER_H;
    lv_obj_t* content = lv_obj_create(s_screen);
    lv_obj_set_size(content, SCREEN_WIDTH, CONTENT_H);
    lv_obj_set_pos(content, 0, DETAIL_HEADER_H);
    lv_obj_set_style_bg_color(content, lv_color_hex(UI_COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(content, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(content, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(content, 0, LV_PART_MAIN);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    // ---- Icon + current state ----------------------------------------------
    const char* icon_sym;
    switch (entity->domain) {
        case EntityDomain::LIGHT:   icon_sym = UI_ICON_LIGHTBULB;   break;
        case EntityDomain::CLIMATE: icon_sym = UI_ICON_THERMOMETER;  break;
        case EntityDomain::COVER:   icon_sym = UI_ICON_GRIP_LINES;   break;
        default:                    icon_sym = UI_ICON_HOME;         break;
    }

    lv_obj_t* icon_lbl = lv_label_create(content);
    lv_label_set_text(icon_lbl, icon_sym);
    lv_obj_set_style_text_font(icon_lbl, &lv_font_icons_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(icon_lbl, lv_color_hex(UI_COL_ACCENT), LV_PART_MAIN);
    lv_obj_set_pos(icon_lbl, 10, 10);

    lv_obj_t* state_lbl = lv_label_create(content);
    lv_label_set_text(state_lbl, entity->state);
    lv_obj_set_style_text_color(state_lbl, lv_color_hex(UI_COL_TEXT_DIM), LV_PART_MAIN);
    lv_obj_set_style_text_font(state_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(state_lbl, 44, 15);

    // ---- Sliders -----------------------------------------------------------
    // First slider row starts below the icon area.
    // Each row: label(22px) + slider(20px) + gap(12px) = 54px
    int slider_y = 52;

    if (entity->domain == EntityDomain::LIGHT) {
        if (entity->attrs.has_brightness && s_slider_count < 2) {
            s_sliders[s_slider_count++] = add_slider_row(
                content, i18n::str(StrId::DETAIL_BRIGHTNESS), slider_y,
                0, 255, static_cast<int>(entity->attrs.brightness),
                entity_id, SliderTarget::BRIGHTNESS);
            slider_y += 54;
        }
        if (entity->attrs.has_color_temp && s_slider_count < 2) {
            const int ct = entity->attrs.color_temp > 0
                ? static_cast<int>(entity->attrs.color_temp) : 300;
            s_sliders[s_slider_count++] = add_slider_row(
                content, i18n::str(StrId::DETAIL_COLOR_TEMP), slider_y,
                153, 500, ct,
                entity_id, SliderTarget::COLOR_TEMP);
        }
    } else if (entity->domain == EntityDomain::CLIMATE &&
               entity->attrs.has_target_temp) {
        int t = static_cast<int>(entity->attrs.target_temp);
        if (t < 10) t = 20;
        if (t > 35) t = 35;
        s_sliders[s_slider_count++] = add_slider_row(
            content, i18n::str(StrId::DETAIL_TARGET_TEMP), slider_y,
            10, 35, t,
            entity_id, SliderTarget::TARGET_TEMP);
    } else if (entity->domain == EntityDomain::COVER &&
               entity->attrs.has_position) {
        s_sliders[s_slider_count++] = add_slider_row(
            content, i18n::str(StrId::DETAIL_POSITION), slider_y,
            0, 100, static_cast<int>(entity->attrs.position),
            entity_id, SliderTarget::POSITION);
    }

    // Fade in; don't auto-delete the shell screen behind us
    lv_scr_load_anim(s_screen, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, false);
}

void on_entity_changed(const HaEntity& entity)
{
    if (!s_screen) return;
    if (strcmp(entity.entity_id, s_entity_id) != 0) return;

    for (int i = 0; i < s_slider_count; ++i) {
        if (!s_sliders[i]) continue;
        const SliderUD* ud =
            static_cast<const SliderUD*>(lv_obj_get_user_data(s_sliders[i]));
        if (!ud) continue;

        int new_val = lv_slider_get_value(s_sliders[i]); // fallback: keep current
        switch (ud->target) {
            case SliderTarget::BRIGHTNESS:
                if (entity.attrs.has_brightness)
                    new_val = static_cast<int>(entity.attrs.brightness);
                break;
            case SliderTarget::COLOR_TEMP:
                if (entity.attrs.has_color_temp && entity.attrs.color_temp > 0)
                    new_val = static_cast<int>(entity.attrs.color_temp);
                break;
            case SliderTarget::TARGET_TEMP:
                if (entity.attrs.has_target_temp)
                    new_val = static_cast<int>(entity.attrs.target_temp);
                break;
            case SliderTarget::POSITION:
                if (entity.attrs.has_position)
                    new_val = static_cast<int>(entity.attrs.position);
                break;
        }
        lv_slider_set_value(s_sliders[i], new_val, LV_ANIM_ON);
        if (ud->value_lbl) {
            char buf[16];
            format_value(buf, sizeof(buf), ud->target, new_val);
            lv_label_set_text(const_cast<lv_obj_t*>(ud->value_lbl), buf);
        }
    }
}

bool is_open()
{
    return s_screen != nullptr;
}

} // namespace detail_screen
