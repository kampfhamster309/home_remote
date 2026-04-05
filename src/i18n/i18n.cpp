#include "i18n.h"

#include <cstddef>

#ifdef ARDUINO
#  include <Preferences.h>
#endif

// ----------------------------------------------------------------------------
// String tables
//
// German strings use UTF-8 encoded umlauts (ä ö ü ß etc.).
// LVGL Montserrat fonts include Latin-1 Supplement (U+00C0–U+00FF),
// so all German diacritics render correctly.
// ----------------------------------------------------------------------------

// clang-format off
static constexpr const char* s_de[] = {
    "Home Remote",                                        // APP_NAME
    "Verbindung mit Home Assistant...",                   // CONNECTING_HA
    "Keine R\xc3\xa4ume in\nHome Assistant gefunden",     // NO_ROOMS
    "Keine Ger\xc3\xa4te in diesem Raum",                 // NO_DEVICES
    "Einrichtungsmodus",                                  // WIFI_SETUP_MODE
    "Verbinde dein Handy mit WLAN:\nHomeRemote-Setup",    // WIFI_SETUP_CONNECT
    "Dann Browser \xc3\xb6" "ffnen.\nEine Einrichtungsseite erscheint.", // WIFI_SETUP_BROWSER
    "Verbinde...",                                        // WIFI_CONNECTING
    "WLAN: %s",                                           // WIFI_SSID_FMT
    "Verbunden",                                          // WIFI_CONNECTED
    "IP: %s",                                             // WIFI_IP_FMT
    "Kein WLAN",                                          // WIFI_NO_WIFI
    "Keine Verbindung.\nHA nicht verf\xc3\xbcgbar.",      // WIFI_FAIL
    "Helligkeit",                                         // DETAIL_BRIGHTNESS
    "Farbtemperatur",                                     // DETAIL_COLOR_TEMP
    "Solltemperatur",                                     // DETAIL_TARGET_TEMP
    "Position",                                           // DETAIL_POSITION
    "Wetter",                                             // WEATHER_TAB
    "Hoch: ",                                             // WEATHER_HIGH
    "Tief: ",                                             // WEATHER_LOW
    "Regen: ",                                            // WEATHER_PRECIP
    "Keine Wetterdaten",                                  // WEATHER_UNAVAIL
    "Einstellungen",                                      // SETTINGS_TITLE
    "Sprache:",                                           // SETTINGS_LANGUAGE
    "Helligkeit:",                                        // SETTINGS_BRIGHTNESS
    "Touch kalibrieren",                                  // SETTINGS_RECALIBRATE
    "HA nicht erreichbar",                                // ERR_HA_UNREACHABLE
    "HA-Token ung\xc3\xbcltig\nNeu einrichten",          // ERR_AUTH_FAILED
    "OTA: Nicht konfiguriert",                            // NB_STATUS_NOT_CFG
    "OTA: Nicht registriert",                             // NB_STATUS_UNREG
    "OTA: Registriert",                                   // NB_STATUS_OK
    "OTA: Fehlgeschlagen",                                // NB_STATUS_FAILED
    "OTA registrieren",                                   // NB_REGISTER_BTN
    "Update installieren",                                // NB_UPDATE_BTN
    "Firmware wird aktualisiert...",                      // NB_UPDATING
    "Erfolgreich. Neustart...",                           // NB_UPDATE_OK
    "Download fehlgeschlagen",                            // NB_UPDATE_FAIL_NET
    "Pr\xc3\xbcfsumme ung\xc3\xbcltig",                  // NB_UPDATE_FAIL_HASH
    "Flash fehlgeschlagen",                               // NB_UPDATE_FAIL_FLASH
    "Kein Update verf\xc3\xbcgbar",                       // NB_UPDATE_NO_RELEASE
};

