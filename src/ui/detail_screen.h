#pragma once

#include "ha_entity.h"

// Returns true if this entity has continuous attributes (brightness, target
// temperature, cover position) that warrant a detail control screen.
// Defined inline here so it can be tested on native without pulling in LVGL.
inline bool has_detail(const HaEntity& e)
{
    if (e.domain == EntityDomain::LIGHT &&
        (e.attrs.has_brightness || e.attrs.has_color_temp))
        return true;
    if (e.domain == EntityDomain::CLIMATE && e.attrs.has_target_temp)
        return true;
    if (e.domain == EntityDomain::COVER && e.attrs.has_position)
        return true;
    return false;
}

// Device detail / control screen.
//
// Opened by long-pressing a tile whose entity has continuous attributes.
// Shows a slider per controllable attribute; changes are sent to HA on finger
// release (debounced). A back button returns to the group screen.
//
// Lifecycle:
//   Long-press on tile  →  detail_screen::open(entity_id)
//   User presses back   →  screen fades out and detail screen is deleted
//   HA push update      →  detail_screen::on_entity_changed(entity) updates sliders

namespace detail_screen {

// Open the detail screen for the given entity.
// No-op if the entity has no detail content (use has_detail() to check first,
// or let open() silently discard unsupported entities).
void open(const char* entity_id);

// Update slider positions when a state_changed event confirms new attribute
// values. No-op if the detail screen is not open or entity_id doesn't match.
void on_entity_changed(const HaEntity& entity);

// True while the detail screen is active.
bool is_open();

} // namespace detail_screen
