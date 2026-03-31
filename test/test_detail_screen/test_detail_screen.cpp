// Tests for the has_detail() inline function in detail_screen.h.
// detail_screen.cpp is NOT compiled here because it depends on LVGL.
// has_detail() is defined inline in the header and has no LVGL dependencies.

#include "ui/detail_screen.h"

#include <unity.h>

// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

static HaEntity make_entity(EntityDomain domain, HaAttributes attrs = {})
{
    HaEntity e{};
    e.domain = domain;
    e.attrs  = attrs;
    return e;
}

// ----------------------------------------------------------------------------
// LIGHT
// ----------------------------------------------------------------------------

void test_light_with_brightness_has_detail()
{
    HaAttributes a{};
    a.has_brightness = true;
    TEST_ASSERT_TRUE(has_detail(make_entity(EntityDomain::LIGHT, a)));
}

void test_light_with_color_temp_has_detail()
{
    HaAttributes a{};
    a.has_color_temp = true;
    TEST_ASSERT_TRUE(has_detail(make_entity(EntityDomain::LIGHT, a)));
}

void test_light_with_both_attrs_has_detail()
{
    HaAttributes a{};
    a.has_brightness = true;
    a.has_color_temp = true;
    TEST_ASSERT_TRUE(has_detail(make_entity(EntityDomain::LIGHT, a)));
}

void test_light_without_attrs_no_detail()
{
    TEST_ASSERT_FALSE(has_detail(make_entity(EntityDomain::LIGHT)));
}

// ----------------------------------------------------------------------------
// CLIMATE
// ----------------------------------------------------------------------------

void test_climate_with_target_temp_has_detail()
{
    HaAttributes a{};
    a.has_target_temp = true;
    TEST_ASSERT_TRUE(has_detail(make_entity(EntityDomain::CLIMATE, a)));
}

void test_climate_without_target_temp_no_detail()
{
    TEST_ASSERT_FALSE(has_detail(make_entity(EntityDomain::CLIMATE)));
}

// ----------------------------------------------------------------------------
// COVER
// ----------------------------------------------------------------------------

void test_cover_with_position_has_detail()
{
    HaAttributes a{};
    a.has_position = true;
    TEST_ASSERT_TRUE(has_detail(make_entity(EntityDomain::COVER, a)));
}

void test_cover_without_position_no_detail()
{
    TEST_ASSERT_FALSE(has_detail(make_entity(EntityDomain::COVER)));
}

// ----------------------------------------------------------------------------
// Binary on/off domains — never have a detail screen
// ----------------------------------------------------------------------------

void test_switch_no_detail()
{
    TEST_ASSERT_FALSE(has_detail(make_entity(EntityDomain::SWITCH)));
}

void test_sensor_no_detail()
{
    TEST_ASSERT_FALSE(has_detail(make_entity(EntityDomain::SENSOR)));
}

void test_binary_sensor_no_detail()
{
    TEST_ASSERT_FALSE(has_detail(make_entity(EntityDomain::BINARY_SENSOR)));
}

void test_input_boolean_no_detail()
{
    TEST_ASSERT_FALSE(has_detail(make_entity(EntityDomain::INPUT_BOOLEAN)));
}

void test_media_player_no_detail()
{
    TEST_ASSERT_FALSE(has_detail(make_entity(EntityDomain::MEDIA_PLAYER)));
}

void test_lock_no_detail()
{
    TEST_ASSERT_FALSE(has_detail(make_entity(EntityDomain::LOCK)));
}

void test_unknown_no_detail()
{
    TEST_ASSERT_FALSE(has_detail(make_entity(EntityDomain::UNKNOWN)));
}

// ----------------------------------------------------------------------------
// Entry point
// ----------------------------------------------------------------------------

int main()
{
    UNITY_BEGIN();

    RUN_TEST(test_light_with_brightness_has_detail);
    RUN_TEST(test_light_with_color_temp_has_detail);
    RUN_TEST(test_light_with_both_attrs_has_detail);
    RUN_TEST(test_light_without_attrs_no_detail);

    RUN_TEST(test_climate_with_target_temp_has_detail);
    RUN_TEST(test_climate_without_target_temp_no_detail);

    RUN_TEST(test_cover_with_position_has_detail);
    RUN_TEST(test_cover_without_position_no_detail);

    RUN_TEST(test_switch_no_detail);
    RUN_TEST(test_sensor_no_detail);
    RUN_TEST(test_binary_sensor_no_detail);
    RUN_TEST(test_input_boolean_no_detail);
    RUN_TEST(test_media_player_no_detail);
    RUN_TEST(test_lock_no_detail);
    RUN_TEST(test_unknown_no_detail);

    return UNITY_END();
}
