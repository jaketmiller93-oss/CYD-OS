# Paged native UI — 0.4.10

The native device UI uses explicit page changes rather than drag scrolling. The
fixed bottom bar has three touch buttons, sized for the current orientation:

- **Back:** previous page; from a section's first page, return to its parent.
- **Home:** stop the current Lua app and close native panels to the launcher.
- **Next:** next page; disabled when no further page is available.

Home also shows the current page/count. The header **Up** shortcut returns
straight to the section parent. These are on-screen controls, not GPIO buttons.
Buttons at the ends are visibly disabled. A running Lua app with no shell-level
history returns to Home on Back; its Next is disabled.

## Native screens

Settings, its display/clock/monitor/test/developer pages, Connections (including
scan/saved/join/AP/device access), app/folder grids, saved versions, folder actions,
move/confirmation dialogs, and Logs use the pager. Controls and icon rows stay
whole. Long labels have clipped viewports with cuts at complete text lines; the
original label remains the source, so diagnostics can still update. Font/layout
use the current screen width/height. Rotation rebuilds native pagination from
page one. Folder refreshes/new dialogs likewise start on their first page.

Settings option dropdowns are replaced with tap-to-cycle values so short screens
do not need to drag a popup list. Label/control pairs stay together when possible.
Keyboards retain their accept/cancel keys and leave the bottom bar exposed; Back
cancels the current editor. Home closes connection editors and retains the
existing password-buffer wiping behavior. Calibration still uses the full touch
plane and its existing hold-anywhere-for-two-seconds cancellation gesture; it is
an exception to the normal bottom bar while touch is captured for calibration.

No native page uses a scroll offset to switch pages. The page manager freezes the
native flex layout, positions complete groups within the viewport, and hides
other-page objects. It disables body drag scrolling, momentum, elasticity and
scroll chaining. No intermediate scroll positions or transition animation are
created. A page may still visibly repaint from top to bottom: this release does
not change RGB scanout, framebuffer buffering, touch transport or panel timing.

Custom Lua apps retain their API and authored layouts/scrolling views. The shell
adds no new Lua Back/Next callback contract and does not automatically repack
arbitrary application widgets. Calculator can keep its existing package. Home
continues to stop apps. This release changes native navigation, not every custom
app's internal navigation. The Safari dashboard remains unchanged.

## Startup and preservation

The known working 0.4.9 S3 control-bus recovery and the 0.4.6 PSRAM scratch-buffer
fix remain. S3 board driver, control sequencer, memory defaults, boot sequence,
partition table, OTA validator and rollback manager are unchanged from 0.4.9.
No new permanent task is added; a small existing-loop timer updates the footer.
Pager allocations belong to each native body and are released with it. No extra
text copies are made to split long labels. C5 retains its own SPI/touch path.

OS settings, calibration, network/access credentials, app packages, versions,
folders and app data are retained through normal OTA. No direct module pin access
or paired-board transport is activated. No firmware is installed by preparing the
files. 0.4.9 is the last observed running S3 firmware until the user updates.

## Validation and limits

182 host regressions pass. Real LVGL/ASan/UBSan checks cover complete control/icon
rows, pointer hits on visible pages, long UTF-8 text, live-text shrinking/clamping,
reset/resize and repeated page deletion. Real shell tests exercise the three
footer buttons, first/last states, parent exit, Home, keyboard cancellation,
Connections/Apps/Logs, and rotation. Existing folder/version and saved-setting
operations run at 240x320, 320x240, 800x480 and 480x800. Input/RTOS/NVS/heap/board
are doubles. Rendering previews show host fixtures, not physical device photos.

Both ESP-IDF 5.5.4 board builds are prepared with blank initial private fields;
OTA identities/layout/hash/Lua ABI and unchanged startup sources are checked.
Physical 0.4.10 boot, touch usability and perceived repaint speed remain unverified.
The tests do not measure actual S3 PSRAM bandwidth or synchronize panel refresh.

Install only the application image for the selected board through Safari >
CYD-OS > OS update > Upload & verify OS, then Restart into update. No SD or USB
cable is required for OS OTA. Keep power applied through startup confirmation.
Rollback remains enabled; do not force an image that fails its checks. The S3
and C5 application .bin files are different and cannot be interchanged.
