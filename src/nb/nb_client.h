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

// ----------------------------------------------------------------------------
// OTA download & flash (TICKET-019)
// ----------------------------------------------------------------------------

enum class OtaResult {
    SUCCESS,          // flashed OK — caller must reboot
    ERR_NO_RELEASE,   // server returned 404 (no firmware for this device type)
    ERR_SERVER,       // server returned 503 (firmware file missing from S3)
    ERR_NETWORK,      // HTTP / connection / incomplete download error
    ERR_CHECKSUM,     // SHA-256 mismatch — update aborted
    ERR_FLASH,        // Update.h write/end failed
};

// Callback type for download progress.  Called with 0..100 during streaming.
// Implementation should call lv_timer_handler() to keep the display live.
typedef void (*OtaProgressFn)(int percent);

// Perform a full OTA update cycle (blocking):
//   1. Fresh GET /api/v1/firmware/latest/ to obtain presigned url + sha256
//   2. Stream firmware into the inactive OTA partition via Update.h
//   3. Verify SHA-256; abort if mismatch
//   4. Finalise partition on success
//
// On SUCCESS the caller must call ESP.restart() — the new partition is already
// marked as the boot target.
// On any error the running firmware is unchanged.
// progress_cb may be nullptr.
OtaResult start_ota_update(OtaProgressFn progress_cb = nullptr);

// ----------------------------------------------------------------------------
// Firmware version check (TICKET-018)
// ----------------------------------------------------------------------------

// Trigger an immediate non-blocking version check via a FreeRTOS task
// (Core 0, 4 KB stack).  Safe to call from setup() after registration.
// No-op if not configured, not registered, or a check is already running.
void start_version_check();

// Call from loop() every iteration.  Triggers a version check if the 24-hour
// interval has elapsed since the last successful check.  No-op if a check is
// already in progress.
void tick();

// Returns true if the last completed version check found a newer firmware
// version on the server than the running FIRMWARE_VERSION.
bool is_update_available();

// Returns the latest version string received from the server (e.g. "1.2.0"),
// or an empty string if no check has completed yet.
const char* get_latest_version();

// ----------------------------------------------------------------------------
// Post-update reporting & rollback (TICKET-020)
// ----------------------------------------------------------------------------

// Call early in setup() after wifi_manager::connect() and nb_client::init().
// If the boot-attempt counter (incremented by nvs_config::increment_boot_count()
// early in setup) has reached 3 with an update pending, this reports
// "update_failed" to nano_backbone (best-effort), clears the NVS state,
// rolls back to the previous OTA partition, and reboots.  May not return.
void check_boot_loop();

// Call once the device is confirmed operational (HA shell displayed).
// If an OTA update was pending, spawns a background FreeRTOS task to report
// "update_success" to nano_backbone and clears the NVS boot state.
// No-op on subsequent calls (guarded by an internal flag).
void on_startup_confirmed();

} // namespace nb_client
