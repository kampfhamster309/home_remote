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
    FETCHING_AREAS,
    FETCHING_ENTITY_REG,
    FETCHING_DEVICE_REG,
    SUBSCRIBED,
};

static WebSocketsClient s_ws;

// Reconnect backoff: 2 s → 4 s → 8 s → 16 s → 32 s → 64 s (then stays)
static const uint32_t BACKOFF_MS[] = {2000, 4000, 8000, 16000, 32000, 64000};
static constexpr size_t BACKOFF_COUNT = sizeof(BACKOFF_MS) / sizeof(BACKOFF_MS[0]);
static size_t   s_backoff_idx  = 0;
static uint32_t s_reconnect_at = 0;

// Message ID counter (HA WS protocol requires incrementing IDs)
static uint32_t s_msg_id = 0;

static HaState s_state = HaState::DISCONNECTED;

static ParsedUrl s_url;
static char      s_token[256]; // HA long-lived tokens are ~183 chars; 256 is safe

// User callbacks
static ha_client::StateListCallback      s_on_states        = nullptr;
static ha_client::StateChangedCallback   s_on_state_changed = nullptr;
static ha_client::AreaListCallback       s_on_areas         = nullptr;
static ha_client::EntityRegCallback      s_on_entity_reg    = nullptr;
static ha_client::DeviceRegCallback      s_on_device_reg    = nullptr;
static ha_client::WeatherForecastCallback s_on_weather      = nullptr;

// Pending weather request tracking
static uint32_t s_weather_msg_id = 0;              // 0 = no pending request
static char     s_weather_entity_id[64] = {};      // entity_id that was requested

// Working document for parsed content.
// Allocated in init() (not at global construction time) because large
// malloc() calls in global constructors fail on ESP32 before the heap
// is fully set up by the FreeRTOS runtime.
static DynamicJsonDocument* s_doc = nullptr;

// ----------------------------------------------------------------------------
// Per-state ArduinoJson filter documents (DynamicJsonDocument — heap, not BSS).
//
// Key insight: HA WS "result" messages wrap their payload like:
//   {"type":"result","success":true,"result":[...]}
// So all result filters must descend through ["result"][0][...].
// [0] in an ArduinoJson filter is a TEMPLATE that applies to every element.
// ----------------------------------------------------------------------------

// get_states: only the fields entity_cache.populate() uses
static DynamicJsonDocument s_flt_states(512);

// config/area_registry/list: area_id + name only
static DynamicJsonDocument s_flt_areas(128);

// config/entity_registry/list: three fields for area resolution
static DynamicJsonDocument s_flt_ereg(192);

// config/device_registry/list: id + area_id for device→area resolution
static DynamicJsonDocument s_flt_dreg(128);

// state_changed events: filtered to just the fields entity_cache.update() uses
static DynamicJsonDocument s_flt_event(512);

// weather.get_forecasts response: keep the entire result.response subtree.
// The entity_id key inside response is dynamic, so we can't filter deeper.
static DynamicJsonDocument s_flt_weather(128);

static bool s_filters_built = false;

static void build_filters()
{
    if (s_filters_built) return;

    // get_states result filter
    s_flt_states["result"][0]["entity_id"]                             = true;
    s_flt_states["result"][0]["state"]                                 = true;
    s_flt_states["result"][0]["attributes"]["friendly_name"]           = true;
    s_flt_states["result"][0]["attributes"]["brightness"]              = true;
    s_flt_states["result"][0]["attributes"]["color_temp"]              = true;
    s_flt_states["result"][0]["attributes"]["current_position"]        = true;
    s_flt_states["result"][0]["attributes"]["current_temperature"]     = true;
    s_flt_states["result"][0]["attributes"]["temperature"]             = true;

    // area_registry result filter
    s_flt_areas["result"][0]["area_id"] = true;
    s_flt_areas["result"][0]["name"]    = true;

    // entity_registry result filter
    s_flt_ereg["result"][0]["entity_id"] = true;
    s_flt_ereg["result"][0]["area_id"]   = true;
    s_flt_ereg["result"][0]["device_id"] = true;

    // device_registry result filter
    s_flt_dreg["result"][0]["id"]      = true;
    s_flt_dreg["result"][0]["area_id"] = true;

    // state_changed event filter
    s_flt_event["event"]["event_type"]                                              = true;
    s_flt_event["event"]["data"]["entity_id"]                                      = true;
    s_flt_event["event"]["data"]["new_state"]["entity_id"]                         = true;
    s_flt_event["event"]["data"]["new_state"]["state"]                             = true;
    s_flt_event["event"]["data"]["new_state"]["attributes"]["friendly_name"]       = true;
    s_flt_event["event"]["data"]["new_state"]["attributes"]["brightness"]          = true;
    s_flt_event["event"]["data"]["new_state"]["attributes"]["color_temp"]          = true;
    s_flt_event["event"]["data"]["new_state"]["attributes"]["current_position"]    = true;
    s_flt_event["event"]["data"]["new_state"]["attributes"]["current_temperature"] = true;
    s_flt_event["event"]["data"]["new_state"]["attributes"]["temperature"]         = true;

    // weather.get_forecasts filter — keep entire response object
    s_flt_weather["result"]["response"] = true;

    s_filters_built = true;

    // Debug: verify filter structure
    Serial.print("[ha] flt_states: ");
    serializeJson(s_flt_states, Serial);
    Serial.println();
}

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
    doc["type"]         = "auth";
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

