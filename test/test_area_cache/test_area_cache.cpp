// Native unit tests for area_cache.
//
// area_cache.cpp has no Arduino-specific dependencies; the implementation is
// included directly so the native test build does not need build_src_filter hacks.

#include <unity.h>
#include <ArduinoJson.h>

// Stub ha_entity.h types required by area_cache
#include "ha_entity.h"
#include "ha_area.h"
#include "ha/area_cache.cpp"

// ---- Helpers ----------------------------------------------------------------

// Build a JsonArray of area objects inside `doc`.
// Format: [{"area_id": aid, "name": name}, ...]
// Pairs is a flat list: {aid1, name1, aid2, name2, ...}
template<size_t N>
static JsonArray make_areas(DynamicJsonDocument& doc,
                             const char* (&pairs)[N])
{
    static_assert(N % 2 == 0, "pairs must have even length");
    JsonArray arr = doc.to<JsonArray>();
    for (size_t i = 0; i < N; i += 2) {
        JsonObject o = arr.createNestedObject();
        o["area_id"] = pairs[i];
        o["name"]    = pairs[i + 1];
    }
    return arr;
}

// Build a JsonArray of entity-registry entries.
// Each tuple: {entity_id, area_id_or_null, device_id_or_null}
// Pass "" for null-equivalent (treated as "no area").
static JsonArray make_entity_reg(DynamicJsonDocument& doc,
                                 const char* tuples[][3], size_t count)
{
    JsonArray arr = doc.to<JsonArray>();
    for (size_t i = 0; i < count; ++i) {
        JsonObject o = arr.createNestedObject();
        o["entity_id"] = tuples[i][0];
        if (tuples[i][1][0] != '\0') {
            o["area_id"] = tuples[i][1];
        } else {
            o["area_id"] = nullptr;
        }
        if (tuples[i][2][0] != '\0') {
            o["device_id"] = tuples[i][2];
        } else {
            o["device_id"] = nullptr;
        }
    }
    return arr;
}

// Build a JsonArray of device-registry entries: {id, area_id_or_null}
static JsonArray make_device_reg(DynamicJsonDocument& doc,
                                 const char* pairs[][2], size_t count)
{
    JsonArray arr = doc.to<JsonArray>();
    for (size_t i = 0; i < count; ++i) {
        JsonObject o = arr.createNestedObject();
        o["id"] = pairs[i][0];
        if (pairs[i][1][0] != '\0') {
            o["area_id"] = pairs[i][1];
        } else {
            o["area_id"] = nullptr;
        }
    }
    return arr;
}

static HaEntity make_entity(const char* entity_id)
{
    HaEntity e{};
    strncpy(e.entity_id, entity_id, sizeof(e.entity_id) - 1);
    return e;
}

// ---- Test setup/teardown ----------------------------------------------------

void setUp()    { area_cache::init(); }
void tearDown() {}

// ============================================================================
// load_areas tests
// ============================================================================

void test_load_areas_empty()
{
    DynamicJsonDocument doc(256);
    JsonArray arr = doc.to<JsonArray>();
    area_cache::load_areas(arr);
    TEST_ASSERT_NULL(area_cache::get_area_name("living_room"));
}

void test_load_areas_single()
{
    DynamicJsonDocument doc(256);
    const char* pairs[] = {"living_room", "Living Room"};
    area_cache::load_areas(make_areas(doc, pairs));
    TEST_ASSERT_EQUAL_STRING("Living Room", area_cache::get_area_name("living_room"));
}

void test_load_areas_multiple()
{
    DynamicJsonDocument doc(512);
    const char* pairs[] = {"living_room", "Living Room", "bedroom", "Bedroom"};
    area_cache::load_areas(make_areas(doc, pairs));
    TEST_ASSERT_EQUAL_STRING("Living Room", area_cache::get_area_name("living_room"));
    TEST_ASSERT_EQUAL_STRING("Bedroom",     area_cache::get_area_name("bedroom"));
}

void test_load_areas_unknown_id_returns_null()
{
    DynamicJsonDocument doc(256);
    const char* pairs[] = {"living_room", "Living Room"};
    area_cache::load_areas(make_areas(doc, pairs));
    TEST_ASSERT_NULL(area_cache::get_area_name("kitchen"));
}

