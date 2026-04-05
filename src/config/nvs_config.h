#pragma once

#include "touch_cal.h"

// NVS helper functions.
// Uses Arduino Preferences library under the hood.

// ----------------------------------------------------------------------------
// Network / provisioning configuration
// ----------------------------------------------------------------------------

struct NetworkConfig {
    char ssid[33];       // max 32 chars + null
    char password[64];   // max 63 chars + null (empty = open network)
    char ha_url[128];    // e.g. http://192.168.1.100:8123
    char ha_token[384];  // HA long-lived access token (JWT, ~150-250 chars)
};

// ----------------------------------------------------------------------------
// nano_backbone OTA config
// ----------------------------------------------------------------------------

struct NanoBackboneConfig {
    char nb_url[128];     // e.g. http://192.168.1.100:8000  (empty = not configured)
    char nb_api_key[128]; // Api-Key issued on registration   (empty = not yet registered)
};

// ----------------------------------------------------------------------------
// UI settings (display brightness)
// ----------------------------------------------------------------------------

struct UiSettings {
    uint8_t brightness;  // 10–255 backlight PWM duty; 255 = maximum brightness
};

namespace nvs_config {

    // ---- Touch calibration --------------------------------------------------

    bool load_touch_cal(TouchCalibration& out);
    void save_touch_cal(const TouchCalibration& cal);
    void clear_touch_cal();

    // ---- Network / provisioning config --------------------------------------

    // Returns true if valid network config was found and loaded into `out`.
    bool load_net_config(NetworkConfig& out);

    // Persists network config to NVS.
    void save_net_config(const NetworkConfig& cfg);

    // Removes network config from NVS (forces captive portal on next boot).
    void clear_net_config();

    // ---- nano_backbone OTA config -------------------------------------------

    // Returns true if a URL was previously saved (nb_url is non-empty).
    // api_key may still be empty if registration has not yet run.
    bool load_nb_config(NanoBackboneConfig& out);

    // Save both URL and key.  Typically called from the captive portal
    // (api_key = "" on first save) and from nb_client after registration.
    void save_nb_config(const NanoBackboneConfig& cfg);

    // Update only the api_key field without touching the URL.
    void save_nb_api_key(const char* key);

    // Erase only the api_key (forces re-registration on next boot).
    void clear_nb_api_key();

    // ---- UI settings --------------------------------------------------------

    // Returns true if stored UI settings were found and loaded into `out`.
    // If false, `out` is unchanged — caller should apply defaults.
    bool load_ui_settings(UiSettings& out);

    // Persists UI settings to NVS.
    void save_ui_settings(const UiSettings& s);

    // ---- OTA update state (TICKET-020) --------------------------------------
    // Used by nb_client to detect boot loops after an OTA update.

    // Mark that OTA just flashed and the device is about to reboot.
    // Resets the boot counter to 0 so counting starts fresh from the new firmware.
    void set_update_pending();

    // Increment the per-boot counter — no-op if no update is pending.
    // Call early in setup() before any code that could crash or hang.
    void increment_boot_count();

    // True if an OTA update was pending at last boot (flag set by set_update_pending).
    bool load_update_pending();

    // Current boot-attempt counter (0 when no update is pending).
    uint8_t load_boot_count();

    // Clear both the pending flag and boot counter (after success or rollback).
    void clear_update_state();

}  // namespace nvs_config
