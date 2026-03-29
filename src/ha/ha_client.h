#pragma once

#include <ArduinoJson.h>

// Home Assistant client.
//
// Transport layer only — does not own the entity cache.
// TICKET-005 will provide the actual callback implementations.
//
// WebSocket path:  ws://<ha-host>:<port>/api/websocket
//   - Authenticates with long-lived token from NVS
//   - Fetches all states once on connect via get_states
//   - Subscribes to state_changed events for live push updates
//
// REST path: POST /api/services/<domain>/<service>
//   - Used for outgoing control commands (toggle, turn_on, etc.)
//   - Fire-and-forget; result arrives as a state_changed event

namespace ha_client {

// Called once when the initial get_states result arrives.
// `states` is a JsonArray of state objects (entity_id, state, attributes).
// The document backing this array is valid only during the callback.
using StateListCallback = void (*)(const JsonArray& states);

// Called for every incoming state_changed event.
// `entity_id` and `new_state` are valid only during the callback.
using StateChangedCallback = void (*)(const char* entity_id,
                                      const JsonObject& new_state);

// Initialise: parse HA URL from NVS config, store callbacks.
// Does not open the WebSocket connection yet — call tick() to drive it.
void init(StateListCallback on_states, StateChangedCallback on_state_changed);

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
