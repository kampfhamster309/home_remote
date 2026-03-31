#pragma once

#include <ArduinoJson.h>
#include "ha_entity.h"

// Area-based grouping of entities.
//
// Lifecycle:
//   area_cache::init()                   — called once in setup()
//   area_cache::load_areas()             — called from AreaListCallback
//   area_cache::load_entity_registry()   — called from EntityRegCallback
//   area_cache::build_groups()           — called from DeviceRegCallback; finalises groups
//
// Grouping strategy:
//   1. Entity has a direct area assignment in the entity registry → use it.
//   2. Entity has no direct area but belongs to a device that has one → use device's area.
//   3. Otherwise → entity is silently dropped (not shown in the UI).

namespace area_cache {

// Max entities shown per group.
// CLAUDE.md: "Max ~4–5 devices per room." 8 gives headroom without excess BSS.
static constexpr size_t MAX_ENTITIES_PER_GROUP = 8;

struct EntityGroup {
    char   area_id[32];
    char   name[64];
    size_t entity_indices[MAX_ENTITIES_PER_GROUP]; // indices into entity_cache
    size_t count;
};

// Reset all internal state.
void init();

// Load the area table from config/area_registry/list result.
void load_areas(const JsonArray& areas);

// Build entity→area mapping from config/entity_registry/list result.
// Only entities present in `entities[0..entity_count)` are tracked.
// Call before build_groups().
void load_entity_registry(const JsonArray& entries,
                          const HaEntity* entities,
                          size_t entity_count);

// Finalise group layout using config/device_registry/list result.
// Entities with no direct area inherit from their device's area.
// Entities with no area after both lookups are silently dropped.
void build_groups(const JsonArray& device_reg);

// Area name for the given area_id, or nullptr if unknown.
const char* get_area_name(const char* area_id);

// Group at position index (0-based), or nullptr if out of range.
const EntityGroup* get_group(size_t index);

// Total number of groups.
size_t group_count();

} // namespace area_cache
