// Tests for the i18n string table and locale switching.
// NVS persistence is a no-op on native builds; set_locale() only updates the
// in-memory state, which is sufficient to verify the string tables.

// Pull in the implementation directly — no Arduino/NVS dependencies on native.
#include "i18n/i18n.cpp"  // NOLINT — deliberate impl include for native tests

#include <unity.h>
#include <cstring>

// Reset to DE before each test to avoid ordering dependencies.
void setUp()    { i18n::set_locale(Locale::DE); }
void tearDown() {}

// ----------------------------------------------------------------------------
// Default locale
// ----------------------------------------------------------------------------

void test_default_locale_is_de()
{
    // i18n.cpp initialises s_locale = Locale::DE at static-init time.
    // setUp() explicitly sets DE, so this always holds.
    TEST_ASSERT_EQUAL(static_cast<int>(Locale::DE),
                      static_cast<int>(i18n::get_locale()));
}

// ----------------------------------------------------------------------------
// Locale switching
// ----------------------------------------------------------------------------

void test_set_locale_en()
{
    i18n::set_locale(Locale::EN);
    TEST_ASSERT_EQUAL(static_cast<int>(Locale::EN),
                      static_cast<int>(i18n::get_locale()));
}

void test_set_locale_de()
{
    i18n::set_locale(Locale::EN);
    i18n::set_locale(Locale::DE);
    TEST_ASSERT_EQUAL(static_cast<int>(Locale::DE),
                      static_cast<int>(i18n::get_locale()));
}

// ----------------------------------------------------------------------------
// String table — German (default)
// ----------------------------------------------------------------------------

void test_de_app_name()
{
    TEST_ASSERT_EQUAL_STRING("Home Remote", i18n::str(StrId::APP_NAME));
}

void test_de_connecting_ha()
{
    TEST_ASSERT_EQUAL_STRING("Verbindung mit Home Assistant...",
                             i18n::str(StrId::CONNECTING_HA));
}

void test_de_no_rooms()
{
    TEST_ASSERT_EQUAL_STRING("Keine R\xc3\xa4ume in\nHome Assistant gefunden",
                             i18n::str(StrId::NO_ROOMS));
}

void test_de_no_devices()
{
    TEST_ASSERT_EQUAL_STRING("Keine Ger\xc3\xa4te in diesem Raum",
                             i18n::str(StrId::NO_DEVICES));
}

void test_de_wifi_setup_mode()
{
    TEST_ASSERT_EQUAL_STRING("Einrichtungsmodus",
                             i18n::str(StrId::WIFI_SETUP_MODE));
}

void test_de_wifi_connecting()
{
    TEST_ASSERT_EQUAL_STRING("Verbinde...", i18n::str(StrId::WIFI_CONNECTING));
}

void test_de_wifi_ssid_fmt()
{
    TEST_ASSERT_EQUAL_STRING("WLAN: %s", i18n::str(StrId::WIFI_SSID_FMT));
}

void test_de_wifi_connected()
{
    TEST_ASSERT_EQUAL_STRING("Verbunden", i18n::str(StrId::WIFI_CONNECTED));
}

void test_de_wifi_ip_fmt()
{
    TEST_ASSERT_EQUAL_STRING("IP: %s", i18n::str(StrId::WIFI_IP_FMT));
}

void test_de_wifi_no_wifi()
{
    TEST_ASSERT_EQUAL_STRING("Kein WLAN", i18n::str(StrId::WIFI_NO_WIFI));
}

void test_de_wifi_fail()
{
    TEST_ASSERT_EQUAL_STRING("Keine Verbindung.\nHA nicht verf\xc3\xbcgbar.",
                             i18n::str(StrId::WIFI_FAIL));
}

void test_de_detail_brightness()
{
    TEST_ASSERT_EQUAL_STRING("Helligkeit", i18n::str(StrId::DETAIL_BRIGHTNESS));
}

void test_de_detail_color_temp()
{
    TEST_ASSERT_EQUAL_STRING("Farbtemperatur", i18n::str(StrId::DETAIL_COLOR_TEMP));
}

