# Validation and physical acceptance

No physical display, touchscreen, SD card, Wi-Fi radio, power cycle, or USB
recovery has been tested in this workspace. A successful compile is not hardware
acceptance. Use the checks below before relying on the device.

## Host tests

Linux requires Python 3.10+ and a C compiler supporting UBSan. No board is needed. Activate the installed ESP-IDF environment to also run the
five native manifest tests against its real cJSON source; those tests skip when
`IDF_PATH` is unavailable.

```bash
python -m unittest discover -s tests -v
```

The tests compile the **same C ZIP parser** used on the ESP32 into a host shared
library with undefined-behavior instrumentation, validate the real Hello package,
and exercise malformed paths, CRC, compression, links, header disagreement,
limits, every truncation of a fixture and 2,500 seeded random mutations. They
also run `cydctl` against an authenticated local HTTP fixture to check deployment,
start confirmation, crash reporting, auth errors and redirect credential handling.
The HTTP fixture is a test double; it does not emulate the ESP32 or prove Lua/UI
behavior.

## Compile checks

From an activated ESP-IDF **5.5.4** environment:

```bash
python scripts/fetch_lua.py
idf.py build                       # safe default, board outputs disabled
bash scripts/build_check.sh crowpanel_s3 # compile enabled S3 paths
bash scripts/build_check.sh nm_cyd_c5     # compile enabled C5 paths
```

The isolated `build-check-*` directories use a synthetic confirmation flag only to compile hardware
code. It does not confirm a physical revision and must not be flashed as a
validated release. Configure your real board separately using `idf.py menuconfig`.
Dependency versions/hashes are recorded in `dependencies.lock` and the Lua fetch
script verifies the official source archive SHA-256 before extracting regular
source files. The default and hardware-enabled branches must both build.

## Physical acceptance procedure

1. Check the product/revision label against HARDWARE.md or HARDWARE-C5.md.
   Apply the DIP-switch instructions below only to the CrowPanel; the C5 has a
   shared SPI bus and requires its own touch calibration check. Confirm 16 MiB flash and
   8 MiB PSRAM from serial boot output (octal on S3; quad on C5). Check a FAT32 card with desktop
   tooling, set the later-revision DIP switch to S1=1/S0=1 with power removed,
   insert the card, and configure private Wi-Fi/token values.
2. Flash once over USB. Record boot/reset reason, display stability, detected
   PSRAM, free heap/largest block, touch coordinates, SD availability and actual
   IP. Tap all screen corners and Home. Verify orientation. If GT911 or STC
   initialization fails, stop and compare the exact revision’s official examples.
3. Verify AP mode requires the configured password. Repeat station mode on a
   trusted network. Use the displayed IP when mDNS fails. Verify the shell works
   with Wi-Fi unavailable and starts without an SD card (installation disabled).
4. Open the dashboard from Linux and iPad. Confirm that a missing/wrong token
   returns 401 for **every** API route, including status, logs and downloads.
   Confirm no token/password is included in boot/application logs or URLs.
5. `cydctl deploy apps/hello --device <ip> --run --logs --json`. Verify the
   touchscreen label/button, saved count, lifecycle context and positive startup
   confirmation. Modify the label, redeploy and confirm the new label appears
   without a reset or uptime discontinuity.
6. Tap Home; require a return to shell and `stopped`. Repeat start/stop/redeploy
   100 times; record heap/largest-block baselines after cleanup. Lua/widget/event
   resources must return to a stable plateau (allow initial allocator caching).
   Watch for panel corruption, slow touch, watchdog resets and growing heap loss.
7. Introduce a Lua syntax error, then a click-callback error. Require `crashed`,
   error context in logs, shell recovery and a nonzero CLI exit status. Test
   `while true do end`, excessive `string.rep` allocation, and too many widgets;
   require bounded failure and a working Home path. Measure stop latency.
