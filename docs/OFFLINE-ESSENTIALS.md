# Offline Essentials 1.0 — both boards

The first suite keeps the existing Calculator and adds five installable Lua apps.
These are CYD-OS apps using native LVGL controls, not web pages or separate ESP
firmware. One identical `.cydapp` works on C5 and S3 with the matching platform API.
They request no network, MQTT or credentials permission. No extra service task is
created for offline timers. The existing Settings and Safari app installer remain.

## Apps and bounds

| App | Working features | Initial bounds |
| --- | --- | --- |
| Calculator | Four operations, repeated equals, sign, decimal, clear/backspace; zero-divide and overflow handling | Existing com.cydos.calculator 1.0.0, no permissions; no need to reinstall if already present |
| Calendar | Gregorian month view, paged weeks, day-filtered/all event lists, add/edit/delete and JSON export | 24 events; dates 2024–2099, title48/note160 bytes; optional HH:MM; no automatic event notifications |
| Clock | OS date/time and 12/24h format, visual daily alarm, countdown with pause/reset and stopwatch | Countdown1–86400s; second-resolution display; saved alarm time; alarms/timers operate only while Clock is open |
| Notes | Add/edit/delete, saved entries across restart/revisions, paged reading and JSON export | 16 short notes; title48/body240 UTF-8 bytes |
| Contacts | Name, phone, email, note; add/edit/delete and JSON export | 16 contacts; name48/phone40/email80/note160 bytes; no calls/SMS |
| Files | Shared Documents list, UTF-8 reader, binary hex preview, create/edit/rename/delete with confirmation | Flat64-file area; names48 lowercase bytes; read16KiB, edit240 bytes; text preview first512 wrapped lines; binary preview first256 bytes |

Notes, Contacts and Calendar keep their records in their own private app data
directory using two alternating validated snapshots. Corrupt/incomplete newer
snapshots can fall back to the previous valid slot. Saved JSON is bounded to7800
bytes by the existing decoder limits; many escaped characters can reach that bound
before the entry-count limit. Save is explicit. Exports create new shared JSON
documents; an existing export is not silently overwritten. This first suite does
not implement import, cloud backup or cross-board synchronization.

Clock's countdown/stopwatch use elapsed monotonic time, not tick counts, so missed
callbacks do not slow them down. Leaving the app cancels runtime timers; there is
no background alert, sound, wake-from-sleep or alarm while powered off. OS clock
validity and UTC offset remain owned by Settings. After losing wall time, set it
in Settings or use Safari's Sync clock; this can work from the iPad without an
internet time server. Calendar indicates an unset clock and starts at Jan2024.

Files manages **Documents**, not the SD root, OS files or other apps' private data.
Its public name in the launcher is Files; its view title is Documents. Exported
notes/contacts/events become visible there. The SD card remains necessary for
package installation/launch and saved data. Data is local to the board running the
app: the UART link currently exchanges health/echo, not files or app services.

The original longer app list is not represented by dummy icons. Phone/SMS needs
a telephony service/module; Camera/Gallery need capture/decoder APIs; a full browser,
email and media need platform services/codecs/accounts; cloud sync needs an actual
destination and authentication. Those are later increments, not included here.

## Install and use

1. On each board that will run the new apps, use its own numeric IP/Safari
   dashboard to OTA its matching platform: **S3 0.4.3s** or **C5 0.4.4c**.
   Wait for startup confirmation and check the version. No USB flash is required.
   Start on the S3 if it is your main screen; C5 0.4.3c can remain its companion
   until you want the new apps on C5 too. The link wire format is unchanged.
2. Extract `Offline-Essentials-Apps.zip` in Files, then upload each `.cydapp` using
   Safari **Apps > Upload app**, not OS update. Apps are installed separately;
   the outer bundle is not one app. Names/IDs group revisions normally.
3. Open the app from CYD-OS Apps. Use its own Back/Previous/Next controls for
   lists/forms; OS Home exits. Calendar Weeks cycles visible weeks; month buttons
   change month. The Calendar Events button shows all saved events.
