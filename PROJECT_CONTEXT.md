# Start here: CYD OS

This repository is the shared source/context for continuing CYD OS in another chat.
Read [the comprehensive project reference](docs/PROJECT-REFERENCE.md), then inspect
the relevant implementation and tests. Earlier chat workspaces are not shared
memory. Do not assume the user has installed the latest prepared firmware.

- Latest prepared source/image: **0.3.2**. Primary hardware: user's working RockBase
  NM-CYD-C5, ESP32-C5HR8, 16 MiB flash/8 MiB PSRAM, ST7789 320×240/XPT2046.
- Secondary profile: CrowPanel Advance DIS01943A V1.1–V1.3, ESP32-S3, 800×480/GT911.
  Builds pass; CrowPanel physical revision/operation remain unverified.
- Goal: iPad/ChatGPT app development, Safari `.cydapp` uploads over Wi-Fi/CYD AP,
  organized app versions and wireless native OS updates; USB for bootstrap/recovery.
- User confirmed C5 display/touch, Wi-Fi, SD app deployment, Connections and CYDAP.
  New 0.3.x features have software checks, not a complete physical acceptance report.
- Implemented: bounded Lua runtime, SD packages/revisions, authenticated dashboard,
  saved Wi-Fi/AP manager, watch-inspired Home, Settings, brightness/orientation/
  calibration/clock, logical Apps folders, A/B OTA and startup rollback.
- 0.3.2 adds default-on **Remember this device**, persistent browser auto-login,
  legacy session migration, tab-only opt-out and Forget saved access. Same browser
  and origin are required. Connection failures retain credentials; 401 clears them.
- CYDAP normally uses `http://192.168.4.1`; mDNS hostname is `cyd.local`. CYDAP has
  no NAT/internet forwarding. iPad may switch between phone hotspot and CYDAP.
- ESP-IDF 5.5.4, LVGL 9.1.0, Lua 5.4.8. User's Debian source:
  `~/cyd-work/cyd-os`; private config `build-device-c5/sdkconfig`; USB `/dev/ttyACM0`.
- Preserve credentials, calibration, SD apps and data. Never commit generated
  sdkconfig/private backups, tokens or real network passwords. Keep board builds
  separate. The C5's BOOT pin is GPIO28, not the S3's GPIO0.
- Apps are textual Lua with the actual limited API, not arbitrary ESP32 firmware.
  Same package ID groups versions; every upload gets a fresh random revision ID.
- Regression evidence: 98 tests, C5/S3 builds, credential-free validated C5 OTA
  artifact and LVGL renders. Browser tests use Chromium/simulated API; native tests
  use SDK doubles. Do not describe them as physical iPad/radio/rollback tests.

Current API details: [API](docs/API.md). Updates/recovery: [OTA](docs/OTA.md),
[Recovery](docs/RECOVERY.md). UI/catalog: [Settings and Apps](docs/SETTINGS-AND-APPS.md).
Hardware sources and acceptance gaps: [Validation](docs/VALIDATION.md).
