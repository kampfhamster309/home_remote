#include <unity.h>
#include "touch_cal.h"

// Representative calibration data.
// Raw ADC range is roughly 200–3800 for a typical XPT2046.
static const TouchCalibration CAL_NORMAL = {
    /* x1_raw */ 350,
    /* y1_raw */ 400,
    /* x2_raw */ 3700,
    /* y2_raw */ 3650,
};

// Inverted axes (some CYD board variants read ADC in reverse direction)
static const TouchCalibration CAL_INVERTED = {
    /* x1_raw */ 3700,
    /* y1_raw */ 3650,
    /* x2_raw */ 350,
    /* y2_raw */ 400,
};

// Degenerate calibration (zero range — should not crash)
static const TouchCalibration CAL_DEGENERATE = {0, 0, 0, 0};

// Calibration specifically designed to exercise clamping:
//   x1_raw=2000 means raw=0 maps to a large negative screen x  → clamps to 0
//   x2_raw=3700 means raw=4095 maps to ~350 screen x           → clamps to SCREEN_WIDTH-1
//   Same logic applies for y.
static const TouchCalibration CAL_CLAMP = {
    /* x1_raw */ 2000,
    /* y1_raw */ 2000,
    /* x2_raw */ 3700,
    /* y2_raw */ 3700,
};

// ----------------------------------------------------------------------------
// touch_map_x — exact calibration points
// ----------------------------------------------------------------------------

void test_map_x_returns_cal_screen_x1_at_x1_raw()
{
    int16_t result = touch_map_x(CAL_NORMAL.x1_raw, CAL_NORMAL);
    TEST_ASSERT_EQUAL(CAL_SCREEN_X1, result);
}

void test_map_x_returns_cal_screen_x2_at_x2_raw()
{
    int16_t result = touch_map_x(CAL_NORMAL.x2_raw, CAL_NORMAL);
    TEST_ASSERT_EQUAL(CAL_SCREEN_X2, result);
}

void test_map_y_returns_cal_screen_y1_at_y1_raw()
{
    int16_t result = touch_map_y(CAL_NORMAL.y1_raw, CAL_NORMAL);
    TEST_ASSERT_EQUAL(CAL_SCREEN_Y1, result);
}

void test_map_y_returns_cal_screen_y2_at_y2_raw()
{
    int16_t result = touch_map_y(CAL_NORMAL.y2_raw, CAL_NORMAL);
    TEST_ASSERT_EQUAL(CAL_SCREEN_Y2, result);
}

// ----------------------------------------------------------------------------
// Linear interpolation — midpoint maps to screen midpoint
// ----------------------------------------------------------------------------

void test_map_x_midpoint_is_linear()
{
    int16_t mid_raw    = (CAL_NORMAL.x1_raw + CAL_NORMAL.x2_raw) / 2;
    int16_t mid_screen = (CAL_SCREEN_X1 + CAL_SCREEN_X2) / 2;
    int16_t result     = touch_map_x(mid_raw, CAL_NORMAL);
    // Allow ±1 px for integer division rounding
    TEST_ASSERT_INT16_WITHIN(1, mid_screen, result);
}

void test_map_y_midpoint_is_linear()
{
    int16_t mid_raw    = (CAL_NORMAL.y1_raw + CAL_NORMAL.y2_raw) / 2;
    int16_t mid_screen = (CAL_SCREEN_Y1 + CAL_SCREEN_Y2) / 2;
    int16_t result     = touch_map_y(mid_raw, CAL_NORMAL);
    TEST_ASSERT_INT16_WITHIN(1, mid_screen, result);
}

// ----------------------------------------------------------------------------
// Clamping — out-of-range raw values stay within screen bounds
// ----------------------------------------------------------------------------

void test_map_x_clamps_below_zero()
{
    // raw=0 with CAL_CLAMP (x1_raw=2000) extrapolates to a large negative screen x
    int16_t result = touch_map_x(0, CAL_CLAMP);
    TEST_ASSERT_EQUAL(0, result);
}

