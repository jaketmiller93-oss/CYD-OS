# Release lines and Diagnostics location

**0.5 paired line:** both roles use **0.5.0**, from `versions/paired.txt` and
`scripts/build_paired.py`. See [PAIRED-TABLET-0.5.md](PAIRED-TABLET-0.5.md).
C5 0.5 restores a top-level Diagnostics app and owns Settings; S3 has the native
app home. The independent 0.4 defaults below are retained for standalone/recovery
builds and do not enable the unified tablet services.

## Historical independent 0.4 line

The user confirmed the paged S3 UI works and requested independent firmware labels:

| Board | Current prepared label | Next label on a later board update |
| --- | --- | --- |
| CrowPanel Advance / ESP32-S3 | **0.4.3s** | 0.4.4s |
| NM-CYD-C5 / ESP32-C5 | **0.4.4c** | 0.4.5c |

Current prepared increments add the Offline Essentials APIs; see
[OFFLINE-ESSENTIALS.md](OFFLINE-ESSENTIALS.md). New builds are not yet installed.

The first labels were S3 0.4.1s (user-confirmed working) and C5 0.4.0c (prepared).
The current increment prepares the first wired health/echo pair; see
[PAIRED-FIRST-LINK.md](PAIRED-FIRST-LINK.md). Standalone build defaults keep the
UART disabled; the paired artifact requires the explicit qualified link profile.

C5 .1c subsequently showed a user-reported automatic 45-second rollback despite
touch/dashboard responsiveness. C5 .2c fixes a reproduced heartbeat sampling race
and adds gate/error diagnostics; device acceptance remains pending. See
[C5-OTA-HEARTBEAT-0.4.2c.md](C5-OTA-HEARTBEAT-0.4.2c.md). S3 .2s is unchanged.

Actual .2c failed-boot capture later identifies missing API, with fresh heartbeats
and a connected UART peer. .3c prioritizes required web startup and moves link
context into PSRAM; see [C5-WEB-STARTUP-0.4.3c.md](C5-WEB-STARTUP-0.4.3c.md).

These are separate release counters. C5's 0.4.0c is the current paged code baseline,
not the old unsuffixed 0.4.0 implementation. The baseline includes the app-memory,
Lua ABI, file-picker, Settings and paged-navigation work. Shared Diagnostics
launcher cleanup is included so both builds use the same native navigation.

`versions/crowpanel_s3.txt` and `versions/nm_cyd_c5.txt` are the authoritative build
defaults. Update only the appropriate file when incrementing that board. Both the
root CMake build and scripts/build_ota.py read these defaults; the S3 recovery
builder does too. An explicit --version / PROJECT_VER override still allows
historical builds. The suffix is embedded in the actual ESP application version
and reported by Settings, the dashboard, boot log and peer-health version string;
it is not just a renamed download. A device shows the new label after its matching
OTA has been installed. Previously published artifacts remain unchanged.

The OS updater does not enforce numeric ordering of these display strings. It
validates chip, exact board profile, required preservation features, protocol,
partition layout and image integrity. Consequently 0.4.10 -> 0.4.1s/0.4.0c is an
allowed naming transition. The s/c suffix does not replace board/chip validation
and cannot make an S3 image valid on C5 or vice versa. App API version remains 1;
OTA protocol and settings/catalog schemas do not change. No rollback safeguards,
private configuration or eFuse settings are changed.

## Diagnostics belongs to Settings

The launcher now has only Settings, Apps and Logs. Diagnostics is accessed via
**Settings > Diagnostics** and no longer has a launcher shortcut. Back from its
first page returns to Settings; Back/Next on other pages still navigate the
current section. The counter describes pages within that section, not navigation
history. The prior Diagnostics launcher shortcut opened that same Settings view,
which explained the seemingly unrelated Settings parent on Back.

Three launcher icons are arranged in one row in landscape and a balanced two-row
layout in portrait. Fixed Back/Home/Next and the rest of the paged UI remain.
No duplicate Diagnostics app/package is created or removed from SD. Settings
Diagnostics retains its live system monitor; the optional native internal helper
is not exposed as a launcher action. The old unreachable static diagnostic view
has been removed from the launcher action handler.

## Validation and installation

This section records the original naming/UI split. The subsequent wired-pair
increment and its 190-test/build evidence are in [PAIRED-FIRST-LINK.md](PAIRED-FIRST-LINK.md).

187 host regressions pass. The actual LVGL shell test checks that the launcher
has no Diagnostics icon, opens Diagnostics through Settings and returns to its
Settings parent with Back. Existing page/navigation/rotation checks run on both
board-size fixtures. Tests cover the two default strings, unknown board rejection,
and real C OTA-policy acceptance of suffixes while still rejecting the wrong
board/chip. Board, scheduler, heap, NVS and I/O are doubles in these tests.

Both ESP-IDF 5.5.4 targets are built with their actual new version descriptors,
validated image/layout/hash and blank initial private fields. The board drivers,
S3 startup recovery, memory defaults, native input and rollback manager remain
unchanged from the prior paged release. Physical installation/acceptance of these
labels/UI adjustments remains pending. Prepared does not mean installed.

Use the matching board's Safari dashboard > OS update > Upload & verify OS >
Restart into update. No USB flash or SD card is required for OS OTA. Existing
credentials, calibration, folders, app revisions and app data are retained.
