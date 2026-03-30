#pragma once

#include <cstdint>
#include <cstddef>

// Maximum number of HA entities held in the in-memory cache.
// Each HaEntity is ~192 bytes → 48 entities ≈ 9 KB static RAM.
// Sized to leave sufficient BSS headroom on the ESP32's 320 KB DRAM.
static constexpr size_t MAX_ENTITIES = 48;

// HA entity domain, derived from the entity_id prefix (e.g. "light.*").
enum class EntityDomain : uint8_t {
    LIGHT,
    SWITCH,
    COVER,
    CLIMATE,
    SENSOR,
    BINARY_SENSOR,
    AUTOMATION,
    SCRIPT,
    SCENE,
    INPUT_BOOLEAN,
    MEDIA_PLAYER,
    FAN,
    LOCK,
    UNKNOWN,
};

// Parsed subset of HA entity attributes.
// Only fields relevant to the supported domains are stored.
struct HaAttributes {
    uint8_t  brightness;      // LIGHT: 0–255
    uint16_t color_temp;      // LIGHT: mireds
    uint8_t  position;        // COVER: current_position 0–100
    float    temperature;     // CLIMATE/SENSOR: current_temperature
    float    target_temp;     // CLIMATE: temperature setpoint

    bool has_brightness    : 1;
    bool has_color_temp    : 1;
    bool has_position      : 1;
    bool has_temperature   : 1;
    bool has_target_temp   : 1;
};

// A single HA entity as stored in the cache.
struct HaEntity {
    char         entity_id[64];     // e.g. "light.living_room"
    char         friendly_name[64]; // from attributes.friendly_name; falls back to entity_id
    char         state[32];         // e.g. "on", "off", "unavailable", "23.5"
    EntityDomain domain;
    HaAttributes attrs;
};
