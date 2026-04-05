#include "nb_client.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <Update.h>
#include <mbedtls/sha256.h>

#include "../config/nvs_config.h"
#include "semver.h"

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

// Version check state
static constexpr uint32_t VERSION_CHECK_INTERVAL_MS = 24UL * 3600UL * 1000UL;
static volatile bool  s_update_available  = false;
static volatile char  s_latest_version[32] = {};
static volatile bool  s_task_running       = false;
static uint32_t       s_last_check_ms      = 0;

// Post-update reporting state (TICKET-020)
static volatile bool  s_startup_reported   = false;

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

// FreeRTOS task: GET /api/v1/firmware/latest/ and compare with running version.
// Deletes itself when done; sets s_task_running = false before exiting.
static void version_check_task(void* /*param*/)
{
    HTTPClient http;
    const String url = make_url("/api/v1/firmware/latest/?device_type=")
                       + DEVICE_TYPE;

    if (http.begin(url)) {
        http.addHeader("Authorization", String("Api-Key ") + s_cfg.nb_api_key);
        const int code = http.GET();

        if (code == 200) {
            // Filter: only keep "version" — the response also contains a long
            // presigned S3 URL (~500–1000 chars) that would overflow a small doc.
            StaticJsonDocument<16>  filter;
            filter["version"] = true;
            StaticJsonDocument<64>  doc;
            const DeserializationError err = deserializeJson(
                doc, http.getString(), DeserializationOption::Filter(filter));
            if (!err) {
                const char* ver = doc["version"] | "";
                if (ver[0] != '\0') {
                    strncpy(const_cast<char*>(s_latest_version), ver,
                            sizeof(s_latest_version) - 1);
                    const_cast<char*>(s_latest_version)[sizeof(s_latest_version) - 1] = '\0';
                    s_update_available = semver_newer(ver, FIRMWARE_VERSION);
                    Serial.printf("[nb] version check: server=%s running=%s update=%s\n",
                                  ver, FIRMWARE_VERSION,
                                  s_update_available ? "YES" : "no");
                }
            } else {
                Serial.printf("[nb] version check: JSON error: %s\n", err.c_str());
            }
        } else if (code == 404) {
            Serial.println("[nb] version check: no firmware release on server");
        } else {
            Serial.printf("[nb] version check: HTTP %d\n", code);
        }
        http.end();
    } else {
        Serial.println("[nb] version check: http.begin() failed");
    }

    s_task_running = false;
    vTaskDelete(nullptr);
}

// Internal helper: POST /api/v1/devices/events/ — blocking HTTP call.
// `event` must be "update_success" or "update_failed".
// Returns true on HTTP 201.
static bool report_event_internal(const char* event)
{
    if (!s_has_config || s_status != Status::REGISTERED) return false;

    HTTPClient http;
    const String url = make_url("/api/v1/devices/events/");
    if (!http.begin(url)) return false;

    http.addHeader("Authorization", String("Api-Key ") + s_cfg.nb_api_key);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<128> doc;
    doc["event"]   = event;
    doc["version"] = FIRMWARE_VERSION;
    String body;
    serializeJson(doc, body);

    const int code = http.POST(body);
    http.end();

    Serial.printf("[nb] events: %s → HTTP %d\n", event, code);
    return code == 201;
}

