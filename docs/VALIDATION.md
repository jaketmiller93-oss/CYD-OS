# Historical validation record

Current 0.4.0 results: [VALIDATION-0.4.md](VALIDATION-0.4.md).
The records below describe earlier versions, not the current checkout.

Recorded 2026-11-13 19:09 EST.

ESP-IDF 5.5.4; LVGL 9.1.0; Lua 5.4.8.

Both builds below enable hardware code only for compile/link verification.
No board, display, touch, SD, Wi-Fi or USB recovery was physically tested.

- **CrowPanel ESP32-S3**: build/link/partition size check passed; 722,912 application-image bytes.
  SHA-256: `6ff47058a02a7f4f6a04af79621c13bbb3ea96dc2b9ac4e577189270391bfe3b`.
  Compiler warnings in final build output: 0.
- **NM-CYD-C5 ESP32-C5**: build/link/partition size check passed; 746,320 application-image bytes.
  SHA-256: `115897ad62fdd63cad340dbfd0cd181e98bfa56d8a99596abad8c1348a807684`.
  Compiler warnings in final build output: 0.

- **27 host tests passed**, including native ZIP/manifest validation with UBSan,
  2,500 malformed archive mutations, both chip targets, authenticated HTTP
  fixture deployment, startup failures, and incompatible-target rejection.
- Dashboard JavaScript syntax, Python syntax and shell script syntax checks passed.

The HTTP fixture is a test double, not an ESP32 simulator. Memory reclamation,
touch calibration, display timing, shared SPI operation and wireless hot restart
still require the physical acceptance procedure in TESTING.md.

Generated images are compile-check artifacts with blank network/API credentials.
Build and configure your own device image using the relevant hardware guide.

## C5 touch mapping follow-up

The user reported a working home screen with touch responding on the opposite
side. Removed the extra reversal of both touch coordinates; display orientation
is unchanged. The updated C5 firmware compiles and passes its partition-size
check. Physical verification of the corrected mapping is pending.

Updated C5 application image SHA-256: `f1a2fa50c76b0c4fa529f699db6d6dd8c39f6f16e53ffbf8b74672f9499e825e`.

## CLI HTTP error reporting follow-up

Non-JSON HTTP errors now retain the response body and identify the request method
and endpoint. Reflected deployment tokens are redacted. All nine CLI tests pass,
including plain-text, empty, invalid UTF-8 and reflected-token error responses.
The cause of the user-reported HTTP 400 is still under investigation.

## Connection manager 0.2 validation

Recorded 2026-11-13 21:26 EST.

The user confirmed the earlier C5 edit/deploy/touchscreen loop works after the
touch correction. The new connection manager is software-validated here and
still needs the physical acceptance checks in CONNECTIONS.md.

- C5: compile/link/partition-size checks passed; 1,321,328 application bytes.
  SHA-256: `04ff293821bd1b9efaaeda8944d36de0190edcc60f85b1e180d43c0c40656e9e`.
  Final build compiler warnings: 0.
- CrowPanel S3: compile/link/partition-size checks passed; 1,177,376 application bytes.
  SHA-256: `8c1aa3987f89777a9ff1461d38c646abee40c5d37411f9e1b4ae2930ef7ef3a7`.
  Final build compiler warnings: 0.

- All 49 Python/native tests passed, including 17 tests of the actual network
  service against radio/NVS/RTOS test doubles and four tests of the update installer.
- Actual LVGL connection views rendered at 320×240 and 800×480. View/input/keyboard
  bounds, valid/invalid entry and keyboard cancel assertions passed.
- Installer path protection tests passed after allowing source sdkconfig.h test
  headers while continuing to protect generated configs.

The preview uses test network names and fake radio/NVS services. It verifies
software layout and callbacks, not RF behavior, actual touch or NVS power loss.

## 0.3.0 OTA / Settings / Apps release

The user has verified the previous 0.2 connection manager, CYDAP and iPad app uploads
on the NM-CYD-C5. Those are the last hardware observations. This release adds OTA,
settings, folders and theme; its new behavior has not been exercised on physical
hardware in this build environment.

Software evidence: ESP-IDF 5.5.4 builds for ESP32-C5 and ESP32-S3; a separate
credential-free C5 OTA image with version override 0.3.1; complete image and custom
metadata validation; native service tests using the actual C sources; browser flow
in Chromium; production LVGL views at 320×240 and 800×480 in all four rotations.
The standard test-suite count/output is recorded in the distributed release README.
No test double verifies physical RF, PWM/STC backlight behavior, LCD flush timing,
raw touch accuracy, flash/NVS power loss, or hardware rollback.

## 0.3.2 remembered-access patch

The dashboard now retains validated credentials in localStorage when “Remember
this device” is enabled (default), migrates existing tab-only access, and supports
forgetting saved access. A 401 clears obsolete credentials and restores login;
connection/service failures retain them. Browser storage exceptions are handled
without preventing sign-in. Opting out remains tab-only across reloads.

The Playwright credential lifecycle test covers fresh browser sessions with saved
website storage, opt-out/new tabs, forgetting access, replaced tokens, legacy
migration, service failures, and disabled browser storage. It runs in Chromium;
physical iPad Safari acceptance is still required. The full suite now has 98 tests.
