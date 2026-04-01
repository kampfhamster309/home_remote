#pragma once

// In-app settings screen.
//
// Accessible via the gear icon button in the shell header.
// Covers: language (DE/EN), display brightness (PWM), touch re-calibration.
// All changes persist to NVS immediately (brightness on slider release).
//
// Locale change and re-calibration both rebuild the shell UI on completion.

namespace settings_screen {

// Open the settings screen over the current active screen.
// No-op if already open.
void open();

// True while the settings screen is displayed.
bool is_open();

} // namespace settings_screen
