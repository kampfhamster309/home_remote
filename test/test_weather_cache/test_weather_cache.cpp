// Pull in the implementations directly (native test pattern used across this project).
#include "ha/entity_cache.cpp"   // NOLINT — deliberate impl include for native tests
#include "ha/weather_cache.cpp"  // NOLINT

#include <unity.h>
#include <ArduinoJson.h>

// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

static void populate_weather_entity(const char* entity_id,
                                    const char* condition,
                                    float temperature)
{
    StaticJsonDocument<512> doc;
    JsonArray arr = doc.to<JsonArray>();
    JsonObject obj = arr.createNestedObject();
    obj["entity_id"] = entity_id;
    obj["state"]     = condition;
    JsonObject attrs = obj.createNestedObject("attributes");
    attrs["friendly_name"] = "Home Weather";
    attrs["temperature"]   = temperature;
    entity_cache::populate(arr);
}

// Build a minimal forecast response object inside `doc`.
// Mirrors result.response[entity_id] from weather.get_forecasts.
static JsonObject make_forecast_obj(StaticJsonDocument<512>& doc,
                                    float temp_high, float temp_low,
                                    int precip)
{
    JsonObject obj      = doc.to<JsonObject>();
    JsonArray forecast  = obj.createNestedArray("forecast");
    JsonObject today    = forecast.createNestedObject();
    today["temperature"]              = temp_high;
    today["templow"]                  = temp_low;
    today["precipitation_probability"] = precip;
    today["condition"]                = "sunny";
    return obj;
}

// ----------------------------------------------------------------------------
// setUp / tearDown
// ----------------------------------------------------------------------------

void setUp()
{
    entity_cache::init(nullptr);
    weather_cache::init_from_cache(); // starts with empty entity_cache → no weather
}

void tearDown() {}

// ----------------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------------

void test_no_weather_when_cache_empty()
{
    TEST_ASSERT_FALSE(weather_cache::has_weather());
    TEST_ASSERT_EQUAL_STRING("", weather_cache::get_entity_id());
}

void test_detects_weather_entity()
{
    populate_weather_entity("weather.home", "sunny", 22.5f);
    weather_cache::init_from_cache();
    TEST_ASSERT_TRUE(weather_cache::has_weather());
    TEST_ASSERT_EQUAL_STRING("weather.home", weather_cache::get_entity_id());
}

void test_ignores_non_weather_entities()
{
    // Populate only a light entity; weather_cache should stay empty
    StaticJsonDocument<256> doc;
    JsonArray arr = doc.to<JsonArray>();
    JsonObject obj = arr.createNestedObject();
    obj["entity_id"] = "light.living_room";
    obj["state"]     = "on";
    obj.createNestedObject("attributes");
    entity_cache::populate(arr);
    weather_cache::init_from_cache();
    TEST_ASSERT_FALSE(weather_cache::has_weather());
}

void test_condition_and_temperature_from_cache()
{
    populate_weather_entity("weather.home", "cloudy", 18.0f);
    weather_cache::init_from_cache();
    const WeatherData& wd = weather_cache::get();
    TEST_ASSERT_EQUAL_STRING("cloudy", wd.condition);
    // HA stores weather temperature in attributes.temperature, which entity_cache
    // maps to target_temp. weather_cache reads from target_temp.
    TEST_ASSERT_TRUE(wd.has_temperature);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 18.0f, wd.temperature);
    TEST_ASSERT_FALSE(wd.has_forecast);
}

void test_update_from_entity_changes_condition()
{
    populate_weather_entity("weather.home", "sunny", 22.0f);
    weather_cache::init_from_cache();

    // Simulate a state_changed event
    HaEntity updated{};
    strncpy(updated.entity_id, "weather.home", sizeof(updated.entity_id) - 1);
    strncpy(updated.state,     "rainy",        sizeof(updated.state)     - 1);
    updated.domain                = EntityDomain::WEATHER;
    updated.attrs.target_temp     = 15.5f;   // HA weather uses attributes.temperature
    updated.attrs.has_target_temp = true;    // mapped to target_temp by entity_cache

    weather_cache::update_from_entity(updated);
    const WeatherData& wd = weather_cache::get();
    TEST_ASSERT_EQUAL_STRING("rainy", wd.condition);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 15.5f, wd.temperature);
}

