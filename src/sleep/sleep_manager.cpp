#include "sleep_manager.h"

#include <Arduino.h>
#include <lvgl.h>
#include <esp_sleep.h>
#include <driver/gpio.h>

#include "display_config.h"
#include "config/nvs_config.h"

// ----------------------------------------------------------------------------
// Module state
// ----------------------------------------------------------------------------

namespace {

static bool     s_battery_mode = false;
static uint16_t s_timeout_s    = 30;   // default: 30 s

static void do_sleep()
{
    // Capture current backlight duty so we can restore it exactly on wake.
    const uint32_t brightness = ledcRead(BL_LEDC_CHANNEL);

    // Blank the display before sleeping.
    ledcWrite(BL_LEDC_CHANNEL, 0);

    // Configure GPIO wakeup on touch IRQ (XPT2046 pulls the line LOW on touch).
    gpio_wakeup_enable(static_cast<gpio_num_t>(TOUCH_PIN_IRQ),
                       GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();

    Serial.println("[sleep] entering light sleep");
    Serial.flush();  // drain UART buffer before the CPU halts

    esp_light_sleep_start();

    // ---- Execution resumes here after touch wakes the device ----------------

    // millis() is backed by esp_timer which continues through light sleep, so
    // LVGL's tick is accurate.  However, lv_disp_get_inactive_time() would now
    // return the full sleep duration and immediately re-trigger sleep on the
    // next tick() call.  Reset it so the inactivity window starts fresh.
    lv_disp_trig_activity(nullptr);

    // Restore backlight.
    ledcWrite(BL_LEDC_CHANNEL, brightness);

    Serial.println("[sleep] woke from light sleep");
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

namespace sleep_manager {

void init()
{
    MobileSettings ms{ false, 30 };
    if (nvs_config::load_mobile_settings(ms)) {
        s_battery_mode = ms.battery_mode;
        s_timeout_s    = ms.sleep_timeout_s;
    }
    Serial.printf("[sleep] battery_mode=%s timeout=%us\n",
                  s_battery_mode ? "on" : "off", s_timeout_s);
}

void tick()
{
    if (!s_battery_mode) return;

    if (lv_disp_get_inactive_time(nullptr) >=
            static_cast<uint32_t>(s_timeout_s) * 1000UL) {
        do_sleep();
    }
}

bool is_battery_mode() { return s_battery_mode; }

void set_battery_mode(bool enabled)
{
    s_battery_mode = enabled;
    nvs_config::save_mobile_settings({ enabled, s_timeout_s });
    Serial.printf("[sleep] battery_mode → %s\n", enabled ? "on" : "off");
}

uint16_t get_timeout_s() { return s_timeout_s; }

void set_timeout_s(uint16_t seconds)
{
    s_timeout_s = seconds;
    nvs_config::save_mobile_settings({ s_battery_mode, seconds });
    Serial.printf("[sleep] sleep_timeout → %us\n", seconds);
}

} // namespace sleep_manager
