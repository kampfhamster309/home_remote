#pragma once

#include <lvgl.h>

// Touch driver module.
// Owns the XPT2046 hardware instance and the LVGL input device.
// Handles first-boot calibration and NVS persistence.

namespace touch_driver {

    // Initialise SPI bus and XPT2046. Load calibration from NVS if available.
    // Must be called before any other function in this namespace.
    void init();

    // Returns true if valid calibration data is loaded (from NVS or just run).
    bool is_calibrated();

    // Blocking calibration routine. Draws a two-point calibration UI using LVGL,
    // reads raw ADC samples directly from XPT2046, saves result to NVS.
    // Safe to call before or after register_lvgl_indev().
    void run_calibration();

    // Register the LVGL pointer input device using loaded calibration data.
    // Must be called after lv_init() and after calibration is complete.
    void register_lvgl_indev();

    // Return the registered LVGL input device handle (nullptr before registration).
    lv_indev_t* get_indev();

}  // namespace touch_driver