void test_load_areas_skips_empty_area_id()
{
    DynamicJsonDocument doc(256);
    // Entry with empty area_id should be silently ignored
    JsonArray arr = doc.to<JsonArray>();
    JsonObject bad = arr.createNestedObject();
    bad["area_id"] = "";
    bad["name"]    = "Bad";
    JsonObject good = arr.createNestedObject();
    good["area_id"] = "bedroom";
    good["name"]    = "Bedroom";
    area_cache::load_areas(arr);
    TEST_ASSERT_NULL(area_cache::get_area_name(""));
    TEST_ASSERT_EQUAL_STRING("Bedroom", area_cache::get_area_name("bedroom"));
}

void test_load_areas_caps_at_max()
{
    DynamicJsonDocument doc(4096);
    JsonArray arr = doc.to<JsonArray>();
    // Add MAX_AREAS + 2 entries
    char aid[32]; char name[64];
    for (size_t i = 0; i < MAX_AREAS + 2; ++i) {
        snprintf(aid,  sizeof(aid),  "area_%zu", i);
        snprintf(name, sizeof(name), "Area %zu",  i);
        JsonObject o = arr.createNestedObject();
        o["area_id"] = aid;
        o["name"]    = name;
    }
    area_cache::load_areas(arr);
    // area_0 through area_(MAX_AREAS-1) should be present
    TEST_ASSERT_NOT_NULL(area_cache::get_area_name("area_0"));
    // area_MAX_AREAS should not have been stored
    snprintf(aid, sizeof(aid), "area_%zu", MAX_AREAS);
    TEST_ASSERT_NULL(area_cache::get_area_name(aid));
}

// ============================================================================
// build_groups tests
// ============================================================================

void test_build_groups_no_entities()
{
    DynamicJsonDocument er_doc(64), dr_doc(64);
    JsonArray er = er_doc.to<JsonArray>();
    JsonArray dr = dr_doc.to<JsonArray>();
    area_cache::load_entity_registry(er, nullptr, 0);
    area_cache::build_groups(dr);
    TEST_ASSERT_EQUAL(0, area_cache::group_count());
}

void test_build_groups_entity_with_direct_area()
{
    // Setup areas
    DynamicJsonDocument ar_doc(256);
    const char* pairs[] = {"living_room", "Living Room"};
    area_cache::load_areas(make_areas(ar_doc, pairs));

    HaEntity entities[1] = {make_entity("light.ceiling")};

    // Entity registry: light.ceiling → area living_room
    DynamicJsonDocument er_doc(512);
    const char* er_tuples[][3] = {{"light.ceiling", "living_room", ""}};
    area_cache::load_entity_registry(make_entity_reg(er_doc, er_tuples, 1),
                                     entities, 1);

    DynamicJsonDocument dr_doc(64);
    JsonArray dr = dr_doc.to<JsonArray>();
    area_cache::build_groups(dr);

    TEST_ASSERT_EQUAL(1, area_cache::group_count());
    const area_cache::EntityGroup* g = area_cache::get_group(0);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_EQUAL_STRING("Living Room", g->name);
    TEST_ASSERT_EQUAL(1, g->count);
    TEST_ASSERT_EQUAL(0, g->entity_indices[0]); // index into entities[]
}

void test_build_groups_entity_no_area_is_dropped()
{
    HaEntity entities[1] = {make_entity("light.ceiling")};

    // Entity has no area and no device — should be silently dropped
    DynamicJsonDocument er_doc(256);
    const char* er_tuples[][3] = {{"light.ceiling", "", ""}};
    area_cache::load_entity_registry(make_entity_reg(er_doc, er_tuples, 1),
                                     entities, 1);

    DynamicJsonDocument dr_doc(64);
    JsonArray dr = dr_doc.to<JsonArray>();
    area_cache::build_groups(dr);

    TEST_ASSERT_EQUAL(0, area_cache::group_count());
}

void test_build_groups_entity_not_in_registry_is_dropped()
{
    HaEntity entities[1] = {make_entity("light.ceiling")};

    // Entity registry doesn't mention this entity at all — should be dropped
    DynamicJsonDocument er_doc(64);
    JsonArray er = er_doc.to<JsonArray>();
    area_cache::load_entity_registry(er, entities, 1);

    DynamicJsonDocument dr_doc(64);
    JsonArray dr = dr_doc.to<JsonArray>();
    area_cache::build_groups(dr);

    TEST_ASSERT_EQUAL(0, area_cache::group_count());
}

