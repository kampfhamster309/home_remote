#pragma once

#include <lvgl.h>
#include <cstddef>
#include "ha_entity.h"

// Main application shell.
//
// Lifecycle:
//   shell::show_loading()           — call in setup() after wifi_manager::connect()
//   shell::create()                 — call from on_ha_device_registry() callback
//   shell::update_status(w, ha)     — call every loop() iteration
//
// Screen layout (320 × 240):
//   ┌──────────────────────────────────┐  y=0
//   │  [Room Name]           ● ●       │  header 36 px
//   ├──────────────────────────────────┤  y=36
//   │                                  │
//   │          content area            │  160 px  ← TICKET-009 adds tiles here
//   │                                  │
//   ├──────────────────────────────────┤  y=196
//   │  [Room1]  [Room2]  [Room3] ...   │  nav-bar 44 px
//   └──────────────────────────────────┘  y=240

namespace shell {

// Show a "Connecting to Home Assistant…" screen while the WS startup sequence runs.
// The screen is automatically replaced when create() is called.
void show_loading();

// Build (or rebuild) the shell from the current area_cache groups.
// Must be called after area_cache::build_groups(). Fades in over the loading screen.
void create();

// Update the WiFi and HA status dots in the header.
// Safe to call before create() — no-op until shell exists.
void update_status(bool wifi_connected, bool ha_connected);

// LVGL object for the content area of the currently active group.
// Returns nullptr when no shell exists.
// TICKET-009 uses this to populate room tiles.
lv_obj_t* get_content();

// Index of the currently displayed group.
size_t active_group();

// Called from main.cpp's on_entity_changed callback.
// Finds the matching tile in the active content area and refreshes its state.
// No-op if the entity is not visible (different group or shell not built yet).
void on_entity_changed(const HaEntity& entity);

} // namespace shell
