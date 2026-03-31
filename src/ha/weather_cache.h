#pragma once

#include <ArduinoJson.h>
#include "ha_entity.h"

// Holds the current state and daily forecast for one weather entity.
struct WeatherData {
    char    entity_id[64];
    char    condition[32];       // HA condition slug, e.g. "sunny", "rainy"
    float   temperature;         // current temperature from attributes
    bool    has_temperature;
    float   temp_high;           // today's forecast high (from get_forecasts)
    float   temp_low;            // today's forecast low
    uint8_t precip_probability;  // 0–100 %
    bool    has_forecast;        // true once get_forecasts response arrived
    bool    valid;               // true once at least one weather entity is known
};

namespace weather_cache {

// Scan entity_cache for the first weather.* entity.
// Call after entity_cache::populate(). Resets all state first.
void init_from_cache();

// Update current condition/temperature from a state_changed event.
// No-op if the entity is not the tracked weather entity.
void update_from_entity(const HaEntity& entity);

// Store daily forecast from a weather.get_forecasts response.
// `entity_id` identifies which entity the response belongs to.
// `forecast_obj` is the JSON object at result.response[entity_id]
// — only the first forecast entry (today) is consumed.
void set_forecast_response(const char* entity_id, const JsonObject& forecast_obj);

// True if a weather entity was found in entity_cache.
bool has_weather();

// entity_id of the tracked weather entity, or "" if none.
const char* get_entity_id();

// Current cached weather data. Only valid when has_weather() is true.
const WeatherData& get();

// Map an HA condition string to an FA5 icon macro string (UTF-8 encoded).
// Returns a fallback icon if the condition is not recognised.
const char* condition_icon(const char* condition);

// Map an HA condition string to a short localised label.
// is_de: true = German, false = English.
const char* condition_label(const char* condition, bool is_de);

} // namespace weather_cache
