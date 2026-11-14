# Third-party sources

- ESP-IDF 5.5.4: Espressif, Apache-2.0 and component-specific licenses. Install
  separately from the official repository; retain its component license notices.
- LVGL 9.1.0: MIT, fetched through ESP-IDF Component Manager.
- Espressif esp_lcd_touch 1.2.1, esp_lcd_touch_gt911 1.1.3 and mDNS 1.8.2:
  fetched through Component Manager; see their bundled license files.
- Lua 5.4.8: MIT, Copyright 1994–2025 Lua.org, PUC-Rio. The source bundle includes
  the verified upstream C sources under components/lua/vendor. The complete
  upstream permission/copyright notice is retained at the end of vendor/lua.h.
  scripts/fetch_lua.py verifies the official archive SHA-256 before extraction.

Manufacturer schematics and examples are linked as hardware references in
HARDWARE.md and HARDWARE-C5.md. They are not redistributed as board support
libraries. Native HAL code uses ESP-IDF APIs and documented pin/timing values.
