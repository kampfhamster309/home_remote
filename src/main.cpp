#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <lvgl.h>

#include "display_config.h"
#include "touch/touch_driver.h"
#include "wifi/wifi_manager.h"
#include "ha/ha_client.h"

// ----------------------------------------------------------------------------
// Globals
// ----------------------------------------------------------------------------

static TFT_eSPI tft;

// LVGL draw buffers (double-buffered)
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[SCREEN_WIDTH * LVGL_BUFFER_LINES];
static lv_color_t buf2[SCREEN_WIDTH * LVGL_BUFFER_LINES];

// ----------------------------------------------------------------------------
// Home Assistant callbacks (stubs — entity model added in TICKET-005)
// ----------------------------------------------------------------------------

static void on_ha_states(const JsonArray& states)
{
    Serial.printf("[ha] got %u initial states\n",
                  static_cast<unsigned>(states.size()));
}

static void on_ha_state_changed(const char* entity_id,
                                const JsonObject& /*new_state*/)
{
    Serial.printf("[ha] state_changed: %s\n", entity_id);
}

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

    pinMode(TFT_PIN_BL, OUTPUT);
    digitalWrite(TFT_PIN_BL, HIGH);

    tft.fillScreen(TFT_BLACK);

    Serial.println("[display] ILI9341 initialised (320x240, landscape)");
}

// ----------------------------------------------------------------------------
// LVGL initialisation (display driver only — input driver added later)
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
// Main screen
// Long-pressing anywhere re-triggers touch calibration.
// This screen will be replaced by the full UI in later tickets.
// ----------------------------------------------------------------------------

static void on_long_press(lv_event_t* /*e*/)
{
    Serial.println("[ui] Long-press — recalibrating touch");
    touch_driver::run_calibration();
}

static void create_main_screen()
{
    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1A1A2E), LV_PART_MAIN);

    // Invisible full-screen overlay for long-press recalibration
    lv_obj_t* overlay = lv_obj_create(scr);
    lv_obj_set_size(overlay, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(overlay, 0, LV_PART_MAIN);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(overlay, on_long_press, LV_EVENT_LONG_PRESSED, nullptr);

    lv_obj_t* title = lv_label_create(scr);
    lv_label_set_text(title, "Home Remote");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -30);

    // Show WiFi IP if connected
    if (wifi_manager::is_connected()) {
        char ip_line[40];
        snprintf(ip_line, sizeof(ip_line), "%s", WiFi.localIP().toString().c_str());

        lv_obj_t* ip = lv_label_create(scr);
        lv_label_set_text(ip, ip_line);
        lv_obj_set_style_text_color(ip, lv_color_hex(0x44AA44), LV_PART_MAIN);
        lv_obj_set_style_text_font(ip, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_align(ip, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_align(ip, LV_ALIGN_CENTER, 0, 0);
    }

    lv_obj_t* hint = lv_label_create(scr);
    lv_label_set_text(hint, "Hold to recalibrate touch");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x444466), LV_PART_MAIN);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 30);

    lv_scr_load(scr);
}

// ----------------------------------------------------------------------------
// Arduino entry points
// ----------------------------------------------------------------------------

void setup()
{
    Serial.begin(115200);
    Serial.println("\n[boot] Home Remote starting...");
    Serial.printf("[boot] Free heap: %u bytes\n", ESP.getFreeHeap());

    display_init();
    touch_driver::init();
    lvgl_init();

    if (!touch_driver::is_calibrated()) {
        touch_driver::run_calibration();
    }

    touch_driver::register_lvgl_indev();

    wifi_manager::init();

    if (!wifi_manager::has_config()) {
        wifi_manager::start_portal();  // does not return — ends with reboot
    }

    wifi_manager::connect();

    ha_client::init(on_ha_states, on_ha_state_changed);

    create_main_screen();

    Serial.printf("[boot] Ready. Free heap: %u bytes\n", ESP.getFreeHeap());
}

void loop()
{
    lv_timer_handler();
    wifi_manager::tick();
    ha_client::tick();
    delay(5);
}
