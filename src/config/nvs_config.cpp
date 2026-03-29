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

}  // namespace nvs_config