void test_map_x_clamps_at_screen_max()
{
    // raw=4095 with CAL_CLAMP extrapolates past SCREEN_WIDTH
    int16_t result = touch_map_x(4095, CAL_CLAMP);
    TEST_ASSERT_EQUAL(SCREEN_WIDTH - 1, result);
}

void test_map_y_clamps_below_zero()
{
    int16_t result = touch_map_y(0, CAL_CLAMP);
    TEST_ASSERT_EQUAL(0, result);
}

void test_map_y_clamps_at_screen_max()
{
    // raw=4095 with CAL_CLAMP extrapolates past SCREEN_HEIGHT
    int16_t result = touch_map_y(4095, CAL_CLAMP);
    TEST_ASSERT_EQUAL(SCREEN_HEIGHT - 1, result);
}

// ----------------------------------------------------------------------------
// Inverted axes — mapping still works when x1_raw > x2_raw
// ----------------------------------------------------------------------------

void test_map_x_inverted_cal_exact_point1()
{
    int16_t result = touch_map_x(CAL_INVERTED.x1_raw, CAL_INVERTED);
    TEST_ASSERT_EQUAL(CAL_SCREEN_X1, result);
}

void test_map_x_inverted_cal_exact_point2()
{
    int16_t result = touch_map_x(CAL_INVERTED.x2_raw, CAL_INVERTED);
    TEST_ASSERT_EQUAL(CAL_SCREEN_X2, result);
}

void test_map_y_inverted_cal_exact_point1()
{
    int16_t result = touch_map_y(CAL_INVERTED.y1_raw, CAL_INVERTED);
    TEST_ASSERT_EQUAL(CAL_SCREEN_Y1, result);
}

void test_map_y_inverted_cal_exact_point2()
{
    int16_t result = touch_map_y(CAL_INVERTED.y2_raw, CAL_INVERTED);
    TEST_ASSERT_EQUAL(CAL_SCREEN_Y2, result);
}

// ----------------------------------------------------------------------------
// Degenerate calibration — must not divide by zero or crash
// ----------------------------------------------------------------------------

void test_map_x_degenerate_returns_zero()
{
    int16_t result = touch_map_x(1000, CAL_DEGENERATE);
    TEST_ASSERT_EQUAL(0, result);
}

void test_map_y_degenerate_returns_zero()
{
    int16_t result = touch_map_y(1000, CAL_DEGENERATE);
    TEST_ASSERT_EQUAL(0, result);
}

// ----------------------------------------------------------------------------
// Runner
// ----------------------------------------------------------------------------

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    RUN_TEST(test_map_x_returns_cal_screen_x1_at_x1_raw);
    RUN_TEST(test_map_x_returns_cal_screen_x2_at_x2_raw);
    RUN_TEST(test_map_y_returns_cal_screen_y1_at_y1_raw);
    RUN_TEST(test_map_y_returns_cal_screen_y2_at_y2_raw);
    RUN_TEST(test_map_x_midpoint_is_linear);
    RUN_TEST(test_map_y_midpoint_is_linear);
    RUN_TEST(test_map_x_clamps_below_zero);
    RUN_TEST(test_map_x_clamps_at_screen_max);
    RUN_TEST(test_map_y_clamps_below_zero);
    RUN_TEST(test_map_y_clamps_at_screen_max);
    RUN_TEST(test_map_x_inverted_cal_exact_point1);
    RUN_TEST(test_map_x_inverted_cal_exact_point2);
    RUN_TEST(test_map_y_inverted_cal_exact_point1);
    RUN_TEST(test_map_y_inverted_cal_exact_point2);
    RUN_TEST(test_map_x_degenerate_returns_zero);
    RUN_TEST(test_map_y_degenerate_returns_zero);

    return UNITY_END();
}
