// entity_cache.cpp is not header-only, so pull in the implementation directly.
// This is safe because entity_cache.cpp has no Arduino-specific dependencies.
#include "ha/entity_cache.cpp"  // NOLINT — deliberate impl include for native tests

#include <unity.h>

// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

// Build a minimal state object inside an existing JsonDocument.
// Returns the created JsonObject.
template <typename TDoc>
static JsonObject add_state(TDoc& doc, JsonArray& arr,
                            const char* entity_id, const char* state)
{
    JsonObject obj = arr.createNestedObject();
    obj["entity_id"] = entity_id;
    obj["state"]     = state;
    obj.createNestedObject("attributes"); // empty attrs
    return obj;
}

// ----------------------------------------------------------------------------
// setUp / tearDown
// ----------------------------------------------------------------------------

void setUp()
{
    entity_cache::init(nullptr);
}

void tearDown() {}

// ----------------------------------------------------------------------------
// Empty cache
// ----------------------------------------------------------------------------

void test_empty_after_init()
{
    TEST_ASSERT_EQUAL(0, (int)entity_cache::count());
}

// ----------------------------------------------------------------------------
// populate — basic
// ----------------------------------------------------------------------------

void test_populate_single_entity()
{
    StaticJsonDocument<512> doc;
    JsonArray arr = doc.to<JsonArray>();
    add_state(doc, arr, "switch.kitchen", "off");

    entity_cache::populate(arr);
    TEST_ASSERT_EQUAL(1, (int)entity_cache::count());
}

void test_populate_multiple_entities()
{
    StaticJsonDocument<1024> doc;
    JsonArray arr = doc.to<JsonArray>();
    add_state(doc, arr, "light.hall",       "on");
    add_state(doc, arr, "switch.fan",       "off");
    add_state(doc, arr, "cover.garage",     "closed");

    entity_cache::populate(arr);
    TEST_ASSERT_EQUAL(3, (int)entity_cache::count());
}

void test_populate_clears_previous_data()
{
    {
        StaticJsonDocument<512> doc;
        JsonArray arr = doc.to<JsonArray>();
        add_state(doc, arr, "light.old", "on");
        entity_cache::populate(arr);
        TEST_ASSERT_EQUAL(1, (int)entity_cache::count());
    }
    {
        StaticJsonDocument<1024> doc;
        JsonArray arr = doc.to<JsonArray>();
        add_state(doc, arr, "switch.new1", "off");
        add_state(doc, arr, "switch.new2", "on");
        entity_cache::populate(arr);
        TEST_ASSERT_EQUAL(2, (int)entity_cache::count());
        TEST_ASSERT_NULL(entity_cache::find("light.old"));
    }
}

// ----------------------------------------------------------------------------
// state field
// ----------------------------------------------------------------------------

void test_state_is_stored()
{
    StaticJsonDocument<512> doc;
    JsonArray arr = doc.to<JsonArray>();
    add_state(doc, arr, "switch.tv", "on");
    entity_cache::populate(arr);

    const HaEntity* e = entity_cache::find("switch.tv");
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_STRING("on", e->state);
}

// ----------------------------------------------------------------------------
// friendly_name
// ----------------------------------------------------------------------------

void test_friendly_name_from_attributes()
{
    StaticJsonDocument<512> doc;
    JsonArray arr = doc.to<JsonArray>();
    JsonObject obj = arr.createNestedObject();
    obj["entity_id"] = "light.lounge";
    obj["state"]     = "off";
    JsonObject attrs = obj.createNestedObject("attributes");
    attrs["friendly_name"] = "Lounge Light";

    entity_cache::populate(arr);
    const HaEntity* e = entity_cache::find("light.lounge");
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_STRING("Lounge Light", e->friendly_name);
}

void test_friendly_name_fallback_to_entity_id()
{
    StaticJsonDocument<512> doc;
    JsonArray arr = doc.to<JsonArray>();
    add_state(doc, arr, "switch.heater", "off"); // no friendly_name in attrs

    entity_cache::populate(arr);
    const HaEntity* e = entity_cache::find("switch.heater");
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_STRING("switch.heater", e->friendly_name);
}

