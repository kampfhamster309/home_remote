/**
 * LVGL v8 configuration for ESP32-2432S028 (CYD)
 * ILI9341 320x240 display, RGB565 color depth
 */

#if 1  /* must stay 1 — LVGL checks this guard */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/

/* 16-bit RGB565 to match ILI9341 */
#define LV_COLOR_DEPTH 16

/* Swap the bytes of RGB565 colors.
 * Set to 1 because TFT_eSPI pushColors() is called with swap=true,
 * resulting in correct byte order on the SPI bus.
 * If colors look wrong on the display, toggle this value. */
#define LV_COLOR_16_SWAP 0

/* Enable transparency support (needed for anti-aliased fonts) */
#define LV_COLOR_SCREEN_TRANSP 0

/* Adjust color mix functions rounding (0: round down, 64: round up) */
#define LV_COLOR_MIX_ROUND_OFS 0

/* Images with the same colors as the display format are stored in the
 * following format (may improve rendering speed) */
#define LV_IMG_CF_INDEXED 0

/*====================
   MEMORY SETTINGS
 *====================*/

/* Use LVGL's built-in allocator */
#define LV_MEM_CUSTOM 0

/* Internal LVGL heap size. 48 KB is comfortable for this UI. */
#define LV_MEM_SIZE (48U * 1024U)

/* Set an address for the memory pool instead of allocating it as a
 * global array. Can be in external SRAM too. 0: unused */
#define LV_MEM_ADR 0

/* Complier prefix for large array declaration */
#define LV_MEM_ATTR

/* Set LV_MEM_POOL_INCLUDE and LV_MEM_POOL_ALLOC/FREE to use a custom
 * memory pool. (disabled) */

/*====================
   HAL SETTINGS
 *====================*/

/* Default display refresh period in ms */
#define LV_DISP_DEF_REFR_PERIOD 10

/* Input device read period in ms */
#define LV_INDEV_DEF_READ_PERIOD 30

/* Use custom tick source (millis() from Arduino) */
#define LV_TICK_CUSTOM 1
#if LV_TICK_CUSTOM
    #define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
    #define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())
#endif

/* Default DPI. Used to initialize default sizes. */
#define LV_DPI_DEF 130

/*====================
   DRAWING
 *====================*/

/* Enable complex draw engine (shadows, gradients, rounded corners) */
#define LV_DRAW_COMPLEX 1
#if LV_DRAW_COMPLEX != 0
    /* Allow buffering some shadow calculation results */
    #define LV_SHADOW_CACHE_SIZE 0
    /* Set number of maximally cached circle data.
     * 4 per unique radius; 0 disables caching */
    #define LV_CIRCLE_CACHE_SIZE 4
#endif

/* Maximum buffer size for gradient pre-rendering (bytes) */
#define LV_GRAD_CACHE_DEF_SIZE 0

/* Dithering algorithm (0: none, 1: ordered) */
#define LV_DITHER_GRADIENT 0

/* Allow buffering the blur calculation */
#define LV_DISP_ROT_MAX_BUF (10 * 1024)

/*====================
   GPU
 *====================*/

/* No GPU on CYD */
#define LV_USE_GPU_STM32_DMA2D 0
#define LV_USE_GPU_SWM341_DMA  0
#define LV_USE_GPU_NXP_PXP     0
#define LV_USE_GPU_NXP_VG_LITE 0
#define LV_USE_GPU_SDL         0

/*====================
   LOGGING
 *====================*/

/* Disable to save flash space in production */
#define LV_USE_LOG 0
#if LV_USE_LOG
    #define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
    #define LV_LOG_PRINTF 0
    #define LV_LOG_USE_TIMESTAMP 1
    #define LV_LOG_USE_FILE_LINE 1
#endif

/*====================
   ASSERTS
 *====================*/

#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0

#define LV_ASSERT_HANDLER_INCLUDE <stdint.h>
#define LV_ASSERT_HANDLER while(1);

/*====================
   DEBUG TOOLS
 *====================*/

/* Disable performance/memory overlays in production */
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR  0
#define LV_USE_REFR_DEBUG   0

/*====================
   OTHERS
 *====================*/

#define LV_USE_MATH_NATURAL 0
#define LV_SPRINTF_CUSTOM   0
#define LV_SPRINTF_USE_FLOAT 0

#define LV_USE_USER_DATA    1

/* Garbage collector settings (only for dynamic languages; unused here) */
#define LV_ENABLE_GC 0

/* Default image cache size (entries). 0 disables caching. */
#define LV_IMG_CACHE_DEF_SIZE 0

/* Number of stops allowed per gradient (must be ≥ 2) */
#define LV_GRADIENT_MAX_STOPS 2

/* Adjust G0 and G255 to compensate for display gamma non-linearity */
#define LV_COLOR_FILTER_DEF_OPA LV_OPA_TRANSP

/* Max. number of custom cursors */
#define LV_INDEV_DEF_SCROLL_LIMIT     10
#define LV_INDEV_DEF_SCROLL_THROW     10
#define LV_INDEV_DEF_LONG_PRESS_TIME  400
#define LV_INDEV_DEF_LONG_PRESS_REP_TIME 100
#define LV_INDEV_DEF_GESTURE_LIMIT    50
#define LV_INDEV_DEF_GESTURE_MIN_VELOCITY 3

/*====================
   FONTS
 *====================*/

