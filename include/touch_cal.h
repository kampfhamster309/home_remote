#pragma once

#include <stdint.h>
#include "display_config.h"

// ----------------------------------------------------------------------------
// Calibration target positions (screen pixels)
// The user is asked to tap these two points during calibration.
// Placed inward from the corners to ensure the touch area is reachable.
// ----------------------------------------------------------------------------
static constexpr int16_t CAL_SCREEN_X1 = 30;
static constexpr int16_t CAL_SCREEN_Y1 = 30;
static constexpr int16_t CAL_SCREEN_X2 = 290;
static constexpr int16_t CAL_SCREEN_Y2 = 210;

// Number of raw ADC samples averaged per calibration point
static constexpr int CAL_SAMPLES = 10;

// ----------------------------------------------------------------------------
// Calibration data stored in NVS
// Holds the raw XPT2046 ADC values recorded at the two target positions above.
// ----------------------------------------------------------------------------
struct TouchCalibration {
    int16_t x1_raw;  // raw X ADC at (CAL_SCREEN_X1, CAL_SCREEN_Y1)
    int16_t y1_raw;  // raw Y ADC at (CAL_SCREEN_X1, CAL_SCREEN_Y1)
    int16_t x2_raw;  // raw X ADC at (CAL_SCREEN_X2, CAL_SCREEN_Y2)
    int16_t y2_raw;  // raw Y ADC at (CAL_SCREEN_X2, CAL_SCREEN_Y2)
};

// ----------------------------------------------------------------------------
// Coordinate mapping (pure C++, no Arduino dependencies — unit-testable)
//
// Maps a raw ADC value to a screen pixel coordinate using linear interpolation
// between the two calibration points. Result is clamped to the screen bounds.
// Handles both normal and inverted ADC ranges (i.e. x1_raw > x2_raw).
// ----------------------------------------------------------------------------

inline int16_t touch_map_x(int16_t raw, const TouchCalibration& cal)
{
    if (cal.x1_raw == cal.x2_raw) return 0;
    int32_t mapped = (int32_t)(raw - cal.x1_raw)
                     * (CAL_SCREEN_X2 - CAL_SCREEN_X1)
                     / (cal.x2_raw - cal.x1_raw)
                     + CAL_SCREEN_X1;
    if (mapped < 0) return 0;
    if (mapped >= SCREEN_WIDTH) return SCREEN_WIDTH - 1;
    return static_cast<int16_t>(mapped);
}

inline int16_t touch_map_y(int16_t raw, const TouchCalibration& cal)
{
    if (cal.y1_raw == cal.y2_raw) return 0;
    int32_t mapped = (int32_t)(raw - cal.y1_raw)
                     * (CAL_SCREEN_Y2 - CAL_SCREEN_Y1)
                     / (cal.y2_raw - cal.y1_raw)
                     + CAL_SCREEN_Y1;
    if (mapped < 0) return 0;
    if (mapped >= SCREEN_HEIGHT) return SCREEN_HEIGHT - 1;
    return static_cast<int16_t>(mapped);
}
