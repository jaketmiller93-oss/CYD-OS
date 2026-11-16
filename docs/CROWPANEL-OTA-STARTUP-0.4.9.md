# CrowPanel OTA startup repair candidate — 0.4.9

## Physical evidence collected 2026-11-15

The user reconnected USB for passive serial logging while still performing the
firmware update over Wi-Fi. No reset command, flash write or setting change was
sent through USB. The capture recorded a complete 0.4.8 attempt and rollback:

    I (779) boot: CYD OS 0.4.8 / CrowPanel Advance DIS01943A V1.1-V1.3
    I (908) boot: Initializing board/display/touch
    I (909) board: CrowPanel PSRAM detected: 8388608 bytes
    I (909) board: Starting control/touch I2C on SDA15/SCL16
    I (912) board: Sending STC touch-enable and backlight commands
    E (973) i2c.master: I2C transaction timeout detected
    E (973) board: cyd_board_init(54): backlight
    E (973) boot: Board initialization failed: ESP_ERR_INVALID_STATE
    I (977) main_task: Returned from app_main()
    I (46149) cyd: [os] OS startup checks failed
    I (46280) esp_ota_ops: Rollback to previously worked partition.

The fallback 0.4.6 then initialized its board/UI/API successfully and confirmed
startup. This failed attempt never reached UI initialization. It does not show a
new Lua, PSRAM, touch-read or core-pinning failure. It is specifically a timeout
on the STC backlight startup command after an OTA software restart. The exact
external electrical/controller state responsible for that timeout is not known.
A software restart does not power-cycle the external STC/touch controllers.

## Targeted change

Keep the 0.4.6 touch reader, polling interval, core placement, RGB timing, pins,
PSRAM scratch buffers and saved settings. Retain only 0.4.8's unchanged-label
redraw suppression and timing summaries; do not reintroduce the 0.4.7 touch/core
experiments. New code is limited to the S3 control-bus startup sequence:

1. Wait 50ms after bus setup and use IDF i2c_master_bus_reset to clear bus/FSM state.
2. Send the existing STC touch-enable byte 250.
3. Wait 100ms for controller settling, then send existing backlight byte 0.
4. For ESP_ERR_TIMEOUT or ESP_ERR_INVALID_STATE, reset the bus, wait 100ms and retry
   that same command, at most three attempts total. Permanent errors stop at once.
   A failed recovery also stops. This is not an infinite retry or bypass of errors.

Each command keeps the existing finite 100ms I/O timeout. This sequence runs
before GT911, panel, UI or other bus consumers start; bus recovery is never run
concurrently with UI touch reads. Only the existing OS-owned SDA15/SCL16 bus is
used. No selector, clock, external-module pin, credential, calibration, partition,
link or C5 behavior is changed. Failure still prevents health confirmation and
allows normal rollback. Startup checks are not weakened or disabled.

## Validation and acceptance

178 host tests pass, including eight tests executing the actual new sequencer:
normal startup, the observed failed backlight command, failed touch-enable,
permanent/continuous errors, recovery failures, and invalid callbacks. SDK I2C,
GPIO and delays are doubles; this does not simulate the real electrical bus.
S3 builds with ESP-IDF 5.5.4, validated S3 OTA identity/layout/hash, 64-bit Lua ABI
and blank initial private fields. Actual OTA boot still requires hardware testing.
No installation or successful hardware repair is claimed until the new capture
and user confirmation establish it. 0.4.6 remains the confirmed fallback.

Install cyd-os-0.4.9-crowpanel_s3-ota.bin using the CrowPanel Safari dashboard's
OS update flow and keep USB connected only for the serial capture. Do not retry
0.4.7/0.4.8, force an invalid image into service, or disable rollback. No SD card
or USB firmware write is needed for this OTA. After startup, wait for firmware
confirmation, check visible home/touch, then test scrolling and collect ui-perf
reports if lag persists. The startup repair and scroll tuning are separate checks.


## Subsequent runtime observation

User reports OTA restart. A follow-up capture confirms .9 STC completion, UI/API
startup and firmware health. The original capture expired before the user restart;
the follow-up shows POWERON, so a complete successful warm-OTA transition is not
claimed. See RUNTIME-OBSERVATION.md in the output bundle. Visible-home and scroll
checks remain separate. No USB firmware write or explicit reset sequence was sent.