8. Save two package revisions, run each from touchscreen Projects and the web
   dashboard, download one and compare exact bytes, and delete an inactive one.
   Deletion of a running revision must return 409. App private data must remain
   across code switches. A malformed/oversized upload must not replace any old
   revision or become executable.
9. Interrupt an upload over Wi-Fi, then reboot. Interrupted staging must be
   removed. Completed older revisions remain available. Do controlled power-loss
   testing only with disposable SD data and backups; FAT cannot promise survival
   of arbitrary power failure. SD removal while powered is not supported.
10. Repeat app updates while polling logs and loading SD files. Measure visual
    tearing and touch latency. Reduce pixel clock/adjust buffering only against
    manufacturer timing constraints and observed behavior.
11. Verify ROM BOOT/RESET USB recovery as described in RECOVERY.md. Record the
    exact physical controls and serial-device name for the user’s board.

Acceptance for the central milestone is step 5 repeated reliably with step 6–8
failure recovery. Record board revision, IDF/component versions, power source,
card type and measurements alongside results. Do not mark acceptance complete
until those tests run on the actual device.


## Connection manager tests

The standard Python suite compiles the actual network service/policy against
native radio, NVS and RTOS test doubles. It exercises first-boot import, profile
bounds, commit failures, reconnect/fallback, stale DHCP events, DHCP renewal,
AP/STA transitions, scans and credential-free public status. It also tests the
update installer's backups, checksums and preservation of app/config/calibration.
These test doubles do not verify Wi-Fi radio behavior or real NVS power loss.

After IDF has downloaded the pinned LVGL component, Linux can render the actual
connection views using the native test backend without an attached display:

```bash
cmake -S tests/ui_preview -B build-ui-preview
cmake --build build-ui-preview --parallel
./build-ui-preview/connection_preview 320
./build-ui-preview/connection_preview 800
```

The harness emits PPM previews and asserts that the keyboard/view stay above Home
at both sizes. It also exercises invalid/valid password entry and keyboard
cancel. See docs/previews for example PNGs and CONNECTIONS.md for the required
physical connection-manager acceptance steps.

## 0.3.0 service and dashboard checks

The same standard suite additionally compiles the real OTA, access, device-settings,
geometry, JSON and catalog C sources against native SDK/NVS/RTOS doubles. It checks
interrupted uploads, SDK integrity failure, delayed boot selection, health confirmation,
stale-heartbeat rollback, Lua-stop failure, cancellation, credential persistence,
first-bootstrap calibration persistence, preview timeouts, calibration fits/cancel,
pixel/touch rotation agreement, Gregorian clock parsing, SD journaling and folder bounds.
The optional Playwright test runs the actual dashboard in system Chromium against
a simulated API (iPad viewport, folders, clock, preview, firmware upload/restart).

```bash
python -m unittest discover -s tests -v
# IDF_PATH must point to the activated IDF for actual cJSON tests.
cmake -S tests/ui_preview -B build-ui-preview
cmake --build build-ui-preview --parallel
./build-ui-preview/watch_preview 320
./build-ui-preview/watch_preview 800
```

The shell renderer uses actual production UI files and native LVGL with service
fixtures. It checks flush bounds and emits home/settings/apps/calibration/keyboard
views for all four orientations at each supported native resolution. PNGs in
`docs/previews` are software renderings, not photographs of hardware.

Before field use, on C5: confirm existing Wi-Fi/AP/token/apps survived USB bootstrap;
check all brightness levels, orientation Keep/Revert and timeout, calibration Keep,
hold-to-cancel and timeout; set/sync clock; create/move/reload/delete folders; upload
the matching OTA image through Safari, restart and observe startup confirmation;
verify the previous compatible OS can be restored. Repeat with SD missing and no
internet. Power-interruption/failed-boot rollback tests require controlled hardware
access and a working USB recovery path. S3 additionally needs its first hardware
validation and exact revision confirmation.

The remembered-access browser regression also verifies persistent auto-login in a
fresh browser session, tab-only opt-out across reloads, legacy session migration,
forgetting access, rejected/replaced credentials, service failures and blocked storage.
