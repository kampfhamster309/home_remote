#pragma once

// Custom domain icons — Font Awesome 5 Solid subset compiled into
// lv_font_icons_20.c (20 px, 4 bpp).  All codepoints are in the Unicode
// Private Use Area (U+F000–U+FFFF), the same range used by LVGL's own
// LV_SYMBOL_* macros.
//
// Use these symbols with &lv_font_icons_20, not with lv_font_montserrat_*.

// ---- Icons new in this custom font (not available in LVGL built-ins) --------
#define UI_ICON_LIGHTBULB    "\xEF\x83\xAB"  // U+F0EB  fa-lightbulb
#define UI_ICON_THERMOMETER  "\xEF\x8B\x89"  // U+F2C9  fa-thermometer-half
#define UI_ICON_LOCK         "\xEF\x80\xA3"  // U+F023  fa-lock
#define UI_ICON_LOCK_OPEN    "\xEF\x8F\x81"  // U+F3C1  fa-lock-open
#define UI_ICON_GRIP_LINES   "\xEF\x96\xA0"  // U+F5A0  fa-grip-lines  (blinds/cover)
#define UI_ICON_FAN          "\xEF\xA1\xA3"  // U+F863  fa-fan

// ---- Icons that coincide with LVGL LV_SYMBOL_* codepoints ------------------
// Using this font for all domains keeps rendering consistent.
#define UI_ICON_MUSIC        "\xEF\x80\x81"  // U+F001  fa-music         (media player)
#define UI_ICON_POWER        "\xEF\x80\x91"  // U+F011  fa-power-off     (switch / input_boolean)
#define UI_ICON_COG          "\xEF\x80\x93"  // U+F013  fa-cog           (fallback)
#define UI_ICON_HOME         "\xEF\x80\x95"  // U+F015  fa-home          (unknown domain)
#define UI_ICON_IMAGE        "\xEF\x80\xBE"  // U+F03E  fa-image         (scene)
#define UI_ICON_PLAY         "\xEF\x81\x8B"  // U+F04B  fa-play          (script)
#define UI_ICON_EYE          "\xEF\x81\xAE"  // U+F06E  fa-eye           (sensor / binary sensor)
#define UI_ICON_REPEAT       "\xEF\x81\xB9"  // U+F079  fa-repeat        (automation)
#define UI_ICON_BOLT         "\xEF\x83\xA7"  // U+F0E7  fa-bolt

// ---- Weather icons (TICKET-012a) --------------------------------------------
// These codepoints require regenerating lv_font_icons_20.c — see human_to_do.md.
// Until regenerated they render as blank glyphs (no crash, just invisible icon).
#define UI_ICON_SUN          "\xEF\x86\x85"  // U+F185  fa-sun
#define UI_ICON_MOON         "\xEF\x86\x86"  // U+F186  fa-moon
#define UI_ICON_CLOUD        "\xEF\x83\x82"  // U+F0C2  fa-cloud
#define UI_ICON_CLOUD_SUN    "\xEF\x9B\x84"  // U+F6C4  fa-cloud-sun
#define UI_ICON_CLOUD_RAIN   "\xEF\x9C\xBD"  // U+F73D  fa-cloud-rain
#define UI_ICON_CLOUD_SHOWERS "\xEF\x9D\x80" // U+F740  fa-cloud-showers-heavy
#define UI_ICON_SNOWFLAKE    "\xEF\x8B\x9C"  // U+F2DC  fa-snowflake
#define UI_ICON_WIND         "\xEF\x9C\xAE"  // U+F72E  fa-wind
#define UI_ICON_SMOG         "\xEF\x9D\x9F"  // U+F75F  fa-smog

// External declarations for the compiled fonts.
// lv_font_icons_20 definition is in lv_font_icons_20.c.
#if defined(__cplusplus)
extern "C" {
#endif
extern const lv_font_t lv_font_icons_20;
#if defined(__cplusplus)
}
#endif
