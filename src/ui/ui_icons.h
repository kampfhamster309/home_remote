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

// External declaration for the compiled font.
// Definition is in lv_font_icons_20.c.
#if defined(__cplusplus)
extern "C" {
#endif
extern const lv_font_t lv_font_icons_20;
#if defined(__cplusplus)
}
#endif