// ----------------------------------------------------------------------------
// domain parsing
// ----------------------------------------------------------------------------

void test_domain_light()
{
    StaticJsonDocument<256> doc;
    JsonArray arr = doc.to<JsonArray>();
    add_state(doc, arr, "light.desk", "on");
    entity_cache::populate(arr);
    TEST_ASSERT_EQUAL((int)EntityDomain::LIGHT,
                      (int)entity_cache::find("light.desk")->domain);
}

void test_domain_switch()
{
    StaticJsonDocument<256> doc;
    JsonArray arr = doc.to<JsonArray>();
    add_state(doc, arr, "switch.fan", "off");
    entity_cache::populate(arr);
    TEST_ASSERT_EQUAL((int)EntityDomain::SWITCH,
                      (int)entity_cache::find("switch.fan")->domain);
}

void test_domain_cover()
{
    StaticJsonDocument<256> doc;
    JsonArray arr = doc.to<JsonArray>();
    add_state(doc, arr, "cover.garage", "open");
    entity_cache::populate(arr);
    TEST_ASSERT_EQUAL((int)EntityDomain::COVER,
                      (int)entity_cache::find("cover.garage")->domain);
}

void test_domain_climate()
{
    StaticJsonDocument<256> doc;
    JsonArray arr = doc.to<JsonArray>();
    add_state(doc, arr, "climate.thermostat", "heat");
    entity_cache::populate(arr);
    TEST_ASSERT_EQUAL((int)EntityDomain::CLIMATE,
                      (int)entity_cache::find("climate.thermostat")->domain);
}

void test_domain_sensor()
{
    StaticJsonDocument<256> doc;
    JsonArray arr = doc.to<JsonArray>();
    add_state(doc, arr, "sensor.temp", "21.5");
    entity_cache::populate(arr);
    TEST_ASSERT_EQUAL((int)EntityDomain::SENSOR,
                      (int)entity_cache::find("sensor.temp")->domain);
}

void test_domain_binary_sensor()
{
    StaticJsonDocument<256> doc;
    JsonArray arr = doc.to<JsonArray>();
    add_state(doc, arr, "binary_sensor.door", "off");
    entity_cache::populate(arr);
    TEST_ASSERT_EQUAL((int)EntityDomain::BINARY_SENSOR,
                      (int)entity_cache::find("binary_sensor.door")->domain);
}

void test_domain_unknown()
{
    StaticJsonDocument<256> doc;
    JsonArray arr = doc.to<JsonArray>();
    add_state(doc, arr, "custom_integration.widget", "idle");
    entity_cache::populate(arr);
    TEST_ASSERT_EQUAL((int)EntityDomain::UNKNOWN,
                      (int)entity_cache::find("custom_integration.widget")->domain);
}

// ----------------------------------------------------------------------------
// Attribute parsing
// ----------------------------------------------------------------------------

void test_brightness_attribute()
{
    StaticJsonDocument<512> doc;
    JsonArray arr = doc.to<JsonArray>();
    JsonObject obj = arr.createNestedObject();
    obj["entity_id"] = "light.bedroom";
    obj["state"]     = "on";
    JsonObject attrs = obj.createNestedObject("attributes");
    attrs["brightness"] = 180;

    entity_cache::populate(arr);
    const HaEntity* e = entity_cache::find("light.bedroom");
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_TRUE(e->attrs.has_brightness);
    TEST_ASSERT_EQUAL(180, (int)e->attrs.brightness);
}

void test_color_temp_attribute()
{
    StaticJsonDocument<512> doc;
    JsonArray arr = doc.to<JsonArray>();
    JsonObject obj = arr.createNestedObject();
    obj["entity_id"] = "light.office";
    obj["state"]     = "on";
    JsonObject attrs = obj.createNestedObject("attributes");
    attrs["color_temp"] = 350;

    entity_cache::populate(arr);
    const HaEntity* e = entity_cache::find("light.office");
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_TRUE(e->attrs.has_color_temp);
    TEST_ASSERT_EQUAL(350, (int)e->attrs.color_temp);
}