void test_build_groups_multiple_entities_same_area()
{
    DynamicJsonDocument ar_doc(256);
    const char* pairs[] = {"living_room", "Living Room"};
    area_cache::load_areas(make_areas(ar_doc, pairs));

    HaEntity entities[2] = {make_entity("light.a"), make_entity("switch.b")};

    DynamicJsonDocument er_doc(512);
    const char* er_tuples[][3] = {
        {"light.a",   "living_room", ""},
        {"switch.b",  "living_room", ""},
    };
    area_cache::load_entity_registry(make_entity_reg(er_doc, er_tuples, 2),
                                     entities, 2);

    DynamicJsonDocument dr_doc(64);
    area_cache::build_groups(dr_doc.to<JsonArray>());

    TEST_ASSERT_EQUAL(1, area_cache::group_count());
    const area_cache::EntityGroup* g = area_cache::get_group(0);
    TEST_ASSERT_EQUAL(2, g->count);
}

void test_build_groups_multiple_areas()
{
    DynamicJsonDocument ar_doc(512);
    const char* pairs[] = {"living_room", "Living Room", "bedroom", "Bedroom"};
    area_cache::load_areas(make_areas(ar_doc, pairs));

    HaEntity entities[2] = {make_entity("light.a"), make_entity("light.b")};

    DynamicJsonDocument er_doc(512);
    const char* er_tuples[][3] = {
        {"light.a", "living_room", ""},
        {"light.b", "bedroom",     ""},
    };
    area_cache::load_entity_registry(make_entity_reg(er_doc, er_tuples, 2),
                                     entities, 2);

    DynamicJsonDocument dr_doc(64);
    area_cache::build_groups(dr_doc.to<JsonArray>());

    TEST_ASSERT_EQUAL(2, area_cache::group_count());
}

void test_build_groups_unassigned_entity_dropped_assigned_kept()
{
    DynamicJsonDocument ar_doc(256);
    const char* pairs[] = {"living_room", "Living Room"};
    area_cache::load_areas(make_areas(ar_doc, pairs));

    // One entity has no area (dropped), one is assigned
    HaEntity entities[2] = {make_entity("sensor.x"), make_entity("light.a")};

    DynamicJsonDocument er_doc(512);
    const char* er_tuples[][3] = {
        {"sensor.x", "",            ""},
        {"light.a",  "living_room", ""},
    };
    area_cache::load_entity_registry(make_entity_reg(er_doc, er_tuples, 2),
                                     entities, 2);

    DynamicJsonDocument dr_doc(64);
    area_cache::build_groups(dr_doc.to<JsonArray>());

    // Only the assigned entity's group survives
    TEST_ASSERT_EQUAL(1, area_cache::group_count());
    TEST_ASSERT_EQUAL_STRING("Living Room", area_cache::get_group(0)->name);
    TEST_ASSERT_EQUAL(1, area_cache::get_group(0)->count);
}

void test_build_groups_no_other_when_all_assigned()
{
    DynamicJsonDocument ar_doc(256);
    const char* pairs[] = {"living_room", "Living Room"};
    area_cache::load_areas(make_areas(ar_doc, pairs));

    HaEntity entities[1] = {make_entity("light.a")};
    DynamicJsonDocument er_doc(256);
    const char* er_tuples[][3] = {{"light.a", "living_room", ""}};
    area_cache::load_entity_registry(make_entity_reg(er_doc, er_tuples, 1),
                                     entities, 1);

    DynamicJsonDocument dr_doc(64);
    area_cache::build_groups(dr_doc.to<JsonArray>());

    // Should be exactly 1 group and it should NOT be "Other"
    TEST_ASSERT_EQUAL(1, area_cache::group_count());
    TEST_ASSERT_TRUE(strcmp("Other", area_cache::get_group(0)->name) != 0);
}

void test_build_groups_device_area_fallback()
{
    DynamicJsonDocument ar_doc(256);
    const char* pairs[] = {"bedroom", "Bedroom"};
    area_cache::load_areas(make_areas(ar_doc, pairs));

    HaEntity entities[1] = {make_entity("light.bedside")};

    // Entity has no direct area but has a device
    DynamicJsonDocument er_doc(256);
    const char* er_tuples[][3] = {{"light.bedside", "", "dev_abc"}};
    area_cache::load_entity_registry(make_entity_reg(er_doc, er_tuples, 1),
                                     entities, 1);

    // Device has area "bedroom"
    DynamicJsonDocument dr_doc(256);
    const char* dr_pairs[][2] = {{"dev_abc", "bedroom"}};
    area_cache::build_groups(make_device_reg(dr_doc, dr_pairs, 1));

    TEST_ASSERT_EQUAL(1, area_cache::group_count());
    TEST_ASSERT_EQUAL_STRING("Bedroom", area_cache::get_group(0)->name);
}