/* Enable Montserrat fonts (vector, anti-aliased) */
#define LV_FONT_MONTSERRAT_12  0
#define LV_FONT_MONTSERRAT_14  0  /* replaced by src/ui/lv_font_montserrat_14.c (+ DE umlauts) */
#define LV_FONT_MONTSERRAT_16  0  /* replaced by src/ui/lv_font_montserrat_16.c (+ DE umlauts) */
#define LV_FONT_MONTSERRAT_18  0
#define LV_FONT_MONTSERRAT_20  1
#define LV_FONT_MONTSERRAT_22  0
#define LV_FONT_MONTSERRAT_24  1
#define LV_FONT_MONTSERRAT_26  0
#define LV_FONT_MONTSERRAT_28  0
#define LV_FONT_MONTSERRAT_30  0
#define LV_FONT_MONTSERRAT_32  0
#define LV_FONT_MONTSERRAT_34  0
#define LV_FONT_MONTSERRAT_36  0
#define LV_FONT_MONTSERRAT_38  0
#define LV_FONT_MONTSERRAT_40  0
#define LV_FONT_MONTSERRAT_42  0
#define LV_FONT_MONTSERRAT_44  0
#define LV_FONT_MONTSERRAT_46  0
#define LV_FONT_MONTSERRAT_48  0

/* Built-in bitmap font (fallback) */
#define LV_FONT_UNSCII_8   0
#define LV_FONT_UNSCII_16  0

/* Built-in special fonts */
#define LV_FONT_MONTSERRAT_12_SUBPX      0
#define LV_FONT_MONTSERRAT_28_COMPRESSED 0
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0
#define LV_FONT_SIMSUN_16_CJK            0

/* Enable built-in symbols (always on for icon support) */
#define LV_USE_FONT_COMPRESSED 1
#define LV_USE_FONT_SUBPX      0

/* Default font used by widgets */
#define LV_FONT_DEFAULT &lv_font_montserrat_20  /* 14+16 replaced by custom DE builds */

/* Kerning — disable to save flash */
#define LV_USE_FONT_SUBPX 0
#define LV_FONT_SUBPX_BGR 0

/*====================
   TEXT SETTINGS
 *====================*/

#define LV_TXT_ENC LV_TXT_ENC_UTF8
#define LV_TXT_BREAK_CHARS " ,.;:-_"
#define LV_TXT_LINE_BREAK_LONG_LEN 0
#define LV_TXT_LINE_BREAK_LONG_PRE_MIN_LEN 3
#define LV_TXT_LINE_BREAK_LONG_POST_MIN_LEN 3
#define LV_TXT_COLOR_CMD "#"
#define LV_USE_BIDI         0
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/*====================
   WIDGETS
 *====================*/

#define LV_USE_ARC        1
#define LV_USE_BAR        1
#define LV_USE_BTN        1
#define LV_USE_BTNMATRIX  1
#define LV_USE_CANVAS     0
#define LV_USE_CHECKBOX   1
#define LV_USE_DROPDOWN   1
#define LV_USE_IMG        1
#define LV_USE_LABEL      1
#  define LV_LABEL_TEXT_SELECTION 1
#  define LV_LABEL_LONG_TXT_HINT  1
#define LV_USE_LINE       1
#define LV_USE_ROLLER     1
#  define LV_ROLLER_INF_PAGES 7
#define LV_USE_SLIDER     1
#define LV_USE_SWITCH     1
#define LV_USE_TEXTAREA   1
#  define LV_TEXTAREA_DEF_PWD_SHOW_TIME 1500
#define LV_USE_TABLE      0

/*====================
   EXTRA WIDGETS
 *====================*/

#define LV_USE_ANIMIMG    0
#define LV_USE_CALENDAR   0
#define LV_USE_CHART      0
#define LV_USE_COLORWHEEL 0
#define LV_USE_IMGBTN     0
#define LV_USE_KEYBOARD   1  /* needed for captive portal text input */
#define LV_USE_LED        1
#define LV_USE_LIST       1
#define LV_USE_MENU       1
#define LV_USE_METER      0
#define LV_USE_MSGBOX     1
#define LV_USE_SPINBOX    0
#define LV_USE_SPINNER    1
#define LV_USE_TABVIEW    1
#define LV_USE_TILEVIEW   1
#define LV_USE_WIN        0
#define LV_USE_SPAN       0

/*====================
   THEMES
 *====================*/

#define LV_USE_THEME_DEFAULT 1
#if LV_USE_THEME_DEFAULT
    #define LV_THEME_DEFAULT_DARK       0   /* 0=light, 1=dark (toggled at runtime) */
    #define LV_THEME_DEFAULT_GROW       1
    #define LV_THEME_DEFAULT_TRANSITION_TIME 80
#endif

#define LV_USE_THEME_BASIC 1
#define LV_USE_THEME_MONO  0

/*====================
   LAYOUTS
 *====================*/

#define LV_USE_FLEX 1
#define LV_USE_GRID 1

/*====================
   GPU (FILE SYSTEM)
 *====================*/

/* No filesystem — all assets compiled into firmware */
#define LV_USE_FS_STDIO   0
#define LV_USE_FS_POSIX   0
#define LV_USE_FS_WIN32   0
#define LV_USE_FS_FATFS   0

/*====================
   PNG / BMP / JPG
 *====================*/

#define LV_USE_PNG  0
#define LV_USE_BMP  0
#define LV_USE_SJPG 0
#define LV_USE_GIF  0
#define LV_USE_QRCODE 0

/*====================
   PROFILING
 *====================*/

#define LV_USE_PROFILER 0

/*====================
   EXAMPLES
 *====================*/

#define LV_BUILD_EXAMPLES 0

#endif /* LV_CONF_H */
#endif /* End of "Content enable" */
