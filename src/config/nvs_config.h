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

}  // namespace nvs_config