void test_build_groups_device_no_area_is_dropped()
{
    HaEntity entities[1] = {make_entity("light.bedside")};

    DynamicJsonDocument er_doc(256);
    const char* er_tuples[][3] = {{"light.bedside", "", "dev_abc"}};
    area_cache::load_entity_registry(make_entity_reg(er_doc, er_tuples, 1),
                                     entities, 1);

    // Device exists but has no area assignment — entity should be dropped
    DynamicJsonDocument dr_doc(256);
    const char* dr_pairs[][2] = {{"dev_abc", ""}};
    area_cache::build_groups(make_device_reg(dr_doc, dr_pairs, 1));

    TEST_ASSERT_EQUAL(0, area_cache::group_count());
}

void test_build_groups_device_not_in_registry_is_dropped()
{
    HaEntity entities[1] = {make_entity("light.x")};

    DynamicJsonDocument er_doc(256);
    const char* er_tuples[][3] = {{"light.x", "", "dev_missing"}};
    area_cache::load_entity_registry(make_entity_reg(er_doc, er_tuples, 1),
                                     entities, 1);

    DynamicJsonDocument dr_doc(64);
    // Empty device registry — entity cannot resolve area, should be dropped
    area_cache::build_groups(dr_doc.to<JsonArray>());

    TEST_ASSERT_EQUAL(0, area_cache::group_count());
}

void test_build_groups_direct_area_wins_over_device()
{
    DynamicJsonDocument ar_doc(512);
    const char* pairs[] = {"kitchen", "Kitchen", "bedroom", "Bedroom"};
    area_cache::load_areas(make_areas(ar_doc, pairs));

    HaEntity entities[1] = {make_entity("light.x")};

    // Entity has direct area "kitchen" AND a device that points to "bedroom"
    DynamicJsonDocument er_doc(256);
    const char* er_tuples[][3] = {{"light.x", "kitchen", "dev_bed"}};
    area_cache::load_entity_registry(make_entity_reg(er_doc, er_tuples, 1),
                                     entities, 1);

    DynamicJsonDocument dr_doc(256);
    const char* dr_pairs[][2] = {{"dev_bed", "bedroom"}};
    area_cache::build_groups(make_device_reg(dr_doc, dr_pairs, 1));

    // Must use direct entity area
    TEST_ASSERT_EQUAL(1, area_cache::group_count());
    TEST_ASSERT_EQUAL_STRING("Kitchen", area_cache::get_group(0)->name);
}

void test_build_groups_entity_not_in_cache_ignored()
{
    HaEntity entities[1] = {make_entity("light.known")};

    // Entity registry has an extra entity not in our cache
    DynamicJsonDocument er_doc(512);
    const char* er_tuples[][3] = {
        {"light.known",   "living_room", ""},
        {"light.unknown", "living_room", ""},  // not in entities[]
    };
    // We don't have areas loaded, so group name will be ""
    area_cache::load_entity_registry(make_entity_reg(er_doc, er_tuples, 2),
                                     entities, 1);

    DynamicJsonDocument dr_doc(64);
    area_cache::build_groups(dr_doc.to<JsonArray>());

    // Should have 1 group with 1 entity (light.known), area unknown name = ""
    TEST_ASSERT_EQUAL(1, area_cache::group_count());
    TEST_ASSERT_EQUAL(1, area_cache::get_group(0)->count);
    TEST_ASSERT_EQUAL(0, area_cache::get_group(0)->entity_indices[0]);
}

void test_build_groups_caps_entities_per_group()
{
    DynamicJsonDocument ar_doc(256);
    const char* pairs[] = {"living_room", "Living Room"};
    area_cache::load_areas(make_areas(ar_doc, pairs));

    // More entities than MAX_ENTITIES_PER_GROUP in same area
    static const size_t N = area_cache::MAX_ENTITIES_PER_GROUP + 2;
    HaEntity entities[N];
    char eid[32];
    for (size_t i = 0; i < N; ++i) {
        snprintf(eid, sizeof(eid), "light.e%zu", i);
        entities[i] = make_entity(eid);
    }

    DynamicJsonDocument er_doc(2048);
    JsonArray er = er_doc.to<JsonArray>();
    for (size_t i = 0; i < N; ++i) {
        snprintf(eid, sizeof(eid), "light.e%zu", i);
        JsonObject o = er.createNestedObject();
        o["entity_id"] = eid;
        o["area_id"]   = "living_room";
        o["device_id"] = nullptr;
    }
    area_cache::load_entity_registry(er, entities, N);

    DynamicJsonDocument dr_doc(64);
    area_cache::build_groups(dr_doc.to<JsonArray>());

    const area_cache::EntityGroup* g = area_cache::get_group(0);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_EQUAL(area_cache::MAX_ENTITIES_PER_GROUP, g->count);
}

