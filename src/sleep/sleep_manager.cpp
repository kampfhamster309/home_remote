#include "sleep_manager.h"

#include <Arduino.h>
#include <lvgl.h>
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>

#include "display_config.h"
#include "config/nvs_config.h"
#include "touch/touch_driver.h"

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

    // Enable WiFi modem sleep so the radio stays associated with the AP while
    // the CPU is halted.  Without this, the WiFi radio can power off entirely
    // during light sleep and the AP will deauthenticate the client after its
    // idle timeout (~30–60 s on most home routers), forcing a full re-association
    // on wake.  WIFI_PS_MIN_MODEM keeps the radio active at DTIM beacon
    // intervals (typically every 100 ms) so the AP never drops the client.
    // Restored to WIFI_PS_NONE after wake so the HA WebSocket operates without
    // the ~100 ms modem-sleep latency while the user is actively using the device.
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

    // Configure GPIO wakeup on touch IRQ (XPT2046 pulls the line LOW on touch).
    gpio_wakeup_enable(static_cast<gpio_num_t>(TOUCH_PIN_IRQ),
                       GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();

    Serial.println("[sleep] entering light sleep");
    Serial.flush();  // drain UART buffer before the CPU halts

    esp_light_sleep_start();

    // ---- Execution resumes here after touch wakes the device ----------------

    // gpio_wakeup_enable() for GPIO 36 internally calls rtc_gpio_init(), which
    // routes the pin through the RTC GPIO controller instead of the normal GPIO
    // matrix.  This disconnects the pin from the main GPIO interrupt controller
    // that attachInterrupt(FALLING) registered on.  The FALLING ISR (isrPin) can
    // no longer fire, so XPT2046_Touchscreen::tirqTouched() always returns false
    // (isrWake is never set), and update() returns immediately — touch is dead.
    // gpio_wakeup_disable() clears the wakeup trigger but does NOT call
    // rtc_gpio_deinit(), so the pin remains under RTC control.
    // rtc_gpio_deinit() restores the GPIO matrix routing, and touch_driver::on_wake()
    // re-attaches the FALLING ISR and primes isrWake for the first post-wake read.
    gpio_wakeup_disable(static_cast<gpio_num_t>(TOUCH_PIN_IRQ));
    rtc_gpio_deinit(static_cast<gpio_num_t>(TOUCH_PIN_IRQ));
    touch_driver::on_wake();

    // Restore full radio-on mode so the HA WebSocket receives events with
    // minimal latency while the user is interacting with the device.
    esp_wifi_set_ps(WIFI_PS_NONE);

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
