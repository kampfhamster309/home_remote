#include "touch_driver.h"

#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>

#include "display_config.h"
#include "touch_cal.h"
#include "../config/nvs_config.h"

// ----------------------------------------------------------------------------
// Module-private state
// ----------------------------------------------------------------------------

static SPIClass          s_spi(VSPI);  // HSPI is used by TFT_eSPI for the display
static XPT2046_Touchscreen s_touch(TOUCH_PIN_CS, TOUCH_PIN_IRQ);

static TouchCalibration  s_cal{};
static bool              s_calibrated = false;
static lv_indev_t*       s_indev      = nullptr;

// ----------------------------------------------------------------------------
// LVGL input driver callback
// Called by lv_timer_handler() on every indev read cycle.
// ----------------------------------------------------------------------------

static void lvgl_touch_read_cb(lv_indev_drv_t* /*drv*/, lv_indev_data_t* data)
{
    if (s_touch.tirqTouched() && s_touch.touched()) {
        TS_Point p = s_touch.getPoint();
        data->point.x = touch_map_x(static_cast<int16_t>(p.x), s_cal);
        data->point.y = touch_map_y(static_cast<int16_t>(p.y), s_cal);
        data->state   = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// ----------------------------------------------------------------------------
// Calibration helpers
// ----------------------------------------------------------------------------

// Collect CAL_SAMPLES raw readings while the screen is held, then average.
// Returns true when enough stable samples were gathered.
static bool collect_point(int16_t& out_x, int16_t& out_y)
{
    // Wait for initial touch
    while (!s_touch.tirqTouched() || !s_touch.touched()) {
        lv_timer_handler();
        delay(10);
    }

    int32_t sum_x = 0, sum_y = 0;
    int     n     = 0;

    const unsigned long deadline = millis() + 800UL;
    while (millis() < deadline) {
        if (s_touch.touched()) {
            TS_Point p = s_touch.getPoint();
            sum_x += p.x;
            sum_y += p.y;
            ++n;
        }
        lv_timer_handler();
        delay(20);
    }

    if (n < 5) return false;

    out_x = static_cast<int16_t>(sum_x / n);
    out_y = static_cast<int16_t>(sum_y / n);

    // Wait for release + debounce
    while (s_touch.tirqTouched() && s_touch.touched()) {
        lv_timer_handler();
        delay(10);
    }
    delay(300);

    return true;
}

// Draw (or update) the calibration target circle on `parent`.
// Returns the created circle object so the caller can reposition it.
static lv_obj_t* make_target(lv_obj_t* parent, int16_t x, int16_t y, bool highlight)
{
    static constexpr int16_t RADIUS = 12;

    lv_obj_t* circle = lv_obj_create(parent);
    lv_obj_set_size(circle, RADIUS * 2, RADIUS * 2);
    lv_obj_set_pos(circle, x - RADIUS, y - RADIUS);
    lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(
        circle,
        highlight ? lv_color_hex(0x00DD00) : lv_color_hex(0xFFDD00),
        LV_PART_MAIN);
    lv_obj_set_style_border_width(circle, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(circle, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_clear_flag(circle, LV_OBJ_FLAG_SCROLLABLE);
    return circle;
}

static lv_obj_t* make_cal_screen()
{
    lv_obj_t* screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x0D0D1A), LV_PART_MAIN);
    lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "Touch Calibration");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    return screen;
}

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

namespace touch_driver {

void init()
{
    s_spi.begin(TOUCH_PIN_CLK, TOUCH_PIN_MISO, TOUCH_PIN_MOSI, TOUCH_PIN_CS);
    s_touch.begin(s_spi);

    s_calibrated = nvs_config::load_touch_cal(s_cal);

    Serial.printf("[touch] XPT2046 init — calibration %s\n",
                  s_calibrated ? "loaded from NVS" : "NOT found, will calibrate");
}

bool is_calibrated()
{
    return s_calibrated;
}

void run_calibration()
{
    Serial.println("[touch] Starting calibration...");

    // Suspend LVGL indev so it doesn't race against the raw reads below
    if (s_indev) lv_indev_enable(s_indev, false);

    // ---- Build calibration screen ----------------------------------------
    lv_obj_t* cal_screen = make_cal_screen();

    lv_obj_t* instr = lv_label_create(cal_screen);
    lv_obj_set_style_text_color(instr, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
    lv_obj_set_style_text_font(instr, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_align(instr, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(instr, SCREEN_WIDTH);
    lv_obj_align(instr, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_scr_load(cal_screen);

    // ---- Point 1 ------------------------------------------------------------
    lv_label_set_text(instr, "Tap the target  (1 / 2)");
    lv_obj_t* t1 = make_target(cal_screen, CAL_SCREEN_X1, CAL_SCREEN_Y1, false);
    lv_timer_handler();

    int16_t x1r = 0, y1r = 0;
    while (!collect_point(x1r, y1r)) {
        Serial.println("[touch] Calibration point 1: not enough samples, retrying");
    }
    Serial.printf("[touch] Cal point 1: raw x=%d y=%d\n", x1r, y1r);

    // Flash green to confirm
    lv_obj_set_style_bg_color(t1, lv_color_hex(0x00DD00), LV_PART_MAIN);
    lv_timer_handler();
    delay(400);

    // ---- Point 2 ------------------------------------------------------------
    lv_label_set_text(instr, "Tap the target  (2 / 2)");
    lv_obj_del(t1);
    lv_obj_t* t2 = make_target(cal_screen, CAL_SCREEN_X2, CAL_SCREEN_Y2, false);
    lv_timer_handler();

    int16_t x2r = 0, y2r = 0;
    while (!collect_point(x2r, y2r)) {
        Serial.println("[touch] Calibration point 2: not enough samples, retrying");
    }
    Serial.printf("[touch] Cal point 2: raw x=%d y=%d\n", x2r, y2r);

    // Flash green to confirm
    lv_obj_set_style_bg_color(t2, lv_color_hex(0x00DD00), LV_PART_MAIN);
    lv_timer_handler();
    delay(400);

    // ---- Save ---------------------------------------------------------------
    s_cal = {x1r, y1r, x2r, y2r};
    s_calibrated = true;
    nvs_config::save_touch_cal(s_cal);

    // Done banner
    lv_label_set_text(instr, "Calibration saved!");
    lv_obj_set_style_text_color(instr, lv_color_hex(0x00DD00), LV_PART_MAIN);
    lv_timer_handler();
    delay(1200);

    Serial.println("[touch] Calibration complete.");

    // ---- Tear down calibration screen --------------------------------------
    lv_obj_t* blank = lv_obj_create(nullptr);
    lv_scr_load(blank);
    lv_obj_del(cal_screen);

    // Re-enable indev if it was registered
    if (s_indev) lv_indev_enable(s_indev, true);
}

void register_lvgl_indev()
{
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_read_cb;
    s_indev = lv_indev_drv_register(&indev_drv);
    Serial.println("[touch] LVGL input device registered");
}

lv_indev_t* get_indev()
{
    return s_indev;
}

}  // namespace touch_driver
