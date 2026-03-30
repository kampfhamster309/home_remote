#pragma once

// ----------------------------------------------------------------------------
// Screen geometry
// ----------------------------------------------------------------------------
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// LVGL draw buffer height (lines). Two buffers of this size are allocated.
// 320 * 10 * 2 bytes = 6 400 bytes per buffer (12 800 total).
// Reduced from 20 to keep BSS within ESP32 DRAM limits.
#define LVGL_BUFFER_LINES 10

// ----------------------------------------------------------------------------
// Display SPI pins (ST7789) — HSPI bus (native pins)
// ----------------------------------------------------------------------------
#define TFT_PIN_MOSI 13
#define TFT_PIN_SCLK 14
#define TFT_PIN_CS   15
#define TFT_PIN_DC    2
#define TFT_PIN_RST  -1
#define TFT_PIN_BL   21   // Backlight (active HIGH)

// ----------------------------------------------------------------------------
// Touch SPI pins (XPT2046) — HSPI bus (separate from display)
// ----------------------------------------------------------------------------
#define TOUCH_PIN_CLK  25
#define TOUCH_PIN_MISO 39
#define TOUCH_PIN_MOSI 32
#define TOUCH_PIN_CS   33
#define TOUCH_PIN_IRQ  36
