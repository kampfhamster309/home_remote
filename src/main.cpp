#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>

#include "display_config.h"

// ----------------------------------------------------------------------------
// Globals
// ----------------------------------------------------------------------------

static TFT_eSPI tft;

// Touch uses HSPI — a separate SPI bus from the display (VSPI)
static SPIClass touchSpi(HSPI);
static XPT2046_Touchscreen touch(TOUCH_PIN_CS, TOUCH_PIN_IRQ);

// LVGL draw buffers (double-buffered)
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[SCREEN_WIDTH * LVGL_BUFFER_LINES];
static lv_color_t buf2[SCREEN_WIDTH * LVGL_BUFFER_LINES];

// ----------------------------------------------------------------------------
// LVGL display flush callback
// ----------------------------------------------------------------------------

static void lvgl_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    const uint32_t w = area->x2 - area->x1 + 1;
    const uint32_t h = area->y2 - area->y1 + 1;

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors(reinterpret_cast<uint16_t *>(color_p), w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp);
}

// ----------------------------------------------------------------------------
// Display initialisation
// ----------------------------------------------------------------------------

static void display_init()
{
    tft.init();
    tft.setRotation(1);  // landscape, USB-C on the right

    // Enable backlight
    pinMode(TFT_PIN_BL, OUTPUT);
    digitalWrite(TFT_PIN_BL, HIGH);

    tft.fillScreen(TFT_BLACK);

    Serial.println("[display] ILI9341 initialised (320x240, landscape)");
}

// ----------------------------------------------------------------------------
// Touch initialisation
// ----------------------------------------------------------------------------

static void touch_init()
{
    touchSpi.begin(TOUCH_PIN_CLK, TOUCH_PIN_MISO, TOUCH_PIN_MOSI, TOUCH_PIN_CS);
    touch.begin(touchSpi);

    Serial.println("[touch] XPT2046 initialised on HSPI");
}

// ----------------------------------------------------------------------------
// LVGL initialisation
// ----------------------------------------------------------------------------

static void lvgl_init()
{
    lv_init();

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, SCREEN_WIDTH * LVGL_BUFFER_LINES);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = SCREEN_WIDTH;
    disp_drv.ver_res  = SCREEN_HEIGHT;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    Serial.println("[lvgl] initialised");
}

// ----------------------------------------------------------------------------
// Hello World screen
// ----------------------------------------------------------------------------

static void create_hello_screen()
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1A1A2E), LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text(label, "Home Remote\nHello World!");
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t *hint = lv_label_create(screen);
    lv_label_set_text(hint, "Touch screen to test input");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 20);
}

// ----------------------------------------------------------------------------
// Arduino entry points
// ----------------------------------------------------------------------------

void setup()
{
    Serial.begin(115200);
    Serial.println("\n[boot] Home Remote starting...");

    display_init();
    touch_init();
    lvgl_init();
    create_hello_screen();

    Serial.println("[boot] Ready. Touch the screen to see coordinates.");
}

void loop()
{
    lv_timer_handler();  // run LVGL tasks (~5 ms budget)

    // Raw touch read — printed to serial for TICKET-001 acceptance verification.
    // Will be replaced by the LVGL input driver in TICKET-002.
    if (touch.tirqTouched() && touch.touched()) {
        TS_Point p = touch.getPoint();
        Serial.printf("[touch] raw x=%-5d y=%-5d z=%d\n", p.x, p.y, p.z);
    }

    delay(5);
}