void test_build_groups_rebuild_clears_previous()
{
    DynamicJsonDocument ar_doc(256);
    const char* pairs[] = {"living_room", "Living Room"};
    area_cache::load_areas(make_areas(ar_doc, pairs));

    HaEntity entities[1] = {make_entity("light.a")};
    DynamicJsonDocument er_doc(256);
    const char* er_tuples[][3] = {{"light.a", "living_room", ""}};
    area_cache::load_entity_registry(make_entity_reg(er_doc, er_tuples, 1),
                                     entities, 1);

    DynamicJsonDocument dr_doc(64);
    area_cache::build_groups(dr_doc.to<JsonArray>());
    TEST_ASSERT_EQUAL(1, area_cache::group_count());

    // Build again with no entity info loaded — should clear to 0
    area_cache::init();
    DynamicJsonDocument er2(64), dr2(64);
    area_cache::load_entity_registry(er2.to<JsonArray>(), nullptr, 0);
    area_cache::build_groups(dr2.to<JsonArray>());
    TEST_ASSERT_EQUAL(0, area_cache::group_count());
}

// ============================================================================
// get_group and group_count boundary tests
// ============================================================================

void test_get_group_out_of_bounds_returns_null()
{
    DynamicJsonDocument doc(64);
    area_cache::build_groups(doc.to<JsonArray>());
    TEST_ASSERT_NULL(area_cache::get_group(0));
    TEST_ASSERT_NULL(area_cache::get_group(99));
}

void test_group_count_zero_initial()
{
    TEST_ASSERT_EQUAL(0, area_cache::group_count());
}

void test_get_area_name_unknown_returns_null()
{
    TEST_ASSERT_NULL(area_cache::get_area_name("does_not_exist"));
}

void test_build_groups_area_unknown_to_load_areas_gets_empty_name()
{
    // Area in entity reg but not in loaded areas → group created with empty name
    HaEntity entities[1] = {make_entity("light.x")};
    DynamicJsonDocument er_doc(256);
    const char* er_tuples[][3] = {{"light.x", "mystery_area", ""}};
    area_cache::load_entity_registry(make_entity_reg(er_doc, er_tuples, 1),
                                     entities, 1);

    DynamicJsonDocument dr_doc(64);
    area_cache::build_groups(dr_doc.to<JsonArray>());

    // Group exists with area_id set, but name is "" (area not in load_areas)
    TEST_ASSERT_EQUAL(1, area_cache::group_count());
    const area_cache::EntityGroup* g = area_cache::get_group(0);
    TEST_ASSERT_EQUAL_STRING("mystery_area", g->area_id);
    TEST_ASSERT_EQUAL_STRING("", g->name);
}

// ============================================================================
// main
// ============================================================================

int main()
{
    UNITY_BEGIN();

    // load_areas
    RUN_TEST(test_load_areas_empty);
    RUN_TEST(test_load_areas_single);
    RUN_TEST(test_load_areas_multiple);
    RUN_TEST(test_load_areas_unknown_id_returns_null);
    RUN_TEST(test_load_areas_skips_empty_area_id);
    RUN_TEST(test_load_areas_caps_at_max);

    // build_groups
    RUN_TEST(test_build_groups_no_entities);
    RUN_TEST(test_build_groups_entity_with_direct_area);
    RUN_TEST(test_build_groups_entity_no_area_is_dropped);
    RUN_TEST(test_build_groups_entity_not_in_registry_is_dropped);
    RUN_TEST(test_build_groups_multiple_entities_same_area);
    RUN_TEST(test_build_groups_multiple_areas);
    RUN_TEST(test_build_groups_unassigned_entity_dropped_assigned_kept);
    RUN_TEST(test_build_groups_no_other_when_all_assigned);
    RUN_TEST(test_build_groups_device_area_fallback);
    RUN_TEST(test_build_groups_device_no_area_is_dropped);
    RUN_TEST(test_build_groups_device_not_in_registry_is_dropped);
    RUN_TEST(test_build_groups_direct_area_wins_over_device);
    RUN_TEST(test_build_groups_entity_not_in_cache_ignored);
    RUN_TEST(test_build_groups_caps_entities_per_group);
    RUN_TEST(test_build_groups_rebuild_clears_previous);

    // boundary / accessors
    RUN_TEST(test_get_group_out_of_bounds_returns_null);
    RUN_TEST(test_group_count_zero_initial);
    RUN_TEST(test_get_area_name_unknown_returns_null);
    RUN_TEST(test_build_groups_area_unknown_to_load_areas_gets_empty_name);

    return UNITY_END();
}
