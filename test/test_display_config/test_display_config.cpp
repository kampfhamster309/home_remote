#include <unity.h>
#include "display_config.h"

// ----------------------------------------------------------------------------
// Screen geometry
// ----------------------------------------------------------------------------

void test_screen_width_is_positive() {
    TEST_ASSERT_GREATER_THAN(0, SCREEN_WIDTH);
}

void test_screen_height_is_positive() {
    TEST_ASSERT_GREATER_THAN(0, SCREEN_HEIGHT);
}

void test_screen_dimensions_match_ili9341() {
    // ILI9341 in landscape: 320 wide, 240 tall
    TEST_ASSERT_EQUAL(320, SCREEN_WIDTH);
    TEST_ASSERT_EQUAL(240, SCREEN_HEIGHT);
}

void test_screen_width_greater_than_height() {
    // Landscape orientation
    TEST_ASSERT_GREATER_THAN(SCREEN_HEIGHT, SCREEN_WIDTH);
}

void test_lvgl_buffer_lines_in_range() {
    TEST_ASSERT_GREATER_THAN(0, LVGL_BUFFER_LINES);
    // Buffer must not exceed screen height
    TEST_ASSERT_LESS_OR_EQUAL(SCREEN_HEIGHT, LVGL_BUFFER_LINES);
}

// ----------------------------------------------------------------------------
// Pin range — valid ESP32 GPIO is 0..39
// ----------------------------------------------------------------------------

static void assert_valid_gpio(int pin, const char *name) {
    (void)name;
    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(0,  pin, name);
    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(39, pin, name);
}

void test_display_pins_in_valid_gpio_range() {
    assert_valid_gpio(TFT_PIN_MOSI, "TFT_PIN_MOSI");
    assert_valid_gpio(TFT_PIN_SCLK, "TFT_PIN_SCLK");
    assert_valid_gpio(TFT_PIN_CS,   "TFT_PIN_CS");
    assert_valid_gpio(TFT_PIN_DC,   "TFT_PIN_DC");
    // TFT_PIN_RST may be -1 (not connected on this board variant) — skip range check
    if (TFT_PIN_RST >= 0) assert_valid_gpio(TFT_PIN_RST, "TFT_PIN_RST");
    assert_valid_gpio(TFT_PIN_BL,   "TFT_PIN_BL");
}

void test_touch_pins_in_valid_gpio_range() {
    assert_valid_gpio(TOUCH_PIN_CLK,  "TOUCH_PIN_CLK");
    assert_valid_gpio(TOUCH_PIN_MISO, "TOUCH_PIN_MISO");
    assert_valid_gpio(TOUCH_PIN_MOSI, "TOUCH_PIN_MOSI");
    assert_valid_gpio(TOUCH_PIN_CS,   "TOUCH_PIN_CS");
    assert_valid_gpio(TOUCH_PIN_IRQ,  "TOUCH_PIN_IRQ");
}

// ----------------------------------------------------------------------------
// Pin conflict — display and touch must not share any pins
// ----------------------------------------------------------------------------

void test_no_pin_conflicts_between_display_and_touch() {
    const int display_pins[] = {
        TFT_PIN_MOSI, TFT_PIN_SCLK, TFT_PIN_CS, TFT_PIN_DC, TFT_PIN_RST, TFT_PIN_BL
    };
    const int touch_pins[] = {
        TOUCH_PIN_CLK, TOUCH_PIN_MISO, TOUCH_PIN_MOSI, TOUCH_PIN_CS, TOUCH_PIN_IRQ
    };

    for (int d : display_pins) {
        for (int t : touch_pins) {
            TEST_ASSERT_NOT_EQUAL_MESSAGE(d, t,
                "A display pin and a touch pin share the same GPIO number");
        }
    }
}

void test_display_pins_are_unique() {
    const int pins[] = {
        TFT_PIN_MOSI, TFT_PIN_SCLK, TFT_PIN_CS, TFT_PIN_DC, TFT_PIN_RST, TFT_PIN_BL
    };
    const int n = sizeof(pins) / sizeof(pins[0]);
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            TEST_ASSERT_NOT_EQUAL_MESSAGE(pins[i], pins[j],
                "Two display pins share the same GPIO number");
        }
    }
}

void test_touch_pins_are_unique() {
    const int pins[] = {
        TOUCH_PIN_CLK, TOUCH_PIN_MISO, TOUCH_PIN_MOSI, TOUCH_PIN_CS, TOUCH_PIN_IRQ
    };
    const int n = sizeof(pins) / sizeof(pins[0]);
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            TEST_ASSERT_NOT_EQUAL_MESSAGE(pins[i], pins[j],
                "Two touch pins share the same GPIO number");
        }
    }
}

// ----------------------------------------------------------------------------
// Runner
// ----------------------------------------------------------------------------

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    RUN_TEST(test_screen_width_is_positive);
    RUN_TEST(test_screen_height_is_positive);
    RUN_TEST(test_screen_dimensions_match_ili9341);
    RUN_TEST(test_screen_width_greater_than_height);
    RUN_TEST(test_lvgl_buffer_lines_in_range);
    RUN_TEST(test_display_pins_in_valid_gpio_range);
    RUN_TEST(test_touch_pins_in_valid_gpio_range);
    RUN_TEST(test_no_pin_conflicts_between_display_and_touch);
    RUN_TEST(test_display_pins_are_unique);
    RUN_TEST(test_touch_pins_are_unique);

    return UNITY_END();
}