static constexpr const char* s_en[] = {
    "Home Remote",                                        // APP_NAME
    "Connecting to Home Assistant...",                    // CONNECTING_HA
    "No rooms found in\nHome Assistant",                  // NO_ROOMS
    "No devices in this room",                            // NO_DEVICES
    "Setup Mode",                                         // WIFI_SETUP_MODE
    "Connect your phone to Wi-Fi:\nHomeRemote-Setup",     // WIFI_SETUP_CONNECT
    "Then open your browser.\nA setup page will appear.", // WIFI_SETUP_BROWSER
    "Connecting...",                                      // WIFI_CONNECTING
    "Wi-Fi: %s",                                          // WIFI_SSID_FMT
    "Connected",                                          // WIFI_CONNECTED
    "IP: %s",                                             // WIFI_IP_FMT
    "No Wi-Fi",                                           // WIFI_NO_WIFI
    "Could not connect.\nHA features unavailable.",       // WIFI_FAIL
    "Brightness",                                         // DETAIL_BRIGHTNESS
    "Color Temp",                                         // DETAIL_COLOR_TEMP
    "Target Temp",                                        // DETAIL_TARGET_TEMP
    "Position",                                           // DETAIL_POSITION
    "Weather",                                            // WEATHER_TAB
    "High: ",                                             // WEATHER_HIGH
    "Low: ",                                              // WEATHER_LOW
    "Rain: ",                                             // WEATHER_PRECIP
    "No weather data",                                    // WEATHER_UNAVAIL
    "Settings",                                           // SETTINGS_TITLE
    "Language:",                                          // SETTINGS_LANGUAGE
    "Brightness:",                                        // SETTINGS_BRIGHTNESS
    "Calibrate Touch",                                    // SETTINGS_RECALIBRATE
    "HA unreachable",                                     // ERR_HA_UNREACHABLE
    "Invalid HA token\nReconfigure in Settings",          // ERR_AUTH_FAILED
    "OTA: Not configured",                                // NB_STATUS_NOT_CFG
    "OTA: Not registered",                                // NB_STATUS_UNREG
    "OTA: Registered",                                    // NB_STATUS_OK
    "OTA: Failed",                                        // NB_STATUS_FAILED
    "Register OTA",                                       // NB_REGISTER_BTN
    "Install Update",                                     // NB_UPDATE_BTN
    "Updating firmware...",                               // NB_UPDATING
    "Success. Rebooting...",                              // NB_UPDATE_OK
    "Download failed",                                    // NB_UPDATE_FAIL_NET
    "Checksum invalid",                                   // NB_UPDATE_FAIL_HASH
    "Flash failed",                                       // NB_UPDATE_FAIL_FLASH
    "No update available",                                // NB_UPDATE_NO_RELEASE
};
// clang-format on

static_assert(sizeof(s_de) / sizeof(s_de[0]) == static_cast<size_t>(StrId::_COUNT),
              "s_de table size mismatch — add entries for all StrId values");
static_assert(sizeof(s_en) / sizeof(s_en[0]) == static_cast<size_t>(StrId::_COUNT),
              "s_en table size mismatch — add entries for all StrId values");

// ----------------------------------------------------------------------------
// Locale state
// ----------------------------------------------------------------------------

static Locale s_locale = Locale::DE;

#ifdef ARDUINO
static constexpr char NVS_NS[]  = "i18n";
static constexpr char NVS_KEY[] = "locale";
#endif

namespace i18n {

void init()
{
#ifdef ARDUINO
    Preferences prefs;
    prefs.begin(NVS_NS, /* readOnly= */ true);
    const uint8_t v = prefs.getUChar(NVS_KEY, static_cast<uint8_t>(Locale::DE));
    prefs.end();
    s_locale = (v == static_cast<uint8_t>(Locale::EN)) ? Locale::EN : Locale::DE;
#endif
}

Locale get_locale()
{
    return s_locale;
}

void set_locale(Locale loc)
{
    s_locale = loc;
#ifdef ARDUINO
    Preferences prefs;
    prefs.begin(NVS_NS, /* readOnly= */ false);
    prefs.putUChar(NVS_KEY, static_cast<uint8_t>(loc));
    prefs.end();
#endif
}

const char* str(StrId id)
{
    const size_t idx = static_cast<size_t>(id);
    constexpr size_t N = static_cast<size_t>(StrId::_COUNT);
    if (idx >= N) return "";
    return (s_locale == Locale::EN) ? s_en[idx] : s_de[idx];
}

} // namespace i18n
