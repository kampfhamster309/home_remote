#include "ha_client.h"

#include <Arduino.h>
#include <WebSocketsClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "../../include/url_parse.h"
#include "../config/nvs_config.h"

// ----------------------------------------------------------------------------
// Internal state
// ----------------------------------------------------------------------------

namespace {

enum class HaState {
    DISCONNECTED,
    AUTHENTICATING,
    FETCHING_STATES,
    SUBSCRIBED,
};

// WebSocket client
static WebSocketsClient s_ws;

// Reconnect backoff: 2 s → 4 s → 8 s → 16 s → 32 s → 64 s (then stays)
static const uint32_t BACKOFF_MS[] = {2000, 4000, 8000, 16000, 32000, 64000};
static constexpr size_t BACKOFF_COUNT = sizeof(BACKOFF_MS) / sizeof(BACKOFF_MS[0]);
static size_t   s_backoff_idx  = 0;
static uint32_t s_reconnect_at = 0;

// Message ID counter (HA WS protocol requires incrementing IDs)
static uint32_t s_msg_id = 0;

static HaState s_state = HaState::DISCONNECTED;

// Parsed HA URL
static ParsedUrl s_url;

// Token from NVS (copy, since NetworkConfig is local to init)
static char s_token[384];

// User callbacks
static ha_client::StateListCallback    s_on_states       = nullptr;
static ha_client::StateChangedCallback s_on_state_changed = nullptr;

// JSON documents.  DynamicJsonDocument keeps the buffers on the heap (not BSS),
// which is necessary to stay within ESP32's DRAM segment limits.
// 24 KB for the full get_states response; 4 KB for event/auth messages.
static DynamicJsonDocument s_states_doc(24576);
static DynamicJsonDocument s_event_doc(4096);

// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

static void send_json(const JsonDocument& doc)
{
    String out;
    serializeJson(doc, out);
    s_ws.sendTXT(out);
}

static void send_auth()
{
    StaticJsonDocument<512> doc;
    doc["type"]        = "auth";
    doc["access_token"] = s_token;
    send_json(doc);
}

static void send_get_states()
{
    StaticJsonDocument<64> doc;
    doc["id"]   = ++s_msg_id;
    doc["type"] = "get_states";
    send_json(doc);
}

static void send_subscribe_events()
{
    StaticJsonDocument<64> doc;
    doc["id"]         = ++s_msg_id;
    doc["type"]       = "subscribe_events";
    doc["event_type"] = "state_changed";
    send_json(doc);
}

// ----------------------------------------------------------------------------
// Incoming message handler
// ----------------------------------------------------------------------------

static void handle_message(const char* payload, size_t length)
{
    s_event_doc.clear();
    DeserializationError err = deserializeJson(s_event_doc, payload, length);
    if (err) return; // malformed JSON — ignore

    const char* type = s_event_doc["type"] | "";

    if (strcmp(type, "auth_required") == 0) {
        // HA greets us — respond with token
        send_auth();
        s_state = HaState::AUTHENTICATING;
        return;
    }

    if (strcmp(type, "auth_ok") == 0) {
        // Authenticated — fetch all states
        send_get_states();
        s_state = HaState::FETCHING_STATES;
        return;
    }

    if (strcmp(type, "auth_invalid") == 0) {
        // Bad token — disconnect and stop retrying immediately
        s_ws.disconnect();
        s_state          = HaState::DISCONNECTED;
        s_backoff_idx    = BACKOFF_COUNT - 1; // max backoff
        s_reconnect_at   = millis() + BACKOFF_MS[s_backoff_idx];
        return;
    }

    if (strcmp(type, "result") == 0) {
        // Response to get_states (our first request with id=1)
        if (s_state != HaState::FETCHING_STATES) return;
        bool success = s_event_doc["success"] | false;
        if (!success) return;

        // Re-parse into the larger states document.
        // s_event_doc (4 KB) is too small for a full get_states response;
        // s_states_doc (24 KB) is sized to hold the complete state list.
        s_states_doc.clear();
        deserializeJson(s_states_doc, payload, length);
        JsonArray result = s_states_doc["result"].as<JsonArray>();
        if (result.isNull()) return;

        if (s_on_states) {
            s_on_states(result);
        }

        // Now subscribe to live events
        send_subscribe_events();
        s_state = HaState::SUBSCRIBED;
        return;
    }

    if (strcmp(type, "event") == 0) {
        if (s_state != HaState::SUBSCRIBED) return;

        const char* event_type =
            s_event_doc["event"]["event_type"] | "";
        if (strcmp(event_type, "state_changed") != 0) return;

        const char* entity_id =
            s_event_doc["event"]["data"]["entity_id"] | "";
        JsonObject new_state =
            s_event_doc["event"]["data"]["new_state"].as<JsonObject>();

        if (entity_id[0] != '\0' && !new_state.isNull() && s_on_state_changed) {
            s_on_state_changed(entity_id, new_state);
        }
    }
}

// ----------------------------------------------------------------------------
// WebSocket event callback
// ----------------------------------------------------------------------------

static void on_ws_event(WStype_t type, uint8_t* payload, size_t length)
{
    switch (type) {
        case WStype_CONNECTED:
            s_state       = HaState::AUTHENTICATING;
            s_backoff_idx = 0; // reset backoff on successful connect
            // HA sends auth_required immediately — we wait for it in handle_message
            break;

        case WStype_DISCONNECTED:
            s_state        = HaState::DISCONNECTED;
            s_reconnect_at = millis() +
                             BACKOFF_MS[s_backoff_idx < BACKOFF_COUNT ? s_backoff_idx++ : BACKOFF_COUNT - 1];
            break;

        case WStype_TEXT:
            if (payload && length > 0) {
                handle_message(reinterpret_cast<const char*>(payload), length);
            }
            break;

        default:
            break;
    }
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

namespace ha_client {

void init(StateListCallback on_states, StateChangedCallback on_state_changed)
{
    s_on_states        = on_states;
    s_on_state_changed = on_state_changed;
    s_state            = HaState::DISCONNECTED;
    s_msg_id           = 0;
    s_backoff_idx      = 0;
    s_reconnect_at     = 0;

    NetworkConfig cfg{};
    if (!nvs_config::load_net_config(cfg)) return; // no config yet

    s_url = parse_ha_url(cfg.ha_url);
    if (!s_url.valid) return;

    // Copy token so it outlives cfg
    strncpy(s_token, cfg.ha_token, sizeof(s_token) - 1);
    s_token[sizeof(s_token) - 1] = '\0';

    // Set up WebSocket
    s_ws.onEvent(on_ws_event);
    s_ws.setReconnectInterval(0); // we manage reconnect ourselves

    const char* ws_path = "/api/websocket";
    if (s_url.secure) {
        s_ws.beginSSL(s_url.host, s_url.port, ws_path);
    } else {
        s_ws.begin(s_url.host, s_url.port, ws_path);
    }

    // Trigger first connect immediately
    s_reconnect_at = millis();
}

void tick()
{
    if (!s_url.valid) return;

    if (s_state == HaState::DISCONNECTED) {
        if (millis() >= s_reconnect_at) {
            s_ws.begin(s_url.host, s_url.port, "/api/websocket");
        }
        return;
    }

    s_ws.loop();
}

bool call_service(const char* domain,
                  const char* service,
                  const char* entity_id)
{
    StaticJsonDocument<128> extra;
    return call_service_ex(domain, service, entity_id, extra.as<JsonObject>());
}

bool call_service_ex(const char* domain,
                     const char* service,
                     const char* entity_id,
                     const JsonObject& extra)
{
    if (!s_url.valid) return false;

    HTTPClient http;

    // Build URL: http[s]://<host>:<port>/api/services/<domain>/<service>
    String url = s_url.secure ? "https://" : "http://";
    url += s_url.host;
    url += ":";
    url += s_url.port;
    url += "/api/services/";
    url += domain;
    url += "/";
    url += service;

    http.begin(url);
    http.addHeader("Authorization", String("Bearer ") + s_token);
    http.addHeader("Content-Type", "application/json");

    // Build body: { "entity_id": "...", ...extra }
    StaticJsonDocument<512> body_doc;
    body_doc["entity_id"] = entity_id;
    for (JsonPair kv : extra) {
        body_doc[kv.key()] = kv.value();
    }

    String body;
    serializeJson(body_doc, body);

    int code = http.POST(body);
    http.end();

    return (code == 200 || code == 201);
}

bool is_connected()
{
    return s_state == HaState::SUBSCRIBED;
}

} // namespace ha_client