// FreeRTOS task: report update_success then clear NVS state.
// Spawned by on_startup_confirmed() so it does not block the LVGL loop.
static void startup_report_task(void* /*param*/)
{
    report_event_internal("update_success");
    nvs_config::clear_update_state();
    vTaskDelete(nullptr);
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

    // Build request body — name includes MAC suffix for uniqueness
    const String device_name = build_device_name();

    HTTPClient http;
    const String url = make_url("/api/v1/devices/register/");

    Serial.printf("[nb] registering '%s' at %s\n", device_name.c_str(), url.c_str());

    if (!http.begin(url)) {
        Serial.println("[nb] register: http.begin() failed");
        s_status = Status::REG_FAILED;
        return false;
    }
    http.addHeader("Content-Type", "application/json");
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

OtaResult start_ota_update(OtaProgressFn progress_cb)
{
    if (!s_has_config || !is_registered()) return OtaResult::ERR_NETWORK;

    // ---- Step 1: fresh firmware/latest call to get url + sha256 ---------------
    // The presigned URL expires after 300 s — never reuse a cached value.
    HTTPClient meta_http;
    const String meta_url = make_url("/api/v1/firmware/latest/?device_type=")
                            + DEVICE_TYPE;

    if (!meta_http.begin(meta_url)) return OtaResult::ERR_NETWORK;
    meta_http.addHeader("Authorization", String("Api-Key ") + s_cfg.nb_api_key);
    const int meta_code = meta_http.GET();

    if (meta_code == 404) { meta_http.end(); return OtaResult::ERR_NO_RELEASE; }
    if (meta_code == 503) { meta_http.end(); return OtaResult::ERR_SERVER;     }
    if (meta_code != 200) { meta_http.end(); return OtaResult::ERR_NETWORK;    }

    // Parse url + sha256.  URL can be 500-1000 chars (presigned S3).
    DynamicJsonDocument meta_doc(2048);
    DeserializationError meta_err = deserializeJson(meta_doc, meta_http.getString());
    meta_http.end();

    if (meta_err) return OtaResult::ERR_NETWORK;

    const char* fw_url_raw    = meta_doc["url"]    | "";
    const char* fw_sha256_raw = meta_doc["sha256"] | "";
    if (fw_url_raw[0] == '\0' || fw_sha256_raw[0] == '\0') return OtaResult::ERR_NETWORK;

    // Copy into local buffers — meta_doc goes out of scope before download ends
    char fw_url[1024]  = {};
    char fw_sha256[65] = {};
    strncpy(fw_url,    fw_url_raw,    sizeof(fw_url)    - 1);
    strncpy(fw_sha256, fw_sha256_raw, sizeof(fw_sha256) - 1);

    Serial.printf("[nb] OTA: url prefix=%.40s sha256=%.8s...\n", fw_url, fw_sha256);

    // ---- Step 2: open firmware download stream --------------------------------
    HTTPClient dl_http;
    if (!dl_http.begin(fw_url)) return OtaResult::ERR_NETWORK;

    const int dl_code = dl_http.GET();
    if (dl_code != 200) { dl_http.end(); return OtaResult::ERR_NETWORK; }

    const int fw_size = dl_http.getSize();  // -1 if unknown (chunked)
    const size_t update_size = (fw_size > 0) ? static_cast<size_t>(fw_size)
                                             : UPDATE_SIZE_UNKNOWN;

    if (!Update.begin(update_size)) {
        Serial.printf("[nb] OTA: Update.begin() error: %s\n", Update.errorString());
        dl_http.end();
        return OtaResult::ERR_FLASH;
    }

    // ---- Step 3: stream, hash, and flash in one pass -------------------------
    mbedtls_sha256_context sha_ctx;
    mbedtls_sha256_init(&sha_ctx);
    mbedtls_sha256_starts_ret(&sha_ctx, 0);  // 0 = SHA-256

    WiFiClient* stream = dl_http.getStreamPtr();
    uint8_t buf[512];
    int written = 0;
    bool stream_error = false;

    while (dl_http.connected() && (fw_size < 0 || written < fw_size)) {
        const int avail = stream->available();
        if (avail == 0) { delay(1); continue; }

        const int chunk = (avail < static_cast<int>(sizeof(buf)))
                          ? avail : static_cast<int>(sizeof(buf));
        const int n = stream->readBytes(buf, chunk);
        if (n <= 0) break;

        mbedtls_sha256_update_ret(&sha_ctx, buf, static_cast<size_t>(n));

        if (Update.write(buf, static_cast<size_t>(n)) != static_cast<size_t>(n)) {
            Serial.printf("[nb] OTA: Update.write() error: %s\n", Update.errorString());
            stream_error = true;
            break;
        }
        written += n;

        if (progress_cb && fw_size > 0) progress_cb(written * 100 / fw_size);
    }
    dl_http.end();

    if (stream_error) {
        mbedtls_sha256_free(&sha_ctx);
        Update.abort();
        return OtaResult::ERR_FLASH;
    }
    if (fw_size > 0 && written != fw_size) {
        mbedtls_sha256_free(&sha_ctx);
        Update.abort();
        return OtaResult::ERR_NETWORK;
    }

    // ---- Step 4: verify SHA-256 before finalising ----------------------------
    uint8_t hash[32];
    mbedtls_sha256_finish_ret(&sha_ctx, hash);
    mbedtls_sha256_free(&sha_ctx);

    char computed_hex[65] = {};
    for (int i = 0; i < 32; ++i) {
        snprintf(computed_hex + i * 2, 3, "%02x", hash[i]);
    }

    if (strcmp(computed_hex, fw_sha256) != 0) {
        Serial.printf("[nb] OTA: SHA-256 mismatch — expected=%s computed=%s\n",
                      fw_sha256, computed_hex);
        Update.abort();
        return OtaResult::ERR_CHECKSUM;
    }

    // ---- Step 5: finalise — marks new partition as boot target ---------------
    if (!Update.end(true)) {
        Serial.printf("[nb] OTA: Update.end() error: %s\n", Update.errorString());
        return OtaResult::ERR_FLASH;
    }

    Serial.printf("[nb] OTA: flash complete (%d bytes). Reboot required.\n", written);
    return OtaResult::SUCCESS;
}

void start_version_check()
{
    if (!s_has_config || !is_registered()) return;
    if (s_task_running) return;

    s_task_running   = true;
    s_last_check_ms  = millis();
    xTaskCreatePinnedToCore(version_check_task, "nb_ver_chk",
                            4096, nullptr, 1, nullptr, 0);
}

void tick()
{
    if (!s_has_config || !is_registered()) return;
    if (s_task_running) return;

    const uint32_t now = millis();
    if (s_last_check_ms != 0 &&
        (now - s_last_check_ms) < VERSION_CHECK_INTERVAL_MS) return;

    start_version_check();
}

bool is_update_available()
{
    return s_update_available;
}

const char* get_latest_version()
{
    return const_cast<const char*>(s_latest_version);
}

void check_boot_loop()
{
    if (!s_has_config) return;
    if (!nvs_config::load_update_pending()) return;
    if (nvs_config::load_boot_count() < 3) return;

    // Three boot attempts without a confirmed successful startup — the new
    // firmware is likely broken.  Report failure (best-effort; WiFi may be
    // down), clear the NVS state so we can't loop here again, then roll back
    // to the previous OTA partition and reboot.
    Serial.println("[nb] boot loop detected after OTA — rolling back");
    report_event_internal("update_failed");  // best-effort
    nvs_config::clear_update_state();

    if (Update.rollBack()) {
        Serial.println("[nb] OTA rollback OK — rebooting into previous firmware");
    } else {
        Serial.println("[nb] WARNING: Update.rollBack() failed — no valid fallback partition?");
    }
    delay(500);
    ESP.restart();
    // Does not return.
}

void on_startup_confirmed()
{
    if (s_startup_reported) return;  // already reported (e.g. after HA reconnect)
    if (!s_has_config || s_status != Status::REGISTERED) return;
    if (!nvs_config::load_update_pending()) return;

    // Guard against re-entry before the task clears NVS (HA reconnects quickly).
    s_startup_reported = true;

    Serial.printf("[nb] startup confirmed — reporting update_success for v%s\n",
                  FIRMWARE_VERSION);
    xTaskCreatePinnedToCore(startup_report_task, "nb_upd_ok",
                            4096, nullptr, 1, nullptr, 0);
}

} // namespace nb_client
