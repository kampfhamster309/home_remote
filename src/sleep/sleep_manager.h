#pragma once

#include <stdint.h>

// Light sleep manager for battery-powered mobile mode (TICKET-022).
//
// When battery mode is enabled, the device enters ESP32 light sleep after a
// configurable period of touch inactivity.  WiFi modem sleep keeps the WiFi
// association and HA WebSocket subscription alive through sleep — state_changed
// events continue to arrive and no reconnect is needed on wake.
//
// Wake source: touch IRQ on TOUCH_PIN_IRQ (GPIO 36, active LOW).
// The CPU halts; execution resumes in loop() immediately after the
// esp_light_sleep_start() call returns.
//
// Usage:
//   setup(): sleep_manager::init()
//   loop():  sleep_manager::tick()   // after lv_timer_handler()

namespace sleep_manager {

// Load settings from NVS.  Call once in setup() after display_init().
void init();

// Check inactivity and enter light sleep if the timeout has elapsed.
// No-op when battery mode is off.  Call from loop() after lv_timer_handler().
void tick();

// Returns true if battery mode is currently enabled.
bool is_battery_mode();

// Enable or disable battery mode.  Persists to NVS immediately.
void set_battery_mode(bool enabled);

// Inactivity timeout in seconds before the device sleeps.
uint16_t get_timeout_s();

// Set the inactivity timeout.  Persists to NVS immediately.
void set_timeout_s(uint16_t seconds);

} // namespace sleep_manager
