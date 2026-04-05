#pragma once

// nano_backbone OTA client.
//
// Handles device registration and API key management for the nano_backbone
// OTA update server (kampfhamster309/nano_backbone).
//
// Device type used for this board: "esp32_2432s028"
//
// Startup flow:
//   nb_client::init()                    — call once after nvs_config is ready
//   if (has_config() && !is_registered())
//       register_device()                — blocking HTTP POST; stores API key in NVS
//
// The API key is issued once by the server and shown only in the registration
// response.  It must be stored in NVS immediately.

namespace nb_client {

enum class Status {
    NOT_CONFIGURED, // no nb_url in NVS — OTA disabled
    UNREGISTERED,   // url set, no api_key yet (first boot after provisioning)
    REGISTERED,     // url + api_key stored in NVS
    REG_FAILED,     // last registration attempt failed (network error / server error)
};

// Load config from NVS.  Must be called once after nvs_config is initialised.
void init();

// True if a nano_backbone server URL is configured.
bool has_config();

// True if an API key is stored in NVS (device was successfully registered).
bool is_registered();

// Current registration status.
Status get_status();

// Attempt to register with the nano_backbone server.
// Blocking HTTP POST to /api/v1/devices/register/.
// On success: stores the API key in NVS and returns true.
// If already registered: returns true without network call.
// On failure: updates status to REG_FAILED and returns false.
// Safe to call when not connected — will return false immediately.
bool register_device();

// Validate the stored API key with GET /api/v1/ping/.
// Returns true if the server responds 200 {"status":"ok"}.
bool ping();

// Erase the stored API key — triggers re-registration on the next call
// to register_device().  The nb_url is preserved.
void clear_registration();

} // namespace nb_client
