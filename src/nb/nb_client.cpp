#include "nb_client.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>

#include "../config/nvs_config.h"

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------

namespace {

static constexpr char DEVICE_TYPE[] = "esp32_2432s028";

// Build device name: "Home Remote XXYYZZ" where XXYYZZ are the last 3 bytes
// of the WiFi MAC address (upper-case hex, no colons).  Ensures uniqueness
// across multiple devices on the same nano_backbone instance.
// MAC format from WiFi.macAddress(): "AA:BB:CC:DD:EE:FF" (17 chars)
// Last 3 bytes sit at substring indices (9,11), (12,14), (15,17).
static String build_device_name()
{
    const String mac = WiFi.macAddress();  // "AA:BB:CC:DD:EE:FF"
    String suffix = mac.substring(9, 11)   // DD
                  + mac.substring(12, 14)  // EE
                  + mac.substring(15, 17); // FF
    suffix.toUpperCase();
    return String("Home Remote ") + suffix;
}

// Module state
static NanoBackboneConfig s_cfg{};
static bool               s_has_config = false;
static nb_client::Status  s_status     = nb_client::Status::NOT_CONFIGURED;

// Build a full API URL by appending `path` to the stored base URL.
// Handles a trailing slash on nb_url gracefully (already stripped by portal,
// but defensive here).
static String make_url(const char* path)
{
    String url = s_cfg.nb_url;
    while (url.endsWith("/")) url.remove(url.length() - 1);
    url += path;
    return url;
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

namespace nb_client {

void init()
{
    memset(&s_cfg, 0, sizeof(s_cfg));
    s_has_config = nvs_config::load_nb_config(s_cfg);

    if (!s_has_config) {
        s_status = Status::NOT_CONFIGURED;
        Serial.println("[nb] no config — OTA disabled");
        return;
    }

    if (s_cfg.nb_api_key[0] != '\0') {
        s_status = Status::REGISTERED;
        Serial.printf("[nb] registered — url=%s key=%.8s...\n",
                      s_cfg.nb_url, s_cfg.nb_api_key);
    } else {
        s_status = Status::UNREGISTERED;
        Serial.printf("[nb] url=%s — not yet registered\n", s_cfg.nb_url);
    }
}

bool has_config()    { return s_has_config; }
bool is_registered() { return s_status == Status::REGISTERED; }
Status get_status()  { return s_status; }

bool register_device()
{
    if (!s_has_config) return false;
    if (is_registered()) return true;  // nothing to do

    HTTPClient http;
    const String url = make_url("/api/v1/devices/register/");

    Serial.printf("[nb] registering '%s' at %s\n", device_name.c_str(), url.c_str());

    if (!http.begin(url)) {
        Serial.println("[nb] register: http.begin() failed");
        s_status = Status::REG_FAILED;
        return false;
    }
    http.addHeader("Content-Type", "application/json");

    // Build request body — name includes MAC suffix for uniqueness
    const String device_name = build_device_name();
    StaticJsonDocument<128> req;
    req["name"]        = device_name.c_str();
    req["device_type"] = DEVICE_TYPE;
    String body;
    serializeJson(req, body);

    const int code = http.POST(body);
    if (code != 201) {
        Serial.printf("[nb] register: HTTP %d\n", code);
        http.end();
        s_status = Status::REG_FAILED;
        return false;
    }

    // Parse response — api_key is shown only once
    StaticJsonDocument<512> resp;
    const DeserializationError err = deserializeJson(resp, http.getString());
    http.end();

    if (err) {
        Serial.printf("[nb] register: JSON parse error: %s\n", err.c_str());
        s_status = Status::REG_FAILED;
        return false;
    }

    const char* api_key = resp["api_key"] | "";
    if (api_key[0] == '\0') {
        Serial.println("[nb] register: api_key missing from response");
        s_status = Status::REG_FAILED;
        return false;
    }

    // Store key in memory and NVS immediately — shown only once by server
    strncpy(s_cfg.nb_api_key, api_key, sizeof(s_cfg.nb_api_key) - 1);
    s_cfg.nb_api_key[sizeof(s_cfg.nb_api_key) - 1] = '\0';
    nvs_config::save_nb_api_key(s_cfg.nb_api_key);

    s_status = Status::REGISTERED;
    Serial.printf("[nb] registration OK — key prefix: %.8s...\n", s_cfg.nb_api_key);
    return true;
}

bool ping()
{
    if (!s_has_config || !is_registered()) return false;

    HTTPClient http;
    const String url = make_url("/api/v1/ping/");

    if (!http.begin(url)) return false;
    http.addHeader("Authorization", String("Api-Key ") + s_cfg.nb_api_key);

    const int code = http.GET();
    http.end();

    Serial.printf("[nb] ping → HTTP %d\n", code);
    return code == 200;
}

void clear_registration()
{
    s_cfg.nb_api_key[0] = '\0';
    nvs_config::clear_nb_api_key();
    s_status = Status::UNREGISTERED;
    Serial.println("[nb] registration cleared");
}

} // namespace nb_client