static void send_area_registry()
{
    StaticJsonDocument<64> doc;
    doc["id"]   = ++s_msg_id;
    doc["type"] = "config/area_registry/list";
    send_json(doc);
}

static void send_entity_registry()
{
    StaticJsonDocument<64> doc;
    doc["id"]   = ++s_msg_id;
    doc["type"] = "config/entity_registry/list";
    send_json(doc);
}

static void send_device_registry()
{
    StaticJsonDocument<64> doc;
    doc["id"]   = ++s_msg_id;
    doc["type"] = "config/device_registry/list";
    send_json(doc);
}

static void send_weather_forecast(const char* entity_id)
{
    StaticJsonDocument<256> doc;
    doc["id"]                        = ++s_msg_id;
    doc["type"]                      = "call_service";
    doc["domain"]                    = "weather";
    doc["service"]                   = "get_forecasts";
    doc["service_data"]["type"]      = "daily";
    doc["target"]["entity_id"]       = entity_id;
    doc["return_response"]           = true;
    s_weather_msg_id                 = s_msg_id;
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
    if (!s_doc) return; // not yet initialised

    // ---- Pass 1: extract type + success + id cheaply using a tiny filter.
    //
    // This avoids allocating a large document just to determine the message
    // type.  auth_required / auth_ok / auth_invalid are always small.  For
    // "result" and "event" messages we re-parse with the appropriate filter
    // into s_doc below.
    // -------------------------------------------------------------------------
    StaticJsonDocument<96> meta_flt;
    meta_flt["type"]    = true;
    meta_flt["success"] = true;
    meta_flt["id"]      = true;

    StaticJsonDocument<128> meta;
    if (deserializeJson(meta, payload, length,
                        DeserializationOption::Filter(meta_flt))) {
        // If even this tiny filtered parse fails something is very wrong;
        // skip and let the state machine time out / reconnect.
        return;
    }

    const char* type    = meta["type"]    | "";
    bool        succ    = meta["success"] | false;
    uint32_t    msg_id  = meta["id"]      | 0u;

    Serial.printf("[ha] msg type='%s' state=%d\n", type, (int)s_state);

    // ---- Auth handshake --------------------------------------------------
    if (strcmp(type, "auth_required") == 0) {
        send_auth();
        s_state = HaState::AUTHENTICATING;
        return;
    }

    if (strcmp(type, "auth_ok") == 0) {
        send_get_states();
        s_state = HaState::FETCHING_STATES;
        return;
    }

    if (strcmp(type, "auth_invalid") == 0) {
        Serial.println("[ha] auth_invalid — check HA long-lived token");
        s_ws.disconnect();
        s_state        = HaState::DISCONNECTED;
        s_backoff_idx  = BACKOFF_COUNT - 1; // lock to max backoff
        s_reconnect_at = millis() + BACKOFF_MS[s_backoff_idx];
        return;
    }

    // ---- Result messages -------------------------------------------------
    if (strcmp(type, "result") == 0) {
        if (!succ) return;

        if (s_state == HaState::FETCHING_STATES) {
            s_doc->clear();
            auto serr = deserializeJson(*s_doc, payload, length,
                                        DeserializationOption::Filter(s_flt_states));
            Serial.printf("[ha] states parse: %s  mem=%u/%u\n",
                          serr ? serr.c_str() : "ok",
                          (unsigned)s_doc->memoryUsage(),
                          (unsigned)s_doc->capacity());
            JsonArray result = (*s_doc)["result"].as<JsonArray>();
            Serial.printf("[ha] result: null=%d size=%u\n",
                          result.isNull(), (unsigned)result.size());
            if (!result.isNull() && s_on_states) s_on_states(result);
            Serial.printf("[ha] get_states done (%u entities in filter)\n",
                          (unsigned)result.size());
            send_area_registry();
            s_state = HaState::FETCHING_AREAS;
            return;
        }

        if (s_state == HaState::FETCHING_AREAS) {
            s_doc->clear();
            deserializeJson(*s_doc, payload, length,
                            DeserializationOption::Filter(s_flt_areas));
            JsonArray result = (*s_doc)["result"].as<JsonArray>();
            if (!result.isNull() && s_on_areas) s_on_areas(result);
            Serial.printf("[ha] area_registry done (%u areas)\n",
                          (unsigned)result.size());
            send_entity_registry();
            s_state = HaState::FETCHING_ENTITY_REG;
            return;
        }

        if (s_state == HaState::FETCHING_ENTITY_REG) {
            s_doc->clear();
            auto err = deserializeJson(*s_doc, payload, length,
                                       DeserializationOption::Filter(s_flt_ereg));
            if (!err) {
                JsonArray result = (*s_doc)["result"].as<JsonArray>();
                if (!result.isNull() && s_on_entity_reg) s_on_entity_reg(result);
                Serial.printf("[ha] entity_registry done (%u entries)\n",
                              (unsigned)result.size());
            } else {
                Serial.printf("[ha] entity_registry parse: %s\n", err.c_str());
            }
            // Always advance — a parse failure here is non-fatal.
            send_device_registry();
            s_state = HaState::FETCHING_DEVICE_REG;
            return;
        }

        if (s_state == HaState::FETCHING_DEVICE_REG) {
            s_doc->clear();
            auto err = deserializeJson(*s_doc, payload, length,
                                       DeserializationOption::Filter(s_flt_dreg));
            if (!err) {
                JsonArray result = (*s_doc)["result"].as<JsonArray>();
                if (!result.isNull() && s_on_device_reg) s_on_device_reg(result);
                Serial.printf("[ha] device_registry done (%u devices)\n",
                              (unsigned)result.size());
            } else {
                Serial.printf("[ha] device_registry parse: %s\n", err.c_str());
            }
            // Always advance — a parse failure here is non-fatal.
            send_subscribe_events();
            s_state = HaState::SUBSCRIBED;
            return;
        }

        // SUBSCRIBED: could be the subscribe_events ack OR a weather response.
        if (s_weather_msg_id != 0 && msg_id == s_weather_msg_id && succ) {
            s_weather_msg_id = 0;
            s_doc->clear();
            auto err = deserializeJson(*s_doc, payload, length,
                                       DeserializationOption::Filter(s_flt_weather));
            if (!err && s_on_weather) {
                JsonObject response =
                    (*s_doc)["result"]["response"].as<JsonObject>();
                if (!response.isNull()) {
                    JsonObject entity_resp =
                        response[s_weather_entity_id].as<JsonObject>();
                    if (!entity_resp.isNull()) {
                        // Log first forecast entry so field names are visible on serial
                        JsonArrayConst fc = entity_resp["forecast"].as<JsonArrayConst>();
                        if (!fc.isNull() && fc.size() > 0) {
                            String dbg;
                            serializeJson(fc[0], dbg);
                            Serial.printf("[ha] forecast[0]: %s\n", dbg.c_str());
                        } else {
                            Serial.println("[ha] weather: forecast array empty or missing");
                        }
                        s_on_weather(s_weather_entity_id, entity_resp);
                    } else {
                        Serial.printf("[ha] weather: entity '%s' not found in response\n",
                                      s_weather_entity_id);
                    }
                } else {
                    Serial.println("[ha] weather: result.response is null");
                }
            } else if (err) {
                Serial.printf("[ha] weather parse error: %s\n", err.c_str());
            }
            Serial.printf("[ha] weather forecast response handled for %s\n",
                          s_weather_entity_id);
        }
        // else: subscribe_events ack — nothing to do.
        return;
    }

    // ---- Live state_changed events ---------------------------------------
    if (strcmp(type, "event") == 0) {
        if (s_state != HaState::SUBSCRIBED) return;

        s_doc->clear();
        if (deserializeJson(*s_doc, payload, length,
                            DeserializationOption::Filter(s_flt_event))) return;

        const char* event_type = (*s_doc)["event"]["event_type"] | "";
        if (strcmp(event_type, "state_changed") != 0) return;

        const char* entity_id =
            (*s_doc)["event"]["data"]["entity_id"] | "";
        JsonObject new_state =
            (*s_doc)["event"]["data"]["new_state"].as<JsonObject>();

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
            Serial.println("[ha] WS connected");
            s_state       = HaState::AUTHENTICATING;
            s_backoff_idx = 0;
            break;

        case WStype_DISCONNECTED:
            Serial.printf("[ha] WS disconnected — retry in %u ms\n",
                          BACKOFF_MS[s_backoff_idx < BACKOFF_COUNT
                                     ? s_backoff_idx : BACKOFF_COUNT - 1]);
            s_state        = HaState::DISCONNECTED;
            s_reconnect_at = millis() +
                BACKOFF_MS[s_backoff_idx < BACKOFF_COUNT
                           ? s_backoff_idx++ : BACKOFF_COUNT - 1];
            break;

        case WStype_TEXT:
            Serial.printf("[ha] WS text: %u bytes\n", (unsigned)length);
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

void init(StateListCallback    on_states,
          StateChangedCallback on_state_changed,
          AreaListCallback     on_areas,
          EntityRegCallback    on_entity_reg,
          DeviceRegCallback    on_device_reg)
{
    s_on_states        = on_states;
    s_on_state_changed = on_state_changed;
    s_on_areas         = on_areas;
    s_on_entity_reg    = on_entity_reg;
    s_on_device_reg    = on_device_reg;

    s_state        = HaState::DISCONNECTED;
    s_msg_id       = 0;
    s_backoff_idx  = 0;
    s_reconnect_at = 0;

    build_filters();

    if (!s_doc) {
        s_doc = new DynamicJsonDocument(24576);
        Serial.printf("[ha] s_doc allocated: cap=%u free_heap=%u\n",
                      (unsigned)s_doc->capacity(), ESP.getFreeHeap());
    }

    NetworkConfig cfg{};
    if (!nvs_config::load_net_config(cfg)) {
        Serial.println("[ha] ERROR: load_net_config failed — no credentials in NVS");
        return;
    }

    Serial.printf("[ha] HA URL from NVS: '%s'\n", cfg.ha_url);
    Serial.printf("[ha] HA token length: %u\n", (unsigned)strlen(cfg.ha_token));

    s_url = parse_ha_url(cfg.ha_url);
    if (!s_url.valid) {
        Serial.printf("[ha] ERROR: parse_ha_url failed for '%s'\n", cfg.ha_url);
        Serial.println("[ha] URL must start with http:// or https://");
        return;
    }

    Serial.printf("[ha] parsed → host='%s' port=%u secure=%d\n",
                  s_url.host, s_url.port, s_url.secure);

    strncpy(s_token, cfg.ha_token, sizeof(s_token) - 1);
    s_token[sizeof(s_token) - 1] = '\0';

    s_ws.onEvent(on_ws_event);
    s_ws.setReconnectInterval(0); // we handle reconnect ourselves

    // Do NOT call begin() here — tick() owns the connection lifecycle.
    // s_reconnect_at = 0 means: connect on the very first tick().
    s_reconnect_at = 0;
}

void tick()
{
    if (!s_url.valid) return;

    if (s_state == HaState::DISCONNECTED && millis() >= s_reconnect_at) {
        // Set sentinel so we don't call begin() again until WStype_DISCONNECTED
        // resets s_reconnect_at via the backoff logic in on_ws_event.
        s_reconnect_at = UINT32_MAX;
        Serial.printf("[ha] connecting to %s://%s:%u/api/websocket\n",
                      s_url.secure ? "wss" : "ws", s_url.host, s_url.port);
        if (s_url.secure) s_ws.beginSSL(s_url.host, s_url.port, "/api/websocket");
        else              s_ws.begin(s_url.host, s_url.port, "/api/websocket");
    }

    // Always drive the WS state machine — the library needs loop() to
    // complete the TCP handshake even while we are still DISCONNECTED.
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

void request_weather_forecast(const char* entity_id, WeatherForecastCallback cb)
{
    if (!entity_id || entity_id[0] == '\0') return;
    if (s_state != HaState::SUBSCRIBED) return;
    if (s_weather_msg_id != 0) return; // request already in flight

    s_on_weather = cb;
    strncpy(s_weather_entity_id, entity_id, sizeof(s_weather_entity_id) - 1);
    s_weather_entity_id[sizeof(s_weather_entity_id) - 1] = '\0';

    Serial.printf("[ha] requesting weather forecast for %s\n", entity_id);
    send_weather_forecast(entity_id);
}

bool is_connected()
{
    return s_state == HaState::SUBSCRIBED;
}

} // namespace ha_client
