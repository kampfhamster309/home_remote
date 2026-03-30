#pragma once

#include <ArduinoJson.h>

// Home Assistant client.
//
// Transport layer only — does not own the entity cache or area grouping.
//
// WebSocket path:  ws://<ha-host>:<port>/api/websocket
//   Startup sequence:
//     1. auth_required → auth → auth_ok
//     2. get_states        → StateListCallback
//     3. area_registry     → AreaListCallback
//     4. entity_registry   → EntityRegCallback
//     5. device_registry   → DeviceRegCallback
//     6. subscribe_events  → StateChangedCallback on each state_changed event
//
// REST path: POST /api/services/<domain>/<service>
//   Used for outgoing control commands (fire-and-forget).

namespace ha_client {

// Called once when the initial get_states result arrives.
// `states` is a JsonArray of state objects (entity_id, state, attributes).
// The document backing this array is valid only during the callback.
using StateListCallback = void (*)(const JsonArray& states);

// Called for every incoming state_changed event.
// `entity_id` and `new_state` are valid only during the callback.
using StateChangedCallback = void (*)(const char* entity_id,
                                      const JsonObject& new_state);

// Called once with the config/area_registry/list result.
// `areas` is valid only during the callback.
using AreaListCallback = void (*)(const JsonArray& areas);

// Called once with the config/entity_registry/list result.
// `entries` is valid only during the callback.
using EntityRegCallback = void (*)(const JsonArray& entries);

// Called once with the config/device_registry/list result.
// `devices` is valid only during the callback.
using DeviceRegCallback = void (*)(const JsonArray& devices);

// Initialise: parse HA URL from NVS config, store callbacks.
// Does not open the WebSocket connection yet — call tick() to drive it.
// Registry callbacks (on_areas, on_entity_reg, on_device_reg) may be nullptr.
void init(StateListCallback    on_states,
          StateChangedCallback on_state_changed,
          AreaListCallback     on_areas,
          EntityRegCallback    on_entity_reg,
          DeviceRegCallback    on_device_reg);

// Drive the WebSocket event loop and reconnect state machine.
// Must be called from the Arduino main loop as frequently as possible.
void tick();

// Call a HA service via REST.
// domain   : e.g. "light", "switch", "cover"
// service  : e.g. "turn_on", "turn_off", "toggle"
// entity_id: e.g. "light.living_room"
// Returns true on HTTP 200/201.
bool call_service(const char* domain,
                  const char* service,
                  const char* entity_id);

// Variant that merges extra JSON fields into the service call body.
// Useful for e.g. {"brightness": 128} when turning on a dimmable light.
bool call_service_ex(const char* domain,
                     const char* service,
                     const char* entity_id,
                     const JsonObject& extra);

// True if the WebSocket is authenticated and state_changed events are flowing.
bool is_connected();

}  // namespace ha_client
