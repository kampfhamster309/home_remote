#include "weather_screen.h"

#include <Arduino.h>
#include <lvgl.h>
#include <cstdio>

#include "ui_theme.h"
#include "ui_fonts.h"
#include "ui_icons.h"
#include "display_config.h"
#include "ha/weather_cache.h"
#include "i18n/i18n.h"

// ----------------------------------------------------------------------------
// Layout constants (content area = SCREEN_WIDTH × UI_CONTENT_H = 320 × 160)
// ----------------------------------------------------------------------------

static constexpr int W = SCREEN_WIDTH;   // 320
static constexpr int H = UI_CONTENT_H;  // 160

namespace weather_screen {

// Build all child widgets from scratch inside `parent`.
void create(lv_obj_t* parent)
{
    lv_obj_clean(parent);

    const WeatherData& wd = weather_cache::get();

    if (!weather_cache::has_weather()) {
        lv_obj_t* hint = lv_label_create(parent);
        lv_label_set_text(hint, i18n::str(StrId::WEATHER_UNAVAIL));
        lv_obj_set_style_text_color(hint, lv_color_hex(UI_COL_TEXT_DIM), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(hint, LV_ALIGN_CENTER, 0, 0);
        return;
    }

    // ---- Condition icon (large, centered top half) ---------------------------
    lv_obj_t* icon_lbl = lv_label_create(parent);
    lv_label_set_text(icon_lbl, weather_cache::condition_icon(wd.condition));
    lv_obj_set_style_text_font(icon_lbl, &lv_font_icons_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(icon_lbl, lv_color_hex(UI_COL_ACCENT), LV_PART_MAIN);
    lv_obj_align(icon_lbl, LV_ALIGN_TOP_MID, 0, 14);

    // ---- Condition label ----------------------------------------------------
    const bool is_de = (i18n::get_locale() == Locale::DE);
    lv_obj_t* cond_lbl = lv_label_create(parent);
    lv_label_set_text(cond_lbl,
        weather_cache::condition_label(wd.condition, is_de));
    lv_obj_set_style_text_font(cond_lbl, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(cond_lbl, lv_color_hex(UI_COL_TEXT), LV_PART_MAIN);
    lv_obj_align(cond_lbl, LV_ALIGN_TOP_MID, 0, 42);

    // ---- Current temperature ------------------------------------------------
    if (wd.has_temperature) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f\xc2\xb0" "C", wd.temperature);
        lv_obj_t* temp_lbl = lv_label_create(parent);
        lv_label_set_text(temp_lbl, buf);
        lv_obj_set_style_text_font(temp_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_set_style_text_color(temp_lbl, lv_color_hex(UI_COL_TEXT), LV_PART_MAIN);
        lv_obj_align(temp_lbl, LV_ALIGN_TOP_MID, 0, 68);
    }

    // ---- Forecast row (high / low / precip) ---------------------------------
    if (wd.has_forecast) {
        char hl_buf[48];
        snprintf(hl_buf, sizeof(hl_buf), "%s%.0f\xc2\xb0" "C - " "%s%.0f\xc2\xb0" "C",
                 i18n::str(StrId::WEATHER_HIGH), wd.temp_high,
                 i18n::str(StrId::WEATHER_LOW),  wd.temp_low);
        lv_obj_t* hl_lbl = lv_label_create(parent);
        lv_label_set_text(hl_lbl, hl_buf);
        lv_obj_set_style_text_font(hl_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(hl_lbl, lv_color_hex(UI_COL_TEXT_DIM), LV_PART_MAIN);
        lv_obj_align(hl_lbl, LV_ALIGN_TOP_MID, 0, 104);

        // Precipitation probability
        char pr_buf[24];
        snprintf(pr_buf, sizeof(pr_buf), "%s%u%%",
                 i18n::str(StrId::WEATHER_PRECIP),
                 static_cast<unsigned>(wd.precip_probability));
        lv_obj_t* pr_lbl = lv_label_create(parent);
        lv_label_set_text(pr_lbl, pr_buf);
        lv_obj_set_style_text_font(pr_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(pr_lbl, lv_color_hex(UI_COL_TEXT_DIM), LV_PART_MAIN);
        lv_obj_align(pr_lbl, LV_ALIGN_TOP_MID, 0, 128);
    }
}

void refresh(lv_obj_t* parent)
{
    // Simplest correct approach: tear down and rebuild.
    // The content area is small (6–8 labels max) so this is cheap.
    create(parent);
}

} // namespace weather_screen
