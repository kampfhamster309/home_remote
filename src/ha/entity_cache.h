#pragma once

#include <ArduinoJson.h>
#include "ha_entity.h"

// In-memory cache of HA entity states.
//
// Lifecycle:
//   entity_cache::init()     — called once in setup(), registers the change callback
//   entity_cache::populate() — called from ha_client StateListCallback (get_states result)
//   entity_cache::update()   — called from ha_client StateChangedCallback (live push)
//
// The cache holds up to MAX_ENTITIES entries in a flat static array.
// Entities beyond that limit are silently dropped.

namespace entity_cache {

// Called whenever a cached entity's state or attributes change.
// The reference is valid only for the duration of the callback.
using EntityChangedCallback = void (*)(const HaEntity& entity);

// Initialise: reset the cache and store the change callback.
void init(EntityChangedCallback on_changed);

// Populate the cache from the full get_states JsonArray.
// Clears any previously cached data first.
void populate(const JsonArray& states);

// Update a single entity from an incoming state_changed new_state object.
// No-op if the entity_id is not in the cache.
void update(const char* entity_id, const JsonObject& new_state);

// Return a pointer to the cached entity with the given entity_id, or nullptr.
const HaEntity* find(const char* entity_id);

// Return the entity at position index (0-based), or nullptr if out of range.
const HaEntity* get(size_t index);

// Number of entities currently in the cache.
size_t count();

} // namespace entity_cache
