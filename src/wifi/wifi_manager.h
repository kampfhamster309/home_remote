#pragma once

#include "../config/nvs_config.h"

// WiFi connection manager.
// Handles first-boot captive portal provisioning, connection with status
// display, and automatic reconnection on drop.

namespace wifi_manager {

    // Load config from NVS. Must be called before any other function here.
    void init();

    // True if valid network credentials are stored in NVS.
    bool has_config();

    // True if WiFi is currently connected.
    bool is_connected();

    // Read-only access to the loaded network config (HA URL + token for later tickets).
    const NetworkConfig& config();

    // Blocking captive portal. Broadcasts softAP, serves setup form, saves
    // credentials to NVS on submission, then calls ESP.restart().
    // Never returns — only call when !has_config() or from enter_setup_mode().
    void start_portal();

    // Connect to WiFi using stored credentials. Displays boot screens via LVGL.
    // Returns true on success. On failure the app continues — HA client retries.
    bool connect();

    // Re-enter setup mode from within the running app (e.g. settings menu).
    // Equivalent to clear_net_config() + start_portal().
    void enter_setup_mode();

    // Call from main loop. Detects WiFi drop and reconnects with backoff.
    void tick();

}  // namespace wifi_manager