void test_update_ignores_wrong_entity()
{
    populate_weather_entity("weather.home", "sunny", 22.0f);
    weather_cache::init_from_cache();

    HaEntity other{};
    strncpy(other.entity_id, "weather.other", sizeof(other.entity_id) - 1);
    strncpy(other.state,     "snowy",         sizeof(other.state)     - 1);
    other.domain = EntityDomain::WEATHER;

    weather_cache::update_from_entity(other);
    // Should still be "sunny" — different entity_id
    TEST_ASSERT_EQUAL_STRING("sunny", weather_cache::get().condition);
}

void test_set_forecast_response()
{
    populate_weather_entity("weather.home", "sunny", 22.0f);
    weather_cache::init_from_cache();

    StaticJsonDocument<512> doc;
    JsonObject fobj = make_forecast_obj(doc, 26.0f, 14.0f, 30);
    weather_cache::set_forecast_response("weather.home", fobj);

    const WeatherData& wd = weather_cache::get();
    TEST_ASSERT_TRUE(wd.has_forecast);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 26.0f, wd.temp_high);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 14.0f, wd.temp_low);
    TEST_ASSERT_EQUAL(30, (int)wd.precip_probability);
}

void test_forecast_ignored_for_wrong_entity()
{
    populate_weather_entity("weather.home", "sunny", 22.0f);
    weather_cache::init_from_cache();

    StaticJsonDocument<512> doc;
    JsonObject fobj = make_forecast_obj(doc, 26.0f, 14.0f, 30);
    weather_cache::set_forecast_response("weather.other", fobj);

    TEST_ASSERT_FALSE(weather_cache::get().has_forecast);
}

void test_condition_icon_known()
{
    const char* icon = weather_cache::condition_icon("sunny");
    // UI_ICON_SUN macro — just verify it is non-empty and non-null
    TEST_ASSERT_NOT_NULL(icon);
    TEST_ASSERT_TRUE(icon[0] != '\0');
}

void test_condition_icon_unknown_returns_fallback()
{
    const char* icon = weather_cache::condition_icon("some_unknown_condition");
    TEST_ASSERT_NOT_NULL(icon);
    TEST_ASSERT_TRUE(icon[0] != '\0'); // UI_ICON_COG fallback
}

void test_condition_label_de()
{
    TEST_ASSERT_EQUAL_STRING("Sonnig",  weather_cache::condition_label("sunny",    true));
    TEST_ASSERT_EQUAL_STRING("Regen",   weather_cache::condition_label("rainy",    true));
    TEST_ASSERT_EQUAL_STRING("Schnee",  weather_cache::condition_label("snowy",    true));
    TEST_ASSERT_EQUAL_STRING("Nebel",   weather_cache::condition_label("fog",      true));
    TEST_ASSERT_EQUAL_STRING("Gewitter",weather_cache::condition_label("lightning",true));
}

void test_condition_label_en()
{
    TEST_ASSERT_EQUAL_STRING("Sunny",     weather_cache::condition_label("sunny",    false));
    TEST_ASSERT_EQUAL_STRING("Rainy",     weather_cache::condition_label("rainy",    false));
    TEST_ASSERT_EQUAL_STRING("Cloudy",    weather_cache::condition_label("cloudy",   false));
    TEST_ASSERT_EQUAL_STRING("Partly Cloudy",
                             weather_cache::condition_label("partlycloudy", false));
}

void test_condition_label_unknown_returns_slug()
{
    const char* label = weather_cache::condition_label("weird-slug", false);
    TEST_ASSERT_EQUAL_STRING("weird-slug", label);
}

// ----------------------------------------------------------------------------
// Runner
// ----------------------------------------------------------------------------

int main(int /*argc*/, char** /*argv*/)
{
    UNITY_BEGIN();

    RUN_TEST(test_no_weather_when_cache_empty);
    RUN_TEST(test_detects_weather_entity);
    RUN_TEST(test_ignores_non_weather_entities);
    RUN_TEST(test_condition_and_temperature_from_cache);
    RUN_TEST(test_update_from_entity_changes_condition);
    RUN_TEST(test_update_ignores_wrong_entity);
    RUN_TEST(test_set_forecast_response);
    RUN_TEST(test_forecast_ignored_for_wrong_entity);
    RUN_TEST(test_condition_icon_known);
    RUN_TEST(test_condition_icon_unknown_returns_fallback);
    RUN_TEST(test_condition_label_de);
    RUN_TEST(test_condition_label_en);
    RUN_TEST(test_condition_label_unknown_returns_slug);

    return UNITY_END();
}
