# Development tablet — C5 0.4.5

This release expands the native Settings application into a small troubleshooting
workbench. It includes all earlier upload, app-memory, Lua ABI and icon-grid fixes.

## Use the new controls

- **Display & touch:** saved idle dimming after Never/30s/1m/2m/5m, with a 10/25/50%
  idle brightness. Never is the upgrade default. Dimming never raises brightness
  and does not sleep the CPU, disconnect Wi-Fi or stop an app. The first touch
  wakes the display and is consumed until released. Tap again to operate it.
  Existing brightness, orientation preview/revert and calibration remain.
- **Clock:** saved 12/24-hour format; timezone steps are now 15 minutes. Existing
  date/time editing, internet synchronization and Safari clock sync remain.
- **Live system monitor:** uptime, total free heap, largest allocatable block,
  internal largest block, detected PSRAM capacity, SD mount state, network
  address summary, effective/configured backlight and touch press count.
  Pause/resume readings or cycle 1s/250ms/2s sampling. Home's Diagnostics shortcut
  opens this live monitor too. Detected PSRAM is capacity, not free PSRAM. Network
  addresses do not prove internet reachability; SD readiness is not a media test.
- **Touch input test:** last pressed raw controller coordinates, calibrated native
  pixels, down/released state, press count, age and a moving marker in a test pad.
  Scroll to the pad and drag inside it. The marker transforms coordinates for
  the current orientation. Input values are touch samples, not GPIO voltages.
  A zero count means no pressed sample has been observed since boot.
- **Screen color test:** cycle a patch through red, green, blue, white and black.
  This is a visual test, not automated stuck-pixel detection.
- **Developer & modules:** supported app features, board-used pin references and
  extension constraints. Pins describe compiled drivers; they do not certify a
  connector's availability. No probes, pin changes or I2C scans are performed.
- **About this tablet:** OS, compiled board profile, actual chip cores/revision,
  flash/PSRAM capacities, reset-reason code, IDF version and native panel size.
  Board profile names do not physically identify the PCB revision.

All native pages scroll on the small C5 screen. Settings outside the existing
Connections/access controls are changed locally on the device in this release;
new preferences have not been added to the Safari dashboard.

## Build small applications

Use the actual [API](API.md), [platform extensions](LUA-API-0.4.md) and
[package tool](../tools/cydctl). Target both `esp32c5` and `esp32s3` in a manifest
when the app is portable. Calculate layout from `cyd.ui.size()` instead of fixing
it to the C5 resolution. A compatible package does not make S3 display support
work; its black-screen bring-up remains a separate task.

The entry script creates UI and returns. Use `cyd.timer.every` and callbacks,
not infinite loops or busy polling. Prefer slow sampling and a bounded history.
The C5 runs one foreground Lua app; Home / Stop terminates it. Print messages
appear in Logs. Use a stable package_id so versions stay grouped together.
Keep app data in the app's storage area and secrets in its credential store.
Existing limits and permissions are unchanged in this release.

`apps/platform_demo` demonstrates UI, timers, JSON, resources, HTTP and MQTT.
It is source for a separately packaged app, not an installed firmware feature.

## Add sensors and modules

A module with its own Wi-Fi HTTP/MQTT endpoint can be used by an app today using
`network` / `mqtt` permissions. Verify its protocol, payload size and update rate;
network requests must remain bounded. No module has been physically tested here.

Direct wired GPIO/ADC/I2C/SPI/UART access is still unavailable to apps. On the C5,
LCD, touch and SD share SPI GPIO6/2/7; their select/control pins are already owned
by the board driver. BOOT GPIO28 and USB/flash/PSRAM must remain reserved.
Do not interpret an unlisted pin as a verified free header pin.

The next wired-module layer should add, in this order:

1. A verified per-board pin/connector capability table and resource leases.
2. Serialized bus access with deadlines, reserved-pin checks and driver errors.
3. Bounded sensor sampling and latest-value snapshots, with cleanup on app stop.
4. Explicit manifest permissions and small Lua sensor handles.
5. After S3 bring-up, a versioned C5–S3 transport with role/capability discovery,
   reconnect handling and bounded queues. Qualify wiring before enabling it.

The intended division is S3-hosted UI/apps and C5 module services where useful.
This firmware does not pair the boards or offload computation yet. Avoid adding
new radio stacks, large buffers or permanent per-sensor tasks to the C5 without
measuring internal heap and the largest internal block under load.

## Implementation and validation

Preferences are stored separately in `cyd_prefs`, leaving the existing format-1
`cyd_device/display` calibration record and all network/access/app state intact.
Changes use the existing queued settings worker; there is no new permanent task.
Dimming runs in that worker and is suspended during maintenance/display previews.
The native UI reuses its existing 200ms timer; readings only update at the selected
sampling interval while the page is visible. No background hardware polling loop
or task enumeration was added. Touch telemetry comes from existing driver reads.

141 host regression tests pass. New checks cover preference persistence, invalid
values, failed commits, dim thresholds, wake gesture consumption, touch snapshots,
maintenance and dim-level clamping. Actual LVGL plus the real settings service
runs under ASan/UBSan at 240x320, 320x240, 800x480 and 480x800, exercising dropdowns,
pause/refresh, touch/color tests, navigation and resizing. Board/RTOS/NVS/touch
inputs in these tests are doubles. Preview images show fixture readings.
Credential-free C5 0.4.5 OTA built with ESP-IDF 5.5.4; its descriptor/image hash and
Lua 64-bit ABI are checked. No device was flashed. Physical display/touch/dimming
acceptance remains pending; no new S3 image was built in this iteration.