4. In Notes/Contacts/Calendar use New, fill each field and Next, then Save on the
   last field. Back discards the draft. Open an entry and choose Actions for
   edit/export/delete. Files allows small text editing and read-only larger files.

The new `documents` permission is shown by package inspection and explained in
the installer. Older OS versions reject it; update the OS before these packages.
Clock checks for the new runtime API and shows an update hint if absent. Existing
Calculator remains compatible with the older platform and keeps its package ID.
The new firmware does not install these packages automatically or change the
user's Wi-Fi, calibration, folders, SD data or credentials.

## Additive API 1 changes

`cyd.time.now()` returns a table with `valid`, `offset_minutes`, `clock_12h`.
When valid it also returns UTC `epoch`, local `year/month/day/hour/minute/second`
and Sunday-zero `weekday`. It is read-only; it exposes no settings mutation.

With manifest permission **documents**:

```lua
local files,err=cyd.documents.list() -- sorted { {name=...,size=...}, ... }
local bytes,err=cyd.documents.read("note.txt")
local ok,err=cyd.documents.write("note.txt","hello") -- refuses overwrite
local ok,err=cyd.documents.write("note.txt","edited",true)
local ok,err=cyd.documents.rename("note.txt","renamed.txt") -- no overwrite
local ok,err=cyd.documents.delete("renamed.txt")
```

Operations return nil/error for storage/path/quota failures; missing permission
raises the normal runtime error. Names cannot contain separators, traversal, NUL,
uppercase or leading/trailing dots. Read/write cap16KiB. Write stages a complete
file before replacement; hidden old/new files allow recovery of known interrupted
rename stages on FAT, which cannot overwrite through rename. Symlinks are rejected
in host tests; FAT has no symlinks. This is not a guarantee against SD corruption
or every possible power-loss failure. App storage remains separately private.

`cyd.ui.text_lines(text,width,font)` wraps valid UTF-8 using native Montserrat metrics
at14(default),20 or28 pixels
and returns lines plus a truncation boolean. Input cap16KiB, width16–800, result
cap512 lines. Apps cache pages and bound each displayed label to1000 bytes; large
documents do not silently lose text at the 1024-byte widget limit. A native test
reads a13KB document through every page and checks every byte is represented.

Widget creation also accepts `padding`0–24, `border`0–4 and `radius`0–32; omitted
styles preserve old defaults. Explicit label height is honored. Native app views
use zero padding/border, no scrolling/animation, and bounded widgets. Existing
API1 limits (96 widgets,1MiB Lua memory,16 timers) remain; no app threads are added.
CrowPanel layouts scale control spacing1.5x and use20/28px text for comparable
physical readability/touch targets. C5 keeps the compact14/20/28px layout.

## Build and validation

```sh
python scripts/build_essentials.py --output dist/essentials
python scripts/build_ota.py --board crowpanel_s3 --link-profile crowpanel-v13-c5-p5
python scripts/build_ota.py --board nm_cyd_c5 --link-profile crowpanel-v13-c5-p5
```

The two firmware builds here reuse credential-free board build directories with
explicit PROJECT_VER overrides. Final target/chip/version/layout/ESP hash, exact
UART profile and blank initial private fields are validated. Prior working C5
web-before-peer startup, PSRAM link context, heartbeat fix and rollback remain.

Host regression suite:199 tests. The suite test packages all six apps and executes
real Lua, native bindings and LVGL under ASan/UBSan at240x320,320x240,800x480,480x800.
It exercises keyboard submission, persistent CRUD/relaunch, JSON exports, leap
dates, timer/stopwatch behavior, file operations, text pagination/fidelity and
memory cleanup. API cases cover permissions, traversal, symlinks, explicit
overwrite, quota, native/Lua allocation failure and interrupted-file recovery.
Synthetic fixture data appears only in previews/tests, not first-launch apps.
Package inspection checks the new permission in both C and cydctl.

These are software/build checks, not physical installation or Safari/touch/SD
acceptance of this new suite. No apps or firmware were installed during this task.
