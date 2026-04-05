#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <WiFi.h>

#include "display_config.h"
#include "config/nvs_config.h"
#include "touch/touch_driver.h"
#include "wifi/wifi_manager.h"
#include "ha/ha_client.h"
#include "ha/entity_cache.h"
#include "ha/area_cache.h"
#include "ha/weather_cache.h"
#include "nb/nb_client.h"
#include "ui/shell.h"
#include "i18n/i18n.h"

// ----------------------------------------------------------------------------
// Globals
// ----------------------------------------------------------------------------

static TFT_eSPI tft;

// LVGL draw buffers (double-buffered)
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[SCREEN_WIDTH * LVGL_BUFFER_LINES];
static lv_color_t buf2[SCREEN_WIDTH * LVGL_BUFFER_LINES];

// ----------------------------------------------------------------------------
// Home Assistant callbacks
// ----------------------------------------------------------------------------

static void on_weather_forecast(const char* entity_id,
                                const JsonObject& forecast_obj)
{
    weather_cache::set_forecast_response(entity_id, forecast_obj);
    shell::refresh_weather();
}

static void on_entity_changed(const HaEntity& entity)
{
    Serial.printf("[cache] changed: %s → %s\n", entity.entity_id, entity.state);

    if (entity.domain == EntityDomain::WEATHER) {
        weather_cache::update_from_entity(entity);
        shell::refresh_weather();
        return;
    }

    shell::on_entity_changed(entity);
}

static void on_ha_states(const JsonArray& states)
{
    entity_cache::populate(states);
    Serial.printf("[ha] populated cache: %u entities\n",
                  static_cast<unsigned>(entity_cache::count()));
}

static void on_ha_state_changed(const char* entity_id,
                                const JsonObject& new_state)
{
    entity_cache::update(entity_id, new_state);
}

static void on_ha_areas(const JsonArray& areas)
{
    area_cache::load_areas(areas);
}

static void on_ha_entity_registry(const JsonArray& entries)
{
    area_cache::load_entity_registry(entries,
                                     entity_cache::data(),
                                     entity_cache::count());
}

static void on_ha_device_registry(const JsonArray& devices)
{
    area_cache::build_groups(devices);

    Serial.printf("[ha] grouped entities: %u room(s)\n",
                  static_cast<unsigned>(area_cache::group_count()));
    for (size_t i = 0; i < area_cache::group_count(); ++i) {
        const area_cache::EntityGroup* g = area_cache::get_group(i);
        if (g) {
            Serial.printf("  [%s] %u entit%s\n",
                          g->name,
                          static_cast<unsigned>(g->count),
                          g->count == 1 ? "y" : "ies");
        }
    }

    // Scan entity_cache for weather entities before building the shell.
    weather_cache::init_from_cache();
    if (weather_cache::has_weather()) {
        Serial.printf("[weather] entity: %s\n", weather_cache::get_entity_id());
    }

    // Build the shell now that groups are available.
    // This runs inside ha_client::tick() (not an ISR) so LVGL calls are safe.
    shell::create();

    // Request forecast after shell is built (needs SUBSCRIBED state,
    // which is entered immediately after device_registry is processed).
    if (weather_cache::has_weather()) {
        ha_client::request_weather_forecast(weather_cache::get_entity_id(),
                                            on_weather_forecast);
    }
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
    tft.setRotation(1);        // landscape, USB-C on the right
    tft.invertDisplay(false);  // ST7789 IPS panel requires INVON for correct colors (after setRotation)

    // Configure LEDC PWM for backlight brightness control.
    // Must run after tft.init() — TFT_eSPI drives GPIO21 HIGH inside init()
    // via the TFT_BL build flag; LEDC takes over GPIO ownership after AttachPin.
    ledcSetup(BL_LEDC_CHANNEL, BL_LEDC_FREQ_HZ, BL_LEDC_BITS);
    ledcAttachPin(TFT_PIN_BL, BL_LEDC_CHANNEL);
    ledcWrite(BL_LEDC_CHANNEL, 255);  // full brightness until NVS setting is loaded

    tft.fillScreen(TFT_BLACK);

    Serial.println("[display] ST7789 initialised (320x240, landscape)");
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
// Arduino entry points
// ----------------------------------------------------------------------------

void setup()
{
    Serial.begin(115200);
    Serial.println("\n[boot] Home Remote starting...");
    Serial.printf("[boot] Free heap: %u bytes\n", ESP.getFreeHeap());

    i18n::init();   // load locale from NVS before any screen is shown

    display_init();

    // Apply stored brightness (display_init defaults to 255).
    {
        UiSettings ui_s;
        if (nvs_config::load_ui_settings(ui_s)) {
            ledcWrite(BL_LEDC_CHANNEL, ui_s.brightness);
            Serial.printf("[display] Brightness restored: %u\n", ui_s.brightness);
        }
    }

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

    // nano_backbone: load config; auto-register on first boot if URL is configured
    // but no API key is stored yet.  Runs only when WiFi is up.
    nb_client::init();
    if (wifi_manager::is_connected() && nb_client::has_config()) {
        if (!nb_client::is_registered()) {
            Serial.println("[nb] Auto-registering device...");
            nb_client::register_device();
        }
        // Kick off a firmware version check immediately after registration
        // (or on subsequent boots if already registered).
        nb_client::start_version_check();
    }

    // Show a loading screen while the HA WebSocket startup sequence runs
    // (auth → get_states → area/entity/device registries).
    // shell::create() replaces this when groups are ready.
    shell::show_loading();

    area_cache::init();
    entity_cache::init(on_entity_changed);
    ha_client::init(on_ha_states, on_ha_state_changed,
                    on_ha_areas, on_ha_entity_registry, on_ha_device_registry);

    Serial.printf("[boot] Ready. Free heap: %u bytes\n", ESP.getFreeHeap());
}

void loop()
{
    lv_timer_handler();
    wifi_manager::tick();
    ha_client::tick();
    nb_client::tick();
    shell::update_status(wifi_manager::is_connected(), ha_client::get_connection_state());
    if (nb_client::is_update_available()) shell::show_update_indicator(true);
    delay(5);
}
