#pragma once

#include <lvgl.h>

// Weather content widget — fills the shell content area with today's forecast.
//
// Usage:
//   weather_screen::create(s_content);   // build widgets inside content container
//   weather_screen::refresh();           // call when weather_cache data changes

namespace weather_screen {

// Build all weather widgets as children of `parent`.
// Clears any existing children first.
void create(lv_obj_t* parent);

// Update all labels/icons from the current weather_cache state.
// Call after weather_cache::update_from_entity() or set_forecast_response().
void refresh(lv_obj_t* parent);

} // namespace weather_screen
