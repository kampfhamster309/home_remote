#pragma once

// ----------------------------------------------------------------------------
// Screen geometry
// ----------------------------------------------------------------------------
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// LVGL draw buffer height (lines). Two buffers of this size are allocated.
// 320 * 20 * 2 bytes = 12 800 bytes per buffer.
#define LVGL_BUFFER_LINES 20

// ----------------------------------------------------------------------------
// Display SPI pins (ILI9341) — VSPI bus
// ----------------------------------------------------------------------------
#define TFT_PIN_MOSI 13
#define TFT_PIN_SCLK 14
#define TFT_PIN_CS   15
#define TFT_PIN_DC    2
#define TFT_PIN_RST  12
#define TFT_PIN_BL   21   // Backlight (active HIGH)

// ----------------------------------------------------------------------------
// Touch SPI pins (XPT2046) — HSPI bus (separate from display)
// ----------------------------------------------------------------------------
#define TOUCH_PIN_CLK  25
#define TOUCH_PIN_MISO 39
#define TOUCH_PIN_MOSI 32
#define TOUCH_PIN_CS   33
#define TOUCH_PIN_IRQ  36
