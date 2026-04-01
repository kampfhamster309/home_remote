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
// Backlight PWM (ESP32 LEDC peripheral, channel dedicated to TFT_PIN_BL)
// ----------------------------------------------------------------------------
#define BL_LEDC_CHANNEL  7     // channel 7 — free of TFT_eSPI / audio conflicts
#define BL_LEDC_FREQ_HZ  5000  // 5 kHz — above audible range for any coil whine
#define BL_LEDC_BITS     8     // 0–255 duty cycle

// ----------------------------------------------------------------------------
// Touch SPI pins (XPT2046) — HSPI bus (separate from display)
// ----------------------------------------------------------------------------
#define TOUCH_PIN_CLK  25
#define TOUCH_PIN_MISO 39
#define TOUCH_PIN_MOSI 32
#define TOUCH_PIN_CS   33
#define TOUCH_PIN_IRQ  36
