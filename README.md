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

## How It Works

After connecting to Wi-Fi, the device:

1. Opens a WebSocket to the HA server and authenticates.
2. Fetches the full entity state list (`get_states`).
3. Fetches area, entity, and device registries to resolve which room each entity belongs to.
4. Groups entities by room (HA area). Entities with no area go into an "Other" group.
5. Subscribes to `state_changed` events for live push updates.
6. Control commands are sent via the HA REST API (`POST /api/services/…`).

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
│   └── lv_conf.h           # LVGL v8 configuration
├── src/
│   ├── main.cpp
│   ├── ha/
│   │   ├── ha_client.h/.cpp      # WebSocket + REST client
│   │   ├── entity_cache.h/.cpp   # Flat entity state cache (max 48 entities)
│   │   └── area_cache.h/.cpp     # Area-based entity grouping (max 12 rooms)
│   ├── config/
│   │   └── nvs_config.h/.cpp     # NVS load/save for WiFi, HA, touch cal
│   ├── touch/
│   │   └── touch_driver.h/.cpp   # XPT2046 init, calibration UI, LVGL indev
│   ├── wifi/
│   │   └── wifi_manager.h/.cpp   # Captive portal, connect, reconnect
│   ├── ui/                       # LVGL screens and widgets (TICKET-007+)
│   └── i18n/                     # DE/EN string tables (TICKET-011+)
├── lib/
│   └── XPT2046_Touchscreen/      # Patched local copy (adds begin(SPIClass&))
└── test/
    ├── test_display_config/
    ├── test_touch_mapping/
    ├── test_net_validate/
    ├── test_url_parse/
    ├── test_entity_cache/
    └── test_area_cache/
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
| 011 | Localization (DE/EN) | — |
| 012 | Icon integration | — |
| 013 | Settings submenu | — |
| 014 | Error handling & offline mode | — |
| 016 | Integration testing & hardening | — |
| 017–020 | nano_backbone OTA integration | — |
