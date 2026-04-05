#include "nvs_config.h"
#include <Preferences.h>

// NVS namespace and key names (Preferences limits: 15 chars each)
static constexpr char NVS_NS_TOUCH[]  = "touch_cal";
static constexpr char NVS_KEY_X1[]    = "x1r";
static constexpr char NVS_KEY_Y1[]    = "y1r";
static constexpr char NVS_KEY_X2[]    = "x2r";
static constexpr char NVS_KEY_Y2[]    = "y2r";
static constexpr char NVS_KEY_VALID[] = "valid";

namespace nvs_config {

bool load_touch_cal(TouchCalibration& out)
{
    Preferences prefs;
    prefs.begin(NVS_NS_TOUCH, /* readOnly= */ true);

    const bool valid = prefs.getBool(NVS_KEY_VALID, false);
    if (valid) {
        out.x1_raw = prefs.getShort(NVS_KEY_X1, 0);
        out.y1_raw = prefs.getShort(NVS_KEY_Y1, 0);
        out.x2_raw = prefs.getShort(NVS_KEY_X2, 0);
        out.y2_raw = prefs.getShort(NVS_KEY_Y2, 0);
    }

    prefs.end();
    return valid;
}

void save_touch_cal(const TouchCalibration& cal)
{
    Preferences prefs;
    prefs.begin(NVS_NS_TOUCH, /* readOnly= */ false);

    prefs.putShort(NVS_KEY_X1, cal.x1_raw);
    prefs.putShort(NVS_KEY_Y1, cal.y1_raw);
    prefs.putShort(NVS_KEY_X2, cal.x2_raw);
    prefs.putShort(NVS_KEY_Y2, cal.y2_raw);
    prefs.putBool(NVS_KEY_VALID, true);

    prefs.end();
}

void clear_touch_cal()
{
    Preferences prefs;
    prefs.begin(NVS_NS_TOUCH, /* readOnly= */ false);
    prefs.clear();
    prefs.end();
}

// ---- Network / provisioning config -----------------------------------------

static constexpr char NVS_NS_NET[]      = "net_cfg";
static constexpr char NVS_KEY_SSID[]    = "ssid";
static constexpr char NVS_KEY_PW[]      = "pw";
static constexpr char NVS_KEY_HA_URL[]  = "ha_url";
static constexpr char NVS_KEY_HA_TOK[]  = "ha_token";
// reuses NVS_KEY_VALID ("valid") with a different namespace

bool load_net_config(NetworkConfig& out)
{
    Preferences prefs;
    prefs.begin(NVS_NS_NET, /* readOnly= */ true);

    const bool valid = prefs.getBool(NVS_KEY_VALID, false);
    if (valid) {
        prefs.getString(NVS_KEY_SSID,   out.ssid,     sizeof(out.ssid));
        prefs.getString(NVS_KEY_PW,     out.password, sizeof(out.password));
        prefs.getString(NVS_KEY_HA_URL, out.ha_url,   sizeof(out.ha_url));
        prefs.getString(NVS_KEY_HA_TOK, out.ha_token, sizeof(out.ha_token));
    }

    prefs.end();
    return valid;
}

void save_net_config(const NetworkConfig& cfg)
{
    Preferences prefs;
    prefs.begin(NVS_NS_NET, /* readOnly= */ false);

    prefs.putString(NVS_KEY_SSID,   cfg.ssid);
    prefs.putString(NVS_KEY_PW,     cfg.password);
    prefs.putString(NVS_KEY_HA_URL, cfg.ha_url);
    prefs.putString(NVS_KEY_HA_TOK, cfg.ha_token);
    prefs.putBool(NVS_KEY_VALID, true);

    prefs.end();
}

void clear_net_config()
{
    Preferences prefs;
    prefs.begin(NVS_NS_NET, /* readOnly= */ false);
    prefs.clear();
    prefs.end();
}

// ---- nano_backbone OTA config ----------------------------------------------

static constexpr char NVS_NS_NB[]      = "nb_cfg";
static constexpr char NVS_KEY_NB_URL[] = "nb_url";
static constexpr char NVS_KEY_NB_KEY[] = "nb_api_key";
// reuses NVS_KEY_VALID ("valid") with a different namespace

bool load_nb_config(NanoBackboneConfig& out)
{
    Preferences prefs;
    prefs.begin(NVS_NS_NB, /* readOnly= */ true);

    const bool valid = prefs.getBool(NVS_KEY_VALID, false);
    if (valid) {
        prefs.getString(NVS_KEY_NB_URL, out.nb_url,     sizeof(out.nb_url));
        prefs.getString(NVS_KEY_NB_KEY, out.nb_api_key, sizeof(out.nb_api_key));
    }

    prefs.end();
    return valid && out.nb_url[0] != '\0';
}

void save_nb_config(const NanoBackboneConfig& cfg)
{
    Preferences prefs;
    prefs.begin(NVS_NS_NB, /* readOnly= */ false);

    prefs.putString(NVS_KEY_NB_URL, cfg.nb_url);
    prefs.putString(NVS_KEY_NB_KEY, cfg.nb_api_key);
    prefs.putBool(NVS_KEY_VALID, true);

    prefs.end();
}

void save_nb_api_key(const char* key)
{
    Preferences prefs;
    prefs.begin(NVS_NS_NB, /* readOnly= */ false);

    prefs.putString(NVS_KEY_NB_KEY, key);
    // Don't touch NVS_KEY_VALID or URL — they were set when nb_url was saved.

    prefs.end();
}

void clear_nb_api_key()
{
    Preferences prefs;
    prefs.begin(NVS_NS_NB, /* readOnly= */ false);
    prefs.putString(NVS_KEY_NB_KEY, "");
    prefs.end();
}

// ---- Mobile / battery mode settings ----------------------------------------

static constexpr char NVS_NS_MOB[]       = "mobile_cfg";
static constexpr char NVS_KEY_BAT_MODE[] = "bat_mode";
static constexpr char NVS_KEY_SLEEP_TO[] = "sleep_tmout";

bool load_mobile_settings(MobileSettings& out)
{
    Preferences prefs;
    prefs.begin(NVS_NS_MOB, /* readOnly= */ true);

    const bool valid = prefs.getBool(NVS_KEY_VALID, false);
    if (valid) {
        out.battery_mode    = prefs.getBool(NVS_KEY_BAT_MODE, false);
        out.sleep_timeout_s = prefs.getUShort(NVS_KEY_SLEEP_TO, 30);
    }

    prefs.end();
    return valid;
}

void save_mobile_settings(const MobileSettings& s)
{
    Preferences prefs;
    prefs.begin(NVS_NS_MOB, /* readOnly= */ false);

    prefs.putBool(NVS_KEY_BAT_MODE,  s.battery_mode);
    prefs.putUShort(NVS_KEY_SLEEP_TO, s.sleep_timeout_s);
    prefs.putBool(NVS_KEY_VALID, true);

    prefs.end();
}

// ---- OTA update state (TICKET-020) -----------------------------------------

static constexpr char NVS_KEY_UPD_PEND[] = "upd_pending";
static constexpr char NVS_KEY_BOOT_CTR[] = "boot_ctr";

void set_update_pending()
{
    Preferences prefs;
    prefs.begin(NVS_NS_NB, /* readOnly= */ false);
    prefs.putBool(NVS_KEY_UPD_PEND, true);
    prefs.putUChar(NVS_KEY_BOOT_CTR, 0);
    prefs.end();
}

void increment_boot_count()
{
    Preferences prefs;
    prefs.begin(NVS_NS_NB, /* readOnly= */ false);
    if (!prefs.getBool(NVS_KEY_UPD_PEND, false)) {
        prefs.end();
        return;  // no update pending — nothing to count
    }
    const uint8_t ctr = prefs.getUChar(NVS_KEY_BOOT_CTR, 0);
    prefs.putUChar(NVS_KEY_BOOT_CTR, ctr + 1);
    prefs.end();
}

bool load_update_pending()
{
    Preferences prefs;
    prefs.begin(NVS_NS_NB, /* readOnly= */ true);
    const bool v = prefs.getBool(NVS_KEY_UPD_PEND, false);
    prefs.end();
    return v;
}

uint8_t load_boot_count()
{
    Preferences prefs;
    prefs.begin(NVS_NS_NB, /* readOnly= */ true);
    const uint8_t v = prefs.getUChar(NVS_KEY_BOOT_CTR, 0);
    prefs.end();
    return v;
}

void clear_update_state()
{
    Preferences prefs;
    prefs.begin(NVS_NS_NB, /* readOnly= */ false);
    prefs.putBool(NVS_KEY_UPD_PEND, false);
    prefs.putUChar(NVS_KEY_BOOT_CTR, 0);
    prefs.end();
}

// ---- UI settings -----------------------------------------------------------

static constexpr char NVS_NS_UI[]    = "ui_cfg";
static constexpr char NVS_KEY_BRT[]  = "brightness";
// reuses NVS_KEY_VALID ("valid") with a different namespace

bool load_ui_settings(UiSettings& out)
{
    Preferences prefs;
    prefs.begin(NVS_NS_UI, /* readOnly= */ true);

    const bool valid = prefs.getBool(NVS_KEY_VALID, false);
    if (valid) {
        out.brightness = prefs.getUChar(NVS_KEY_BRT, 255);
    }

    prefs.end();
    return valid;
}

void save_ui_settings(const UiSettings& s)
{
    Preferences prefs;
    prefs.begin(NVS_NS_UI, /* readOnly= */ false);

    prefs.putUChar(NVS_KEY_BRT, s.brightness);
    prefs.putBool(NVS_KEY_VALID, true);

    prefs.end();
}

}  // namespace nvs_config
