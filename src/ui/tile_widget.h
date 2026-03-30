#pragma once

#include <lvgl.h>
#include "ha_entity.h"

// Device tile widget.
//
// Each tile represents one HA entity.  It shows a domain icon, the entity's
// friendly name, and (for sensors/climate) its current state value.  The tile
// background colour reflects on/off/unavailable state.  Tapping a tile fires
// the appropriate HA service call and briefly shows a "pending" colour while
// waiting for the confirmed state_changed event to arrive.
//
// Lifecycle:
//   tile_widget::create()   — called by shell::build_tile_grid() for each entity
//   tile_widget::update()   — called by shell::on_entity_changed() on state push
//   tile_widget::entity_id() — used by on_entity_changed() to identify the tile
//
// Memory: create() allocates a small TileUD struct on the heap (freed automatically
// via LV_EVENT_DELETE when the tile is destroyed).

namespace tile_widget {

// Create a tile at pixel position (x, y) with size (w × h) inside parent.
// The tile stores entity.entity_id and calls ha_client::call_service() on tap.
// Returns the lv_btn object.
lv_obj_t* create(lv_obj_t* parent, const HaEntity& entity, int x, int y, int w, int h);

// Refresh the tile's colour and state text from the entity's current state.
// Must be called with the tile returned by create() and the updated entity.
void update(lv_obj_t* tile, const HaEntity& entity);

// Return the entity_id stored in this tile, or nullptr if tile is not a tile widget.
const char* entity_id(lv_obj_t* tile);

} // namespace tile_widget
