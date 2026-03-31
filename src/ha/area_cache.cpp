#include "area_cache.h"

#include <cstring>

#include "ha_area.h"

// ----------------------------------------------------------------------------
// Internal storage
// ----------------------------------------------------------------------------

namespace {

// Area table (from config/area_registry/list)
static HaArea  s_areas[MAX_AREAS];
static size_t  s_area_count = 0;

// Per-entity registry info, filtered to only entities in the cache.
// Stores area_id (direct entity assignment) and device_id (for fallback lookup).
// Heap-allocated in init() to avoid inflating BSS — 48 entries × 136 bytes = 6.5 KB.
struct EntityAreaInfo {
    char         entity_id[64];
    char         area_id[32];    // "" if entity has no direct area assignment
    char         device_id[40];  // "" if entity has no device
    EntityDomain domain;         // copied from entity_cache; used to skip display-only domains
};
static EntityAreaInfo* s_entity_info       = nullptr;
static size_t          s_entity_info_count = 0;

// Named-area groups (one per distinct area_id found in entity registry)
static area_cache::EntityGroup s_groups[MAX_AREAS];
static size_t                  s_group_count = 0;

// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

static int find_area_idx(const char* area_id)
{
    for (size_t i = 0; i < s_area_count; ++i) {
        if (strcmp(s_areas[i].area_id, area_id) == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// Find or create a named group for the given area.
// Returns index into s_groups, or -1 if the table is full.
static int find_or_create_group(const char* area_id, const char* area_name)
{
    for (size_t i = 0; i < s_group_count; ++i) {
        if (strcmp(s_groups[i].area_id, area_id) == 0) {
            return static_cast<int>(i);
        }
    }
    if (s_group_count >= MAX_AREAS) return -1;

    area_cache::EntityGroup& g = s_groups[s_group_count];
    g = {};
    strncpy(g.area_id, area_id,   sizeof(g.area_id)  - 1);
    strncpy(g.name,    area_name, sizeof(g.name)      - 1);
    return static_cast<int>(s_group_count++);
}

static void add_to_group(area_cache::EntityGroup& g, size_t entity_index)
{
    if (g.count < area_cache::MAX_ENTITIES_PER_GROUP) {
        g.entity_indices[g.count++] = entity_index;
    }
}

// Resolve effective area_id for an EntityAreaInfo entry,
// using device_reg as fallback for entities with no direct area.
// Returns nullptr if no area can be determined.
static const char* resolve_area(const EntityAreaInfo& info,
                                const JsonArray&      device_reg)
{
    if (info.area_id[0] != '\0') {
        return info.area_id;
    }

    if (info.device_id[0] == '\0') {
        return nullptr;
    }

    // Scan device registry for this device's area
    for (JsonVariantConst v : device_reg) {
        JsonObjectConst obj     = v.as<JsonObjectConst>();
        const char*     dev_id  = obj["id"]      | "";
        const char*     area_id = obj["area_id"] | "";
        if (strcmp(dev_id, info.device_id) == 0) {
            return (area_id[0] != '\0') ? area_id : nullptr;
        }
    }
    return nullptr;
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

namespace area_cache {

void init()
{
    // Allocate entity-info table on the heap the first time; reuse on subsequent calls.
    // 48 entries × 136 bytes = 6.5 KB heap vs BSS — keeps us within DRAM segment limits.
    if (!s_entity_info) {
        s_entity_info = new EntityAreaInfo[MAX_ENTITIES]();
    }

    s_area_count        = 0;
    s_entity_info_count = 0;
    s_group_count       = 0;
    memset(s_areas,  0, sizeof(s_areas));
    memset(s_groups, 0, sizeof(s_groups));
}

void load_areas(const JsonArray& areas)
{
    s_area_count = 0;
    for (JsonVariantConst v : areas) {
        if (s_area_count >= MAX_AREAS) break;
        JsonObjectConst obj     = v.as<JsonObjectConst>();
        const char*     area_id = obj["area_id"] | "";
        const char*     name    = obj["name"]    | "";
        if (area_id[0] == '\0') continue;
        strncpy(s_areas[s_area_count].area_id, area_id, sizeof(HaArea::area_id) - 1);
        strncpy(s_areas[s_area_count].name,    name,    sizeof(HaArea::name)    - 1);
        ++s_area_count;
    }
}

void load_entity_registry(const JsonArray& entries,
                          const HaEntity* entities,
                          size_t entity_count)
{
    // Pre-populate every cached entity with empty area/device so that entities
    // absent from the registry are still tracked and land in "Other".
    s_entity_info_count = 0;
    const size_t cap = (entity_count < MAX_ENTITIES) ? entity_count : MAX_ENTITIES;
    for (size_t i = 0; i < cap; ++i) {
        EntityAreaInfo& info = s_entity_info[s_entity_info_count++];
        memset(&info, 0, sizeof(info));
        strncpy(info.entity_id, entities[i].entity_id, sizeof(info.entity_id) - 1);
        info.domain = entities[i].domain;
    }

    // Update area_id / device_id for entities found in the registry.
    for (JsonVariantConst v : entries) {
        JsonObjectConst obj     = v.as<JsonObjectConst>();
        const char*     eid     = obj["entity_id"] | "";
        const char*     area_id = obj["area_id"]   | "";
        const char*     dev_id  = obj["device_id"] | "";

        if (eid[0] == '\0') continue;

        for (size_t i = 0; i < s_entity_info_count; ++i) {
            if (strcmp(s_entity_info[i].entity_id, eid) == 0) {
                strncpy(s_entity_info[i].area_id,   area_id, sizeof(EntityAreaInfo::area_id)   - 1);
                strncpy(s_entity_info[i].device_id, dev_id,  sizeof(EntityAreaInfo::device_id) - 1);
                break;
            }
        }
    }
}

void build_groups(const JsonArray& device_reg)
{
    // Reset group state
    s_group_count = 0;
    memset(s_groups, 0, sizeof(s_groups));

    for (size_t i = 0; i < s_entity_info_count; ++i) {
        const EntityAreaInfo& info = s_entity_info[i];

        // Weather entities are display-only (shown in weather tab, not room tiles).
        if (info.domain == EntityDomain::WEATHER) continue;

        // Resolve area (direct assignment, then device fallback)
        const char* effective_area = resolve_area(info, device_reg);

        if (effective_area == nullptr) {
            // No area found → drop this entity
            continue;
        }

        // Look up the human-readable name for this area
        const char* area_name = "";
        int ai = find_area_idx(effective_area);
        if (ai >= 0) area_name = s_areas[ai].name;

        // Find or create the named group; overflow → drop
        int gi = find_or_create_group(effective_area, area_name);
        if (gi >= 0) {
            add_to_group(s_groups[gi], i);
        }
    }
}

const char* get_area_name(const char* area_id)
{
    int ai = find_area_idx(area_id);
    return (ai >= 0) ? s_areas[ai].name : nullptr;
}

const EntityGroup* get_group(size_t index)
{
    if (index < s_group_count) return &s_groups[index];
    return nullptr;
}

size_t group_count()
{
    return s_group_count;
}

} // namespace area_cache
