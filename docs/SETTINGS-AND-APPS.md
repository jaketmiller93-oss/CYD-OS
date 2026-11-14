# Settings, Apps and the home screen (0.3.0)

The dark home screen has four round buttons: Settings, Apps, Diagnostics and Logs.
It uses LVGL shapes and font symbols, no image atlas or animated launcher. Home /
Stop stays available outside apps and menus. Scroll inside menus to see more controls.

**Settings → Connections** contains the working 0.2 connection manager. It still
supports five saved networks and protected AP/AP+station modes. **Device access &
OS updates** shows startup/update state and the dashboard token. Token replacement
requires an explicit confirmation on the device and signs out existing clients.

**Display & touch** provides 10–100% brightness and four orientations. C5 brightness
uses LEDC PWM; S3 uses its STC peripheral. Orientation is rendered in software with
an extra partial DMA buffer; no full-frame animation is used. Apps are stopped before
rotation so their layout cannot remain stuck at old dimensions. Touch coordinates
rotate with the display. Keep within 15 seconds to save an orientation; otherwise
it reverts to the last saved display configuration.

Calibration is a five-target affine fit to raw touch coordinates. Tap/release each
target, then test and Keep within 15 seconds. During collection, hold anywhere for
2 seconds to cancel without relying on the old mapping; collection also times out
after 60 seconds. A failed fit restores the saved settings. Reset display restores
board defaults and keeps Wi-Fi, token, clock offset, apps and data. Calibration,
orientation, brightness and UTC offset live in NVS across ordinary OS updates.

**Clock** accepts local `YYYY-MM-DD HH:MM` and a UTC offset in 30-minute steps.
Safari's Settings → Sync clock copies the iPad epoch and exact timezone offset.
SNTP uses `pool.ntp.org` when internet is available. There is no battery-backed RTC:
after power loss offline, set/sync the time again. A fixed UTC offset is saved;
daylight saving changes need another sync. Lua `uptime_ms` stays monotonic and is
not changed by clock settings.

**Apps** opens custom folders and app groups, rather than a flat revision list.
Open an app group to choose an iteration to run. The package ID is the group key;
keep it the same when uploading new iterations. Change the manifest version to
label an iteration. Each successful upload gets a fresh random revision ID, even if its contents
are identical; content-based deduplication is not implemented.

Create/rename folders, move an entire app group, or delete an empty/filled folder
on the device or in Safari. Deleting a folder returns its app groups to Apps; it
never deletes packages, versions or private app data. Deleting an individual version
is available in Safari. Root “Apps outside folders” holds unfiled groups. No SD
reformatting or re-upload is needed for existing packages.

Folders are a persisted **logical catalog**, not arbitrary filesystem directories.
Revision files stay at their stable deployment paths; private app data is unchanged.
The SD stores alternate generation/checksum journals under `cydos/system/` and loads
the newest valid copy, falling back to the previous copy if the latest is corrupt.
There are 16 custom folders and 64 custom app placements. The device displays up to
64 groups/versions per view; Safari exposes the full list. Nested custom folders,
arbitrary file browsing, pinning and automatic version retention are outside this release.
