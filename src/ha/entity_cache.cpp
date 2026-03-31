#include "entity_cache.h"
#include <cstring>

// ----------------------------------------------------------------------------
// Internal storage
// ----------------------------------------------------------------------------

namespace {

static HaEntity s_entities[MAX_ENTITIES];
static size_t   s_count      = 0;
static entity_cache::EntityChangedCallback s_on_changed = nullptr;

// ----------------------------------------------------------------------------
// Domain parsing
// ----------------------------------------------------------------------------

EntityDomain domain_from_id(const char* entity_id)
{
    const char* dot = strchr(entity_id, '.');
    if (!dot) return EntityDomain::UNKNOWN;
    const size_t len = static_cast<size_t>(dot - entity_id);

    struct Entry { const char* name; EntityDomain domain; };
    static const Entry table[] = {
        { "light",         EntityDomain::LIGHT         },
        { "switch",        EntityDomain::SWITCH        },
        { "cover",         EntityDomain::COVER         },
        { "climate",       EntityDomain::CLIMATE       },
        { "sensor",        EntityDomain::SENSOR        },
        { "binary_sensor", EntityDomain::BINARY_SENSOR },
        { "automation",    EntityDomain::AUTOMATION    },
        { "script",        EntityDomain::SCRIPT        },
        { "scene",         EntityDomain::SCENE         },
        { "input_boolean", EntityDomain::INPUT_BOOLEAN },
        { "media_player",  EntityDomain::MEDIA_PLAYER  },
        { "fan",           EntityDomain::FAN           },
        { "lock",          EntityDomain::LOCK          },
        { "weather",       EntityDomain::WEATHER       },
    };

    for (const auto& e : table) {
        if (strlen(e.name) == len && strncmp(entity_id, e.name, len) == 0) {
            return e.domain;
        }
    }
    return EntityDomain::UNKNOWN;
}

// ----------------------------------------------------------------------------
// Attribute parsing
// ----------------------------------------------------------------------------

static void parse_attrs(HaEntity& ent, JsonObjectConst attrs_obj)
{
    ent.attrs = {};

    // friendly_name — fall back to entity_id if absent
    const char* fname = attrs_obj["friendly_name"] | "";
    if (fname[0] != '\0') {
        strncpy(ent.friendly_name, fname, sizeof(ent.friendly_name) - 1);
    } else {
        strncpy(ent.friendly_name, ent.entity_id, sizeof(ent.friendly_name) - 1);
    }
    ent.friendly_name[sizeof(ent.friendly_name) - 1] = '\0';

    if (attrs_obj.containsKey("brightness")) {
        ent.attrs.brightness     = attrs_obj["brightness"].as<uint8_t>();
        ent.attrs.has_brightness = true;
    }
    if (attrs_obj.containsKey("color_temp")) {
        ent.attrs.color_temp     = attrs_obj["color_temp"].as<uint16_t>();
        ent.attrs.has_color_temp = true;
    }
    if (attrs_obj.containsKey("current_position")) {
        ent.attrs.position     = attrs_obj["current_position"].as<uint8_t>();
        ent.attrs.has_position = true;
    }
    if (attrs_obj.containsKey("current_temperature")) {
        ent.attrs.temperature     = attrs_obj["current_temperature"].as<float>();
        ent.attrs.has_temperature = true;
    }
    if (attrs_obj.containsKey("temperature")) {
        ent.attrs.target_temp     = attrs_obj["temperature"].as<float>();
        ent.attrs.has_target_temp = true;
    }
}

// ----------------------------------------------------------------------------
// Fill one entity from a state JSON object
// ----------------------------------------------------------------------------

static void fill_entity(HaEntity& ent, JsonObjectConst state_obj)
{
    // entity_id
    const char* eid = state_obj["entity_id"] | "";
    strncpy(ent.entity_id, eid, sizeof(ent.entity_id) - 1);
    ent.entity_id[sizeof(ent.entity_id) - 1] = '\0';

    // state
    const char* st = state_obj["state"] | "unknown";
    strncpy(ent.state, st, sizeof(ent.state) - 1);
    ent.state[sizeof(ent.state) - 1] = '\0';

    // domain
    ent.domain = domain_from_id(ent.entity_id);

    // attributes (friendly_name + domain-specific fields)
    parse_attrs(ent, state_obj["attributes"].as<JsonObjectConst>());
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

namespace entity_cache {

void init(EntityChangedCallback on_changed)
{
    s_on_changed = on_changed;
    s_count      = 0;
    memset(s_entities, 0, sizeof(s_entities));
}

void populate(const JsonArray& states)
{
    s_count = 0;
    for (JsonVariantConst v : states) {
        if (s_count >= MAX_ENTITIES) break;
        JsonObjectConst obj = v.as<JsonObjectConst>();
        fill_entity(s_entities[s_count], obj);
        // Skip entries that arrived with no entity_id
        if (s_entities[s_count].entity_id[0] != '\0') {
            ++s_count;
        }
    }
}

void update(const char* entity_id, const JsonObject& new_state)
{
    for (size_t i = 0; i < s_count; ++i) {
        if (strcmp(s_entities[i].entity_id, entity_id) == 0) {
            fill_entity(s_entities[i], JsonObjectConst(new_state));
            if (s_on_changed) {
                s_on_changed(s_entities[i]);
            }
            return;
        }
    }
    // Entity not found in cache — silently ignore
    // (could be beyond MAX_ENTITIES or from an unsupported domain)
}

const HaEntity* find(const char* entity_id)
{
    for (size_t i = 0; i < s_count; ++i) {
        if (strcmp(s_entities[i].entity_id, entity_id) == 0) {
            return &s_entities[i];
        }
    }
    return nullptr;
}

const HaEntity* get(size_t index)
{
    if (index >= s_count) return nullptr;
    return &s_entities[index];
}

size_t count()
{
    return s_count;
}

const HaEntity* data()
{
    return s_entities;
}

} // namespace entity_cache
