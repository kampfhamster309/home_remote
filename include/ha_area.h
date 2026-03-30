#pragma once

#include <cstddef>

// An HA area (room) as returned by config/area_registry/list.
struct HaArea {
    char area_id[32];
    char name[64];
};

// Maximum number of HA areas (rooms) the area cache will track.
// Kept at 12 to fit within ESP32 DRAM BSS limits alongside entity_cache.
static constexpr size_t MAX_AREAS = 12;
