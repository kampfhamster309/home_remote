#include "i18n.h"

#include <cstddef>

#ifdef ARDUINO
#  include <Preferences.h>
#endif

// ----------------------------------------------------------------------------
// String tables
//
// All strings are plain ASCII. German umlauts are written as digraphs
// (ae/oe/ue/ss) because the LVGL Montserrat fonts cover ASCII only.
// TICKET-012 will supply a custom font that renders the actual Unicode
// characters, at which point these digraphs can be replaced with UTF-8.
// ----------------------------------------------------------------------------

// clang-format off
static constexpr const char* s_de[] = {
    "Home Remote",                                        // APP_NAME
    "Verbindung mit Home Assistant...",                   // CONNECTING_HA
    "Keine Raeume in\nHome Assistant gefunden",           // NO_ROOMS
    "Keine Geraete in diesem Raum",                       // NO_DEVICES
    "Einrichtungsmodus",                                  // WIFI_SETUP_MODE
    "Verbinde dein Handy mit WLAN:\nHomeRemote-Setup",    // WIFI_SETUP_CONNECT
    "Dann Browser oeffnen.\nEine Einrichtungsseite erscheint.", // WIFI_SETUP_BROWSER
    "Verbinde...",                                        // WIFI_CONNECTING
    "WLAN: %s",                                           // WIFI_SSID_FMT
    "Verbunden",                                          // WIFI_CONNECTED
    "IP: %s",                                             // WIFI_IP_FMT
    "Kein WLAN",                                          // WIFI_NO_WIFI
    "Keine Verbindung.\nHA nicht verfuegbar.",             // WIFI_FAIL
    "Helligkeit",                                         // DETAIL_BRIGHTNESS
    "Farbtemperatur",                                     // DETAIL_COLOR_TEMP
    "Solltemperatur",                                     // DETAIL_TARGET_TEMP
    "Position",                                           // DETAIL_POSITION
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
