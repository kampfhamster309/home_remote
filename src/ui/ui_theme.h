#pragma once

// ----------------------------------------------------------------------------
// Color palette — dark theme
// Usage:  lv_obj_set_style_bg_color(obj, lv_color_hex(UI_COL_BG), LV_PART_MAIN);
// ----------------------------------------------------------------------------

#define UI_COL_BG         0x0D0D1A   // page background
#define UI_COL_SURFACE    0x1A1A2E   // card / panel surface
#define UI_COL_HEADER     0x0F0F22   // header bar
#define UI_COL_NAV        0x0F0F22   // nav-bar background
#define UI_COL_NAV_ACTIVE 0x222244   // active tab background
#define UI_COL_ACCENT     0x4F8EF7   // blue accent / active indicator
#define UI_COL_TEXT       0xE0E0E0   // primary text
#define UI_COL_TEXT_DIM   0x606080   // secondary / hint text
#define UI_COL_OK         0x44AA44   // green — connected / on
#define UI_COL_ERR        0xCC4444   // red — disconnected / error
#define UI_COL_BORDER     0x222244   // subtle separator lines

// ----------------------------------------------------------------------------
// Layout constants (pixels, 320 × 240 landscape)
// ----------------------------------------------------------------------------

#define UI_HEADER_H    36   // header bar height (display only, not a touch target)
#define UI_NAV_H       44   // nav-bar height (touch target ≥ 44 px per CLAUDE.md)
#define UI_CONTENT_H   160  // SCREEN_HEIGHT − UI_HEADER_H − UI_NAV_H

#define UI_NAV_TAB_W   76   // each nav tab button width when > 4 groups
#define UI_DOT_SIZE    10   // status indicator dot diameter
