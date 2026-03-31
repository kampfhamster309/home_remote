#pragma once
// Forward-declarations for the custom Montserrat font builds that add German
// umlaut support (ASCII 0x20–0x7F + ä ö ü Ä Ö Ü ß).
// These are defined in src/ui/lv_font_montserrat_14.c / _16.c.
// The LVGL built-ins for size 14 and 16 are disabled (LV_FONT_MONTSERRAT_14/16=0).

#include <lvgl.h>

#if defined(__cplusplus)
extern "C" {
#endif

extern const lv_font_t lv_font_montserrat_14;
extern const lv_font_t lv_font_montserrat_16;

#if defined(__cplusplus)
}
#endif