void test_de_detail_target_temp()
{
    TEST_ASSERT_EQUAL_STRING("Solltemperatur", i18n::str(StrId::DETAIL_TARGET_TEMP));
}

void test_de_detail_position()
{
    TEST_ASSERT_EQUAL_STRING("Position", i18n::str(StrId::DETAIL_POSITION));
}

// ----------------------------------------------------------------------------
// String table — English
// ----------------------------------------------------------------------------

void test_en_connecting_ha()
{
    i18n::set_locale(Locale::EN);
    TEST_ASSERT_EQUAL_STRING("Connecting to Home Assistant...",
                             i18n::str(StrId::CONNECTING_HA));
}

void test_en_no_devices()
{
    i18n::set_locale(Locale::EN);
    TEST_ASSERT_EQUAL_STRING("No devices in this room",
                             i18n::str(StrId::NO_DEVICES));
}

void test_en_wifi_setup_mode()
{
    i18n::set_locale(Locale::EN);
    TEST_ASSERT_EQUAL_STRING("Setup Mode", i18n::str(StrId::WIFI_SETUP_MODE));
}

void test_en_wifi_connecting()
{
    i18n::set_locale(Locale::EN);
    TEST_ASSERT_EQUAL_STRING("Connecting...", i18n::str(StrId::WIFI_CONNECTING));
}

void test_en_wifi_ssid_fmt()
{
    i18n::set_locale(Locale::EN);
    TEST_ASSERT_EQUAL_STRING("Wi-Fi: %s", i18n::str(StrId::WIFI_SSID_FMT));
}

void test_en_wifi_no_wifi()
{
    i18n::set_locale(Locale::EN);
    TEST_ASSERT_EQUAL_STRING("No Wi-Fi", i18n::str(StrId::WIFI_NO_WIFI));
}

void test_en_detail_brightness()
{
    i18n::set_locale(Locale::EN);
    TEST_ASSERT_EQUAL_STRING("Brightness", i18n::str(StrId::DETAIL_BRIGHTNESS));
}

