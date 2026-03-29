# Home Remote

Copyright 2026 Felix Harenbrock — Licensed under the [Apache License 2.0](LICENSE).

Touch-screen smart home controller for the ESP32-2432S028 (CYD — Cheap Yellow Display).
Controls Home Assistant devices via the local network. Supports all devices registered in Home Assistant, including HomeKit-proxied devices.

## Hardware

| Component | Spec |
|---|---|
| Board | ESP32-2432S028 |
| Display | 2.8" ILI9341, 320×240, RGB565 |
| Touch | XPT2046 resistive, separate SPI bus |
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

To re-enter setup mode, use the settings submenu on the device.

### Generating a Home Assistant Long-Lived Access Token

1. In Home Assistant, go to your profile (bottom-left avatar).
2. Scroll to **Long-Lived Access Tokens** → **Create Token**.
3. Give it a name (e.g. `home-remote`) and copy the token — it is only shown once.

## Project Structure

```
home_remote/
├── platformio.ini          # Build config
├── include/
│   ├── display_config.h    # Pin assignments and screen constants
│   └── lv_conf.h           # LVGL v8 configuration
├── src/
│   ├── main.cpp
│   ├── ui/                 # LVGL screens and widgets (TICKET-007+)
│   ├── ha/                 # Home Assistant client (TICKET-004+)
│   ├── config/             # NVS read/write helpers (TICKET-003+)
│   └── i18n/               # DE/EN string tables (TICKET-011+)
└── test/
    └── test_display_config/ # Native unit tests for pin/screen config
```

## Development Plan

See `development_plan.md` for the full ticket list.
