# Home Remote

Copyright 2026 Felix Harenbrock — Licensed under the [Apache License 2.0](LICENSE).

Touch-screen smart home controller for the ESP32-2432S028 (CYD — Cheap Yellow Display).
Controls Home Assistant devices via the local network. Supports all devices registered in Home Assistant, including HomeKit-proxied devices.

## Hardware

| Component | Spec |
|---|---|
| Board | ESP32-2432S028 |
| Display | 2.8" ST7789, 320×240, RGB565, HSPI (native pins) |
| Touch | XPT2046 resistive, VSPI (GPIO-matrix routed) |
| Power | USB-C |

## Building & Flashing

### Prerequisites

- [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html) or PlatformIO IDE (VS Code extension)

### Build

```bash
pio run -e cyd
```

### Flash

Connect the board via USB-C, then:

```bash
pio run -e cyd --target upload
```

### Serial Monitor

```bash
pio device monitor
```

### Run Tests (host, no hardware needed)

```bash
pio test -e native
```

## First-Time Setup

On first boot, the device starts in provisioning mode:

1. Connect to the Wi-Fi network **`HomeRemote-Setup`** from your phone or laptop.
2. Open a browser — you will be redirected to the setup page automatically.
3. Enter your Wi-Fi SSID and password, Home Assistant server URL, and a long-lived HA access token.
4. Submit — the device saves the config and reboots into normal mode.

To re-enter setup mode, long-press anywhere on the main screen and then use the settings submenu.

### Generating a Home Assistant Long-Lived Access Token

1. In Home Assistant, go to your profile (bottom-left avatar).
2. Scroll to **Long-Lived Access Tokens** → **Create Token**.
3. Give it a name (e.g. `home-remote`) and copy the token — it is only shown once.

## OTA Firmware Updates

Firmware updates are delivered via [nano_backbone](https://github.com/kampfhamster309/nano_backbone) (Django OTA backend).

1. On first boot the device auto-registers with the nano_backbone server (URL entered during provisioning).
2. A version check runs on boot and every 24 hours in the background (non-blocking FreeRTOS task).
3. When a newer firmware is detected, a badge appears on the settings gear icon.
4. Open **Settings → Install Update** to download and flash the new firmware.  
   The device streams the binary directly into the inactive OTA partition, verifies the SHA-256 checksum, and reboots. The running firmware is unchanged on any error.

## How It Works

After connecting to Wi-Fi, the device:

1. Opens a WebSocket to the HA server and authenticates.
2. Fetches the full entity state list (`get_states`).
3. Fetches area, entity, and device registries to resolve which room each entity belongs to.
4. Groups entities by room (HA area). Entities with no area assignment are silently dropped.
5. If a `weather.*` entity is present, fetches today's forecast via `weather.get_forecasts` and shows a weather tab.
6. Subscribes to `state_changed` events for live push updates.
7. Control commands are sent via the HA REST API (`POST /api/services/…`).

## Project Structure

```
home_remote/
├── platformio.ini
├── include/
│   ├── display_config.h    # Pin assignments and screen constants
│   ├── ha_entity.h         # HaEntity struct and EntityDomain enum
│   ├── ha_area.h           # HaArea struct and MAX_AREAS constant
│   ├── touch_cal.h         # Touch calibration struct and mapping functions
│   ├── net_validate.h      # WiFi/HA config field validators
│   ├── url_parse.h         # HA URL parser (host, port, secure flag)
│   ├── semver.h            # Inline semver parser and comparator
│   └── lv_conf.h           # LVGL v8 configuration
├── src/
│   ├── main.cpp
│   ├── ha/
│   │   ├── ha_client.h/.cpp      # WebSocket + REST client
│   │   ├── entity_cache.h/.cpp   # Flat entity state cache (max 40 entities)
│   │   ├── area_cache.h/.cpp     # Area-based entity grouping (max 12 rooms)
│   │   └── weather_cache.h/.cpp  # Weather entity state + forecast cache
│   ├── config/
│   │   └── nvs_config.h/.cpp     # NVS load/save for WiFi, HA, touch cal, nb OTA
│   ├── touch/
│   │   └── touch_driver.h/.cpp   # XPT2046 init, calibration UI, LVGL indev
│   ├── wifi/
│   │   └── wifi_manager.h/.cpp   # Captive portal (WiFi+HA+nb URL), connect, reconnect
│   ├── nb/
│   │   └── nb_client.h/.cpp      # nano_backbone OTA: register, version check, flash
│   ├── ui/
│   │   ├── shell.h/.cpp          # Nav bar, screen switching, weather tab
│   │   ├── tile_widget.h/.cpp    # Device tile (on/off/unavailable states)
│   │   ├── room_screen.h/.cpp    # Room tile grid
│   │   ├── detail_screen.h/.cpp  # Slider controls for lights, climate, covers
│   │   ├── weather_screen.h/.cpp # Weather condition, temperature, forecast
│   │   ├── settings_screen.h/.cpp# Language toggle, brightness, touch calibration
│   │   ├── ui_theme.h            # Color and spacing constants
│   │   ├── ui_fonts.h            # Custom font extern declarations
│   │   └── ui_icons.h            # FA5 icon UTF-8 macros
│   └── i18n/
│       ├── i18n.h                # StrId enum and locale API
│       └── i18n.cpp              # DE/EN string tables
├── lib/
│   └── XPT2046_Touchscreen/      # Patched local copy (adds begin(SPIClass&))
└── test/
    ├── test_display_config/
    ├── test_touch_mapping/
    ├── test_net_validate/
    ├── test_url_parse/
    ├── test_entity_cache/
    ├── test_area_cache/
    ├── test_detail_screen/
    ├── test_i18n/
    ├── test_weather_cache/
    └── test_version_compare/
```

## Implementation Status

| Ticket | Description | Status |
|---|---|---|
| 001 | Project scaffolding & hardware bringup | ✅ |
| 002 | Touch calibration & LVGL input routing | ✅ |
| 003 | WiFi & captive portal provisioning | ✅ |
| 004 | Home Assistant WebSocket + REST client | ✅ |
| 005 | Entity model & state cache | ✅ |
| 006 | Device grouping by HA area | ✅ |
| 007 | UI shell & navigation framework | ✅ |
| 008 | Device tile widget | ✅ |
| 009 | Group/room screen | ✅ |
| 010 | Device detail/control screen | ✅ |
| 011 | Localization (DE/EN) | ✅ |
| 012 | Icon integration | ✅ |
| 012a | Weather forecast tab | ✅ |
| 013 | Settings submenu | ✅ |
| 014 | Error handling & offline mode | ✅ |
| 016 | Integration testing & hardening | — |
| 017 | nano_backbone device registration | ✅ |
| 018 | Firmware version check client | ✅ |
| 019 | OTA download & flash | ✅ |
| 020 | Post-update reporting & rollback | — |
