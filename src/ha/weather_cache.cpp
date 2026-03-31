#include "weather_cache.h"

#include <cstring>
#include <cstdlib>

#include "entity_cache.h"

// Icon strings are UTF-8-encoded FA5 codepoints (same values as ui_icons.h macros).
// Defined here to keep weather_cache.cpp free of LVGL header dependencies,
// so it can be compiled in native unit tests without the LVGL library.
#define WI_SUN           "\xEF\x86\x85"  // U+F185 fa-sun
#define WI_MOON          "\xEF\x86\x86"  // U+F186 fa-moon
#define WI_CLOUD         "\xEF\x83\x82"  // U+F0C2 fa-cloud
#define WI_CLOUD_SUN     "\xEF\x9B\x84"  // U+F6C4 fa-cloud-sun
#define WI_CLOUD_RAIN    "\xEF\x9C\xBD"  // U+F73D fa-cloud-rain
#define WI_CLOUD_SHOWERS "\xEF\x9D\x80"  // U+F740 fa-cloud-showers-heavy
#define WI_SNOWFLAKE     "\xEF\x8B\x9C"  // U+F2DC fa-snowflake
#define WI_WIND          "\xEF\x9C\xAE"  // U+F72E fa-wind
#define WI_SMOG          "\xEF\x9D\x9F"  // U+F75F fa-smog
#define WI_BOLT          "\xEF\x83\xA7"  // U+F0E7 fa-bolt (already in font)
#define WI_COG           "\xEF\x80\x93"  // U+F013 fa-cog  (fallback)

// ----------------------------------------------------------------------------
// Internal state
// ----------------------------------------------------------------------------

namespace {

static WeatherData s_data{};

// HA condition → icon + labels
struct ConditionEntry {
    const char* slug;
    const char* icon;   // UTF-8 FA5 codepoint macro expansion
    const char* de;
    const char* en;
};

// Conditions that map to icons already compiled into lv_font_icons_20
// (i.e. glyphs present BEFORE the TICKET-012a font regen):
//   UI_ICON_BOLT  (U+F0E7) — already compiled in
// All other weather icons require regenerating lv_font_icons_20 with the
// new codepoints (see human_to_do.md for the lv_font_conv command).
static const ConditionEntry s_conditions[] = {
    { "sunny",           WI_SUN,           "Sonnig",                "Sunny"         },
    { "clear-night",     WI_MOON,          "Klare Nacht",           "Clear Night"   },
    { "partlycloudy",    WI_CLOUD_SUN,     "Bew\xc3\xb6lkt+",      "Partly Cloudy" },
    { "cloudy",          WI_CLOUD,         "Bew\xc3\xb6lkt",        "Cloudy"        },
    { "fog",             WI_SMOG,          "Nebel",                 "Fog"           },
    { "hail",            WI_CLOUD_SHOWERS, "Hagel",                 "Hail"          },
    { "lightning",       WI_BOLT,          "Gewitter",              "Lightning"     },
    { "lightning-rainy", WI_BOLT,          "Gewitterregen",         "Thunderstorm"  },
    { "pouring",         WI_CLOUD_SHOWERS, "Starkregen",            "Pouring"       },
    { "rainy",           WI_CLOUD_RAIN,    "Regen",                 "Rainy"         },
    { "snowy",           WI_SNOWFLAKE,     "Schnee",                "Snowy"         },
    { "snowy-rainy",     WI_CLOUD_RAIN,    "Schneeregen",           "Sleet"         },
    { "windy",           WI_WIND,          "Windig",                "Windy"         },
    { "windy-variant",   WI_WIND,          "Windig+",               "Windy+"        },
    { "exceptional",     WI_COG,           "Au\xc3\x9f" "ergew.",   "Exceptional"   },
};
static constexpr size_t COND_COUNT =
    sizeof(s_conditions) / sizeof(s_conditions[0]);

static const ConditionEntry* find_condition(const char* slug)
{
    for (size_t i = 0; i < COND_COUNT; ++i) {
        if (strcmp(s_conditions[i].slug, slug) == 0) {
            return &s_conditions[i];
        }
    }
    return nullptr;
}

static void apply_entity_state(const HaEntity& entity)
{
    strncpy(s_data.entity_id,  entity.entity_id, sizeof(s_data.entity_id)  - 1);
    strncpy(s_data.condition,  entity.state,      sizeof(s_data.condition)  - 1);
    s_data.entity_id[sizeof(s_data.entity_id) - 1] = '\0';
    s_data.condition[sizeof(s_data.condition) - 1]  = '\0';

    // HA weather entities store the current temperature in attributes.temperature.
    // entity_cache maps attributes.temperature → attrs.target_temp (shared with CLIMATE).
    // Use target_temp here; has_target_temp guards its validity.
    s_data.temperature     = entity.attrs.target_temp;
    s_data.has_temperature = entity.attrs.has_target_temp;
    s_data.valid           = true;
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

namespace weather_cache {

void init_from_cache()
{
    s_data = {};

    const size_t n = entity_cache::count();
    for (size_t i = 0; i < n; ++i) {
        const HaEntity* e = entity_cache::get(i);
        if (e && e->domain == EntityDomain::WEATHER) {
            apply_entity_state(*e);
            return; // use the first weather entity found
        }
    }
}

void update_from_entity(const HaEntity& entity)
{
    if (entity.domain != EntityDomain::WEATHER) return;
    // Only update if it matches the tracked entity (or we don't have one yet).
    if (s_data.valid && strcmp(s_data.entity_id, entity.entity_id) != 0) return;
    apply_entity_state(entity);
}

void set_forecast_response(const char* entity_id, const JsonObject& forecast_obj)
{
    if (!s_data.valid) return;
    if (strcmp(s_data.entity_id, entity_id) != 0) return;

    // forecast_obj is result.response[entity_id]
    // It has a "forecast" array; we only read the first entry (today).
    JsonArrayConst forecast = forecast_obj["forecast"].as<JsonArrayConst>();
    if (forecast.isNull() || forecast.size() == 0) return;

    JsonObjectConst today = forecast[0].as<JsonObjectConst>();
    if (today.isNull()) return;

    if (today.containsKey("temperature")) {
        s_data.temp_high     = today["temperature"].as<float>();
        s_data.has_forecast  = true;
    }
    if (today.containsKey("templow")) {
        s_data.temp_low     = today["templow"].as<float>();
        s_data.has_forecast = true;
    }
    if (today.containsKey("precipitation_probability")) {
        s_data.precip_probability =
            static_cast<uint8_t>(today["precipitation_probability"].as<int>());
        s_data.has_forecast = true;
    }
}

bool has_weather()
{
    return s_data.valid;
}

const char* get_entity_id()
{
    return s_data.valid ? s_data.entity_id : "";
}

const WeatherData& get()
{
    return s_data;
}

const char* condition_icon(const char* condition)
{
    const ConditionEntry* e = find_condition(condition);
    return e ? e->icon : WI_COG;
}

const char* condition_label(const char* condition, bool is_de)
{
    const ConditionEntry* e = find_condition(condition);
    if (!e) return condition; // fallback: raw slug
    return is_de ? e->de : e->en;
}

} // namespace weather_cache