void test_en_detail_color_temp()
{
    i18n::set_locale(Locale::EN);
    TEST_ASSERT_EQUAL_STRING("Color Temp", i18n::str(StrId::DETAIL_COLOR_TEMP));
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// Error state strings (TICKET-014)
// ----------------------------------------------------------------------------

void test_de_err_ha_unreachable()
{
    TEST_ASSERT_EQUAL_STRING("HA nicht erreichbar",
                             i18n::str(StrId::ERR_HA_UNREACHABLE));
}

void test_de_err_auth_failed()
{
    TEST_ASSERT_EQUAL_STRING("HA-Token ung\xc3\xbcltig\nNeu einrichten",
                             i18n::str(StrId::ERR_AUTH_FAILED));
}

void test_en_err_ha_unreachable()
{
    i18n::set_locale(Locale::EN);
    TEST_ASSERT_EQUAL_STRING("HA unreachable",
                             i18n::str(StrId::ERR_HA_UNREACHABLE));
}

void test_en_err_auth_failed()
{
    i18n::set_locale(Locale::EN);
    TEST_ASSERT_EQUAL_STRING("Invalid HA token\nReconfigure in Settings",
                             i18n::str(StrId::ERR_AUTH_FAILED));
}

// ----------------------------------------------------------------------------
// nano_backbone OTA status strings (TICKET-017)
// ----------------------------------------------------------------------------

void test_de_nb_status_not_cfg()
{
    TEST_ASSERT_EQUAL_STRING("OTA: Nicht konfiguriert",
                             i18n::str(StrId::NB_STATUS_NOT_CFG));
}

void test_de_nb_status_unreg()
{
    TEST_ASSERT_EQUAL_STRING("OTA: Nicht registriert",
                             i18n::str(StrId::NB_STATUS_UNREG));
}

void test_de_nb_status_ok()
{
    TEST_ASSERT_EQUAL_STRING("OTA: Registriert", i18n::str(StrId::NB_STATUS_OK));
}

void test_de_nb_status_failed()
{
    TEST_ASSERT_EQUAL_STRING("OTA: Fehlgeschlagen", i18n::str(StrId::NB_STATUS_FAILED));
}

void test_de_nb_register_btn()
{
    TEST_ASSERT_EQUAL_STRING("OTA registrieren", i18n::str(StrId::NB_REGISTER_BTN));
}

void test_en_nb_status_ok()
{
    i18n::set_locale(Locale::EN);
    TEST_ASSERT_EQUAL_STRING("OTA: Registered", i18n::str(StrId::NB_STATUS_OK));
}

void test_en_nb_register_btn()
{
    i18n::set_locale(Locale::EN);
    TEST_ASSERT_EQUAL_STRING("Register OTA", i18n::str(StrId::NB_REGISTER_BTN));
}

// ----------------------------------------------------------------------------
// OTA download & flash strings (TICKET-019)
// ----------------------------------------------------------------------------

void test_de_nb_update_btn()
{
    TEST_ASSERT_EQUAL_STRING("Update installieren",
                             i18n::str(StrId::NB_UPDATE_BTN));
}

void test_de_nb_updating()
{
    TEST_ASSERT_EQUAL_STRING("Firmware wird aktualisiert...",
                             i18n::str(StrId::NB_UPDATING));
}

void test_de_nb_update_ok()
{
    TEST_ASSERT_EQUAL_STRING("Erfolgreich. Neustart...",
                             i18n::str(StrId::NB_UPDATE_OK));
}

void test_de_nb_update_fail_net()
{
    TEST_ASSERT_EQUAL_STRING("Download fehlgeschlagen",
                             i18n::str(StrId::NB_UPDATE_FAIL_NET));
}

void test_de_nb_update_fail_hash()
{
    TEST_ASSERT_EQUAL_STRING("Pr\xc3\xbcfsumme ung\xc3\xbcltig",
                             i18n::str(StrId::NB_UPDATE_FAIL_HASH));
}

void test_de_nb_update_fail_flash()
{
    TEST_ASSERT_EQUAL_STRING("Flash fehlgeschlagen",
                             i18n::str(StrId::NB_UPDATE_FAIL_FLASH));
}

void test_de_nb_update_no_release()
{
    TEST_ASSERT_EQUAL_STRING("Kein Update verf\xc3\xbcgbar",
                             i18n::str(StrId::NB_UPDATE_NO_RELEASE));
}

void test_en_nb_update_btn()
{
    i18n::set_locale(Locale::EN);
    TEST_ASSERT_EQUAL_STRING("Install Update", i18n::str(StrId::NB_UPDATE_BTN));
}

void test_en_nb_updating()
{
    i18n::set_locale(Locale::EN);
    TEST_ASSERT_EQUAL_STRING("Updating firmware...", i18n::str(StrId::NB_UPDATING));
}

void test_en_nb_update_ok()
{
    i18n::set_locale(Locale::EN);
    TEST_ASSERT_EQUAL_STRING("Success. Rebooting...", i18n::str(StrId::NB_UPDATE_OK));
}

void test_en_nb_update_fail_net()
{
    i18n::set_locale(Locale::EN);
    TEST_ASSERT_EQUAL_STRING("Download failed", i18n::str(StrId::NB_UPDATE_FAIL_NET));
}

void test_en_nb_update_no_release()
{
    i18n::set_locale(Locale::EN);
    TEST_ASSERT_EQUAL_STRING("No update available",
                             i18n::str(StrId::NB_UPDATE_NO_RELEASE));
}

// ----------------------------------------------------------------------------
// Guard: out-of-range id returns ""
// ----------------------------------------------------------------------------

void test_out_of_range_returns_empty()
{
    // Cast a value beyond _COUNT to StrId
    const StrId invalid = static_cast<StrId>(255);
    TEST_ASSERT_EQUAL_STRING("", i18n::str(invalid));
}

// ----------------------------------------------------------------------------
// Switching mid-session returns the new locale's strings
// ----------------------------------------------------------------------------

void test_switch_de_to_en_returns_en_string()
{
    TEST_ASSERT_EQUAL_STRING("Helligkeit", i18n::str(StrId::DETAIL_BRIGHTNESS));
    i18n::set_locale(Locale::EN);
    TEST_ASSERT_EQUAL_STRING("Brightness", i18n::str(StrId::DETAIL_BRIGHTNESS));
}

void test_switch_en_to_de_returns_de_string()
{
    i18n::set_locale(Locale::EN);
    TEST_ASSERT_EQUAL_STRING("Brightness", i18n::str(StrId::DETAIL_BRIGHTNESS));
    i18n::set_locale(Locale::DE);
    TEST_ASSERT_EQUAL_STRING("Helligkeit", i18n::str(StrId::DETAIL_BRIGHTNESS));
}

// ----------------------------------------------------------------------------
// Entry point
// ----------------------------------------------------------------------------

int main()
{
    UNITY_BEGIN();

    RUN_TEST(test_default_locale_is_de);

    RUN_TEST(test_set_locale_en);
    RUN_TEST(test_set_locale_de);

    // German strings
    RUN_TEST(test_de_app_name);
    RUN_TEST(test_de_connecting_ha);
    RUN_TEST(test_de_no_rooms);
    RUN_TEST(test_de_no_devices);
    RUN_TEST(test_de_wifi_setup_mode);
    RUN_TEST(test_de_wifi_connecting);
    RUN_TEST(test_de_wifi_ssid_fmt);
    RUN_TEST(test_de_wifi_connected);
    RUN_TEST(test_de_wifi_ip_fmt);
    RUN_TEST(test_de_wifi_no_wifi);
    RUN_TEST(test_de_wifi_fail);
    RUN_TEST(test_de_detail_brightness);
    RUN_TEST(test_de_detail_color_temp);
    RUN_TEST(test_de_detail_target_temp);
    RUN_TEST(test_de_detail_position);

    // English strings
    RUN_TEST(test_en_connecting_ha);
    RUN_TEST(test_en_no_devices);
    RUN_TEST(test_en_wifi_setup_mode);
    RUN_TEST(test_en_wifi_connecting);
    RUN_TEST(test_en_wifi_ssid_fmt);
    RUN_TEST(test_en_wifi_no_wifi);
    RUN_TEST(test_en_detail_brightness);
    RUN_TEST(test_en_detail_color_temp);

    // Error state strings (TICKET-014)
    RUN_TEST(test_de_err_ha_unreachable);
    RUN_TEST(test_de_err_auth_failed);
    RUN_TEST(test_en_err_ha_unreachable);
    RUN_TEST(test_en_err_auth_failed);

    // nano_backbone OTA strings (TICKET-017)
    RUN_TEST(test_de_nb_status_not_cfg);
    RUN_TEST(test_de_nb_status_unreg);
    RUN_TEST(test_de_nb_status_ok);
    RUN_TEST(test_de_nb_status_failed);
    RUN_TEST(test_de_nb_register_btn);
    RUN_TEST(test_en_nb_status_ok);
    RUN_TEST(test_en_nb_register_btn);

    // nano_backbone OTA download & flash strings (TICKET-019)
    RUN_TEST(test_de_nb_update_btn);
    RUN_TEST(test_de_nb_updating);
    RUN_TEST(test_de_nb_update_ok);
    RUN_TEST(test_de_nb_update_fail_net);
    RUN_TEST(test_de_nb_update_fail_hash);
    RUN_TEST(test_de_nb_update_fail_flash);
    RUN_TEST(test_de_nb_update_no_release);
    RUN_TEST(test_en_nb_update_btn);
    RUN_TEST(test_en_nb_updating);
    RUN_TEST(test_en_nb_update_ok);
    RUN_TEST(test_en_nb_update_fail_net);
    RUN_TEST(test_en_nb_update_no_release);

    // Boundary
    RUN_TEST(test_out_of_range_returns_empty);

    // Mid-session switching
    RUN_TEST(test_switch_de_to_en_returns_en_string);
    RUN_TEST(test_switch_en_to_de_returns_de_string);

    return UNITY_END();
}