void test_cover_position_attribute()
{
    StaticJsonDocument<512> doc;
    JsonArray arr = doc.to<JsonArray>();
    JsonObject obj = arr.createNestedObject();
    obj["entity_id"] = "cover.blinds";
    obj["state"]     = "open";
    JsonObject attrs = obj.createNestedObject("attributes");
    attrs["current_position"] = 75;

    entity_cache::populate(arr);
    const HaEntity* e = entity_cache::find("cover.blinds");
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_TRUE(e->attrs.has_position);
    TEST_ASSERT_EQUAL(75, (int)e->attrs.position);
}

void test_climate_temperature_attributes()
{
    StaticJsonDocument<512> doc;
    JsonArray arr = doc.to<JsonArray>();
    JsonObject obj = arr.createNestedObject();
    obj["entity_id"] = "climate.living_room";
    obj["state"]     = "heat";
    JsonObject attrs = obj.createNestedObject("attributes");
    attrs["current_temperature"] = 20.5f;
    attrs["temperature"]         = 22.0f;

    entity_cache::populate(arr);
    const HaEntity* e = entity_cache::find("climate.living_room");
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_TRUE(e->attrs.has_temperature);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 20.5f, e->attrs.temperature);
    TEST_ASSERT_TRUE(e->attrs.has_target_temp);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 22.0f, e->attrs.target_temp);
}

void test_no_spurious_attrs_for_switch()
{
    StaticJsonDocument<256> doc;
    JsonArray arr = doc.to<JsonArray>();
    add_state(doc, arr, "switch.pump", "off");
    entity_cache::populate(arr);
    const HaEntity* e = entity_cache::find("switch.pump");
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_FALSE(e->attrs.has_brightness);
    TEST_ASSERT_FALSE(e->attrs.has_position);
    TEST_ASSERT_FALSE(e->attrs.has_temperature);
}

// ----------------------------------------------------------------------------
// find / get
// ----------------------------------------------------------------------------

void test_find_existing()
{
    StaticJsonDocument<512> doc;
    JsonArray arr = doc.to<JsonArray>();
    add_state(doc, arr, "light.entry", "on");
    entity_cache::populate(arr);
    TEST_ASSERT_NOT_NULL(entity_cache::find("light.entry"));
}

void test_find_nonexistent_returns_null()
{
    StaticJsonDocument<256> doc;
    JsonArray arr = doc.to<JsonArray>();
    add_state(doc, arr, "light.entry", "on");
    entity_cache::populate(arr);
    TEST_ASSERT_NULL(entity_cache::find("light.nonexistent"));
}

void test_get_by_index()
{
    StaticJsonDocument<512> doc;
    JsonArray arr = doc.to<JsonArray>();
    add_state(doc, arr, "light.a", "on");
    add_state(doc, arr, "light.b", "off");
    entity_cache::populate(arr);
    const HaEntity* e = entity_cache::get(1);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_STRING("light.b", e->entity_id);
}

void test_get_out_of_bounds_returns_null()
{
    StaticJsonDocument<256> doc;
    JsonArray arr = doc.to<JsonArray>();
    add_state(doc, arr, "switch.x", "off");
    entity_cache::populate(arr);
    TEST_ASSERT_NULL(entity_cache::get(1));
    TEST_ASSERT_NULL(entity_cache::get(100));
}

// ----------------------------------------------------------------------------
// update
// ----------------------------------------------------------------------------

void test_update_changes_state()
{
    StaticJsonDocument<512> doc;
    JsonArray arr = doc.to<JsonArray>();
    add_state(doc, arr, "light.lamp", "off");
    entity_cache::populate(arr);

    StaticJsonDocument<256> upd;
    upd["entity_id"] = "light.lamp";
    upd["state"]     = "on";
    upd.createNestedObject("attributes");
    entity_cache::update("light.lamp", upd.as<JsonObject>());

    const HaEntity* e = entity_cache::find("light.lamp");
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_STRING("on", e->state);
}

