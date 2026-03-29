#pragma once

#include "touch_cal.h"

// NVS helper functions.
// Uses Arduino Preferences library under the hood.
// Expanded in later tickets to cover WiFi credentials, HA token, UI settings.

namespace nvs_config {

    // Returns true if valid calibration data was found and loaded into `out`.
    bool load_touch_cal(TouchCalibration& out);

    // Persists calibration data to NVS.
    void save_touch_cal(const TouchCalibration& cal);

    // Removes calibration data from NVS (forces re-calibration on next boot).
    void clear_touch_cal();

}  // namespace nvs_config
