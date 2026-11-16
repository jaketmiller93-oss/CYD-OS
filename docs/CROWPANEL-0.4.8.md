# CrowPanel 0.4.8 — narrowed OTA scroll candidate

## Observed behavior

0.4.6 is physically installed and the user confirmed visible home and working
touch. They reported scrolling leaves the interface sluggish. The exact affected
page/orientation is not yet known. 0.4.7 was installed by the user through OTA;
it showed black for about 45 seconds and automatically returned to 0.4.6.
That matches the startup-health deadline, but no failed-boot log is available.
The exact failing stage is unknown. 0.4.7 is withdrawn; do not disable rollback.

## Scope of this candidate

0.4.8 starts from the exact working 0.4.6 source bundle, rather than patching
around the unverified 0.4.7 hardware changes. Board source, board CMake, saved
memory defaults and main boot sequence match 0.4.6 byte-for-byte. Touch sampling,
I2C speed, polling timer, panel timing, pins and UI task creation/placement are
restored to 0.4.6. The PSRAM scratch buffers that resolved initial startup remain.

The only production changes from 0.4.6 are in ui.c and settings.c:

- Settings notice, clock and brightness/preview labels update only when visible
  text changes. Previously they freed/reallocated/invalidate text every 200ms.
- Shell clock and network labels likewise update only when text changes.
- Touch/flush/handler durations are measured, and once per 10s a [ui-perf] report
  goes to the existing OS Logs. The formatting buffer is static, avoiding an extra
  160-byte buffer on the UI task's drawing call stack. Reports include peak
  callback durations (microseconds) and flush chunk count; chunk count is not FPS.
  Peaks are separate maxima and need not describe the same event.

No input scheduling/core changes or new touch reader are present. No new permanent
task, heap buffer, external module API or pairing connection is added. This does
not claim to fix all sources of scroll lag. The known unbounded SDK I2C path is
retained for now to keep this hardware iteration narrow; improvements to it need
separate hardware qualification.

## Validation / installation

170 host regressions pass, including real-LVGL no-redraw checks on settled Display
settings, actual shell startup/all rotations and allocation-failure cleanup. Tests
use board/RTOS/NVS/heap doubles; they do not reproduce the real 0.4.7 startup stall
or measure the CrowPanel's frame rate. S3 built with ESP-IDF 5.5.4. App identity,
layout, image hash, Lua ABI and blank private initial fields are verified. No
installation of 0.4.8 has been performed here; 0.4.6 remains the confirmed device OS.

Install cyd-os-0.4.8-crowpanel_s3-ota.bin in the CrowPanel Safari dashboard's
OS update > Upload & verify OS, then Restart into update. It is an application
image for the S3, not an app package or merged USB image. No SD card/USB cable is
needed. Existing settings and app files are kept. Keep the board powered until
startup confirmation; rollback remains enabled and unchanged.

If startup succeeds, compare the same scroll in the same orientation. If lag
persists, copy [ui-perf] lines from Logs covering idle, scroll, then idle again,
and identify the page/orientation. If startup rolls back, stay on 0.4.6 and report
its timing rather than repeatedly forcing the candidate. The next diagnostic
step depends on that result. A failed candidate's RAM Logs do not survive reboot.