void test_update_fires_callback()
{
    static bool called = false;
    static char fired_id[64] = {};

    entity_cache::init([](const HaEntity& ent) {
        called    = true;
        strncpy(fired_id, ent.entity_id, sizeof(fired_id) - 1);
    });

    StaticJsonDocument<512> doc;
    JsonArray arr = doc.to<JsonArray>();
    add_state(doc, arr, "switch.boiler", "off");
    entity_cache::populate(arr);

    StaticJsonDocument<256> upd;
    upd["entity_id"] = "switch.boiler";
    upd["state"]     = "on";
    upd.createNestedObject("attributes");
    entity_cache::update("switch.boiler", upd.as<JsonObject>());

    TEST_ASSERT_TRUE(called);
    TEST_ASSERT_EQUAL_STRING("switch.boiler", fired_id);
}

void test_update_unknown_entity_is_noop()
{
    StaticJsonDocument<512> doc;
    JsonArray arr = doc.to<JsonArray>();
    add_state(doc, arr, "light.a", "off");
    entity_cache::populate(arr);

    StaticJsonDocument<256> upd;
    upd["entity_id"] = "light.unknown";
    upd["state"]     = "on";
    upd.createNestedObject("attributes");
    // Must not crash
    entity_cache::update("light.unknown", upd.as<JsonObject>());

    TEST_ASSERT_EQUAL(1, (int)entity_cache::count()); // cache unchanged
}

// ----------------------------------------------------------------------------
// overflow protection
// ----------------------------------------------------------------------------

void test_populate_overflow()
{
    // Build MAX_ENTITIES + 1 entries; cache must cap at MAX_ENTITIES
    StaticJsonDocument<16384> doc;
    JsonArray arr = doc.to<JsonArray>();
    for (size_t i = 0; i <= MAX_ENTITIES; ++i) {
        char eid[24];
        snprintf(eid, sizeof(eid), "switch.sw_%u", (unsigned)i);
        JsonObject obj = arr.createNestedObject();
        obj["entity_id"] = eid;
        obj["state"]     = "off";
        obj.createNestedObject("attributes");
    }
    entity_cache::populate(arr);
    TEST_ASSERT_EQUAL((int)MAX_ENTITIES, (int)entity_cache::count());
}

// ----------------------------------------------------------------------------
// Runner
// ----------------------------------------------------------------------------

int main(int /*argc*/, char** /*argv*/)
{
    UNITY_BEGIN();

    RUN_TEST(test_empty_after_init);
    RUN_TEST(test_populate_single_entity);
    RUN_TEST(test_populate_multiple_entities);
    RUN_TEST(test_populate_clears_previous_data);
    RUN_TEST(test_state_is_stored);
    RUN_TEST(test_friendly_name_from_attributes);
    RUN_TEST(test_friendly_name_fallback_to_entity_id);
    RUN_TEST(test_domain_light);
    RUN_TEST(test_domain_switch);
    RUN_TEST(test_domain_cover);
    RUN_TEST(test_domain_climate);
    RUN_TEST(test_domain_sensor);
    RUN_TEST(test_domain_binary_sensor);
    RUN_TEST(test_domain_unknown);
    RUN_TEST(test_brightness_attribute);
    RUN_TEST(test_color_temp_attribute);
    RUN_TEST(test_cover_position_attribute);
    RUN_TEST(test_climate_temperature_attributes);
    RUN_TEST(test_no_spurious_attrs_for_switch);
    RUN_TEST(test_find_existing);
    RUN_TEST(test_find_nonexistent_returns_null);
    RUN_TEST(test_get_by_index);
    RUN_TEST(test_get_out_of_bounds_returns_null);
    RUN_TEST(test_update_changes_state);
    RUN_TEST(test_update_fires_callback);
    RUN_TEST(test_update_unknown_entity_is_noop);
    RUN_TEST(test_populate_overflow);

    return UNITY_END();
}
