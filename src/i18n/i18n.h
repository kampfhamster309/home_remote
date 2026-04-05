#pragma once

#include <stdint.h>

// Supported display locales.
enum class Locale : uint8_t { DE = 0, EN = 1 };

// Every user-visible string shown on the device display has an entry here.
// The HTML captive portal is served to a browser and is intentionally
// excluded — it always uses English regardless of the device locale.
enum class StrId : uint8_t {
    // Loading / main shell
    APP_NAME,           // "Home Remote"
    CONNECTING_HA,      // "Connecting to Home Assistant..."
    NO_ROOMS,           // "No rooms found in\nHome Assistant"
    NO_DEVICES,         // "No devices in this room"

    // WiFi boot screens
    WIFI_SETUP_MODE,    // "Setup Mode"
    WIFI_SETUP_CONNECT, // "Connect your phone to Wi-Fi:\nHomeRemote-Setup"
    WIFI_SETUP_BROWSER, // "Then open your browser.\nA setup page will appear."
    WIFI_CONNECTING,    // "Connecting..."
    WIFI_SSID_FMT,      // "Wi-Fi: %s"  (printf format — %s = SSID)
    WIFI_CONNECTED,     // "Connected"
    WIFI_IP_FMT,        // "IP: %s"     (printf format — %s = IP address)
    WIFI_NO_WIFI,       // "No Wi-Fi"
    WIFI_FAIL,          // "Could not connect.\nHA features unavailable."

    // Detail / control screen slider labels
    DETAIL_BRIGHTNESS,  // "Brightness"
    DETAIL_COLOR_TEMP,  // "Color Temp"
    DETAIL_TARGET_TEMP, // "Target Temp"
    DETAIL_POSITION,    // "Position"

    // Weather tab (TICKET-012a)
    WEATHER_TAB,        // "Wetter" / "Weather"  (nav tab label)
    WEATHER_HIGH,       // "Hoch: " / "High: "
    WEATHER_LOW,        // "Tief: " / "Low: "
    WEATHER_PRECIP,     // "Regen: " / "Rain: "
    WEATHER_UNAVAIL,    // "Keine Wetterdaten" / "No weather data"

    // Settings screen (TICKET-013)
    SETTINGS_TITLE,       // "Einstellungen" / "Settings"
    SETTINGS_LANGUAGE,    // "Sprache:" / "Language:"
    SETTINGS_BRIGHTNESS,  // "Helligkeit:" / "Brightness:"
    SETTINGS_RECALIBRATE, // "Touch kalibrieren" / "Calibrate Touch"

    // Error / offline states (TICKET-014)
    ERR_HA_UNREACHABLE, // "HA nicht erreichbar" / "HA unreachable"
    ERR_AUTH_FAILED,    // "HA-Token ungültig\nNeu einrichten" / "Invalid HA token\nReconfigure in Settings"

    // nano_backbone OTA settings section (TICKET-017)
    NB_STATUS_NOT_CFG,  // "OTA: Nicht konfiguriert" / "OTA: Not configured"
    NB_STATUS_UNREG,    // "OTA: Nicht registriert"  / "OTA: Not registered"
    NB_STATUS_OK,       // "OTA: Registriert"         / "OTA: Registered"
    NB_STATUS_FAILED,   // "OTA: Fehlgeschlagen"      / "OTA: Failed"
    NB_REGISTER_BTN,    // "OTA registrieren"         / "Register OTA"

    // nano_backbone OTA download & flash (TICKET-019)
    NB_UPDATE_BTN,        // "Update installieren"              / "Install Update"
    NB_UPDATING,          // "Firmware wird aktualisiert..."    / "Updating firmware..."
    NB_UPDATE_OK,         // "Erfolgreich. Neustart..."         / "Success. Rebooting..."
    NB_UPDATE_FAIL_NET,   // "Download fehlgeschlagen"          / "Download failed"
    NB_UPDATE_FAIL_HASH,  // "Prüfsumme ungültig"               / "Checksum invalid"
    NB_UPDATE_FAIL_FLASH, // "Flash fehlgeschlagen"             / "Flash failed"
    NB_UPDATE_NO_RELEASE, // "Kein Update verfügbar"            / "No update available"

    // Battery / mobile mode settings (TICKET-025)
    SETTINGS_BATTERY_MODE,  // "Akkubetrieb:"   / "Battery mode:"
    SETTINGS_BATTERY_OFF,   // "Aus"            / "Off"
    SETTINGS_BATTERY_ON,    // "An"             / "On"
    SETTINGS_SLEEP_TIMEOUT, // "Schlaf-Timer:"  / "Sleep timeout:"
    NB_UPDATE_BATT_HINT,    // hint shown instead of "Install Update" when battery mode is on

    _COUNT              // sentinel — keep last
};

namespace i18n {

// Load locale from NVS.  Call once at startup before any str() calls.
// On native builds (no NVS) this is a no-op; locale defaults to DE.
void init();

// Read / write the active locale.
// set_locale() persists the change to NVS on device builds.
Locale get_locale();
void   set_locale(Locale loc);

// Return the string for `id` in the active locale.
// Returns "" for out-of-range ids (never nullptr).
const char* str(StrId id);

} // namespace i18n
