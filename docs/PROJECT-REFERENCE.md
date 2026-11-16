# CYD OS project reference

Historical 0.3.x reference. The current 0.4.0 extension is described in
ARCHITECTURE-0.4.md, LUA-API-0.4.md, SECURITY-0.4.md and UPGRADE-0.4.md.
Those documents supersede the earlier capability/permission limits below.

Original snapshot: source and prepared firmware **0.3.2**. This document carries the project
context into new chats. Check the device's displayed version before assuming that
a prepared update has been installed. No Wi-Fi password, access token or private
device sdkconfig belongs in this repository.

## Goal and working style

CYD OS is an ESP-IDF application shell for a touchscreen ESP32. The goal is to
create, organize, upload and run small apps wirelessly, using an iPad and ChatGPT
when away from a desktop. Routine OS updates should also work wirelessly. USB is
retained for the initial OTA bootstrap and serious recovery.

The user prefers direct, completed changes and clear, short setup steps. Explain
which device/app a command belongs in. Normal shell commands run in the Debian
terminal, not inside the serial monitor. Preserve working Wi-Fi, AP settings,
access token, touch calibration, SD apps and app data through updates. Do not
require reflashing for routine app development or connection changes.

The iPad is Wi-Fi-only. A phone hotspot supplies internet for ChatGPT/downloads.
The C5 can create its own protected local Wi-Fi network, named **CYDAP** by the
user, for Safari uploads away from an existing LAN.

This is an embedded Lua app environment. It does not run arbitrary desktop apps,
Python apps, Arduino sketches or foreign firmware as `.cydapp` packages.

## Hardware

The user's primary, working device is the **RockBase NM-CYD-C5**. The implemented
profile follows the V1.0 ST7789 schematic/examples. A USB chip query reported:

| Item | Confirmed/configured value |
| --- | --- |
| Chip | ESP32-C5HR8, silicon revision v1.0 |
| CPU | Single main core, up to 240 MHz |
| Flash | 16 MiB external flash |
| PSRAM | 8 MiB embedded PSRAM; quad configuration |
| Crystal | 48 MHz |
| Working USB port | `/dev/ttyACM0`, native USB Serial/JTAG |
| Display profile | ST7789, 320×240, RGB565 over SPI |
| Touch profile | XPT2046 resistive touch |

The chip's silicon revision is not proof of the PCB's printed revision. The
silicon also supports dual-band Wi-Fi 6, BLE and IEEE 802.15.4; that does not mean
CYD OS implements Bluetooth/Thread/Zigbee services or has physically verified
every radio mode.

C5 wiring used by the HAL: shared SPI SCK/MISO/MOSI GPIO6/2/7; LCD CS23, DC24,
backlight25; touch CS1; SD CS10. BOOT is GPIO28, not GPIO0. SD must enter SPI mode
before LCD/touch traffic on the shared bus. Keep the working panel initialization,
RGB565 wire byte order, asynchronous flush completion and bus arbitration intact.

The secondary supported build is **Elecrow CrowPanel Advance 4.3-inch DIS01943A,
V1.1–V1.3**, using ESP32-S3, an 800×480 RGB panel and GT911 touch. This is compile
support, not confirmed hardware operation. The user did not have the board/revision
available to inspect. CrowPanel V1.0 uses different controls and is not covered.
C5 and S3 use separate HALs, build directories and memory configuration.

See `docs/HARDWARE-C5.md` and `docs/HARDWARE.md` for pinned manufacturer references.

## Host setup and source

The user's desktop runs Debian 13 (trixie), with Python 3.13.5 reported during
setup. The ESP-IDF environment is **5.5.4**, normally activated with:

```bash
source ~/esp/esp-idf/export.sh
```

The user's working source directory is `~/cyd-work/cyd-os` (previously shown as
`/home/jacob/cyd-work/cyd-os`). Their hardware configuration is
`build-device-c5/sdkconfig`. That private generated file can contain Wi-Fi and
token values. Preserve it locally; it is excluded from GitHub.

The assistant workspace used `/workspace/cyd-os`, with separate compile and OTA
build directories. Those paths are not paths on the user's Debian computer. A new
chat must obtain/clone the repository; earlier chat workspaces are not shared memory.

The native stack is ESP-IDF 5.5.4, FreeRTOS, LVGL 9.1.0, Lua 5.4.8, cJSON, FAT SD
storage and the ESP-IDF HTTP server. Component versions are pinned. Lua's verified
upstream source and license notice are included; ESP-IDF and managed dependencies
are installed/fetched separately.

## Development history and validation status

| Stage | Result |
| --- | --- |
| Initial C5 bring-up | User built/flashed firmware and reached the shell |
| Touch correction | Opposite-side taps were fixed; user confirmed touch worked |
| Wi-Fi, SD and Lua deployment | User confirmed successful authenticated uploads/runs |
| 0.2 connection manager | User confirmed Connections and CYDAP worked from the iPad |
| 0.3.0 feature set | OTA, Settings, Apps folders and watch-inspired theme implemented and software-tested |
| 0.3.1 image | Same feature set with a version bump, supplied for a first OTA exercise |
| 0.3.2 patch | Persistent browser access and automatic dashboard login added |

Latest source/image is 0.3.2. The conversation does **not** contain a complete
physical acceptance report for every 0.3.x feature or confirmation that 0.3.2 is
currently installed. Treat “implemented,” “software-tested,” and “user-confirmed on
hardware” as separate facts.

Software evidence includes successful C5 and S3 builds, **98 tests** spanning
Python/native/browser checks, a validated credential-free C5 OTA image, and actual
LVGL software renders at both display sizes in all four rotations. Browser checks
used Chromium and a simulated API. Native service checks use SDK/radio/NVS/RTOS
doubles. They do not establish physical Safari, backlight, touch, RF, flash power
loss or hardware rollback behavior. CrowPanel hardware remains untested.

## Home screen and device Settings

The old Projects button is now **Apps**. Home has four round buttons: Settings,
Apps, Diagnostics and Logs. Its Apple Watch-inspired treatment is restrained:
black background, colored circles and font symbols, without a heavy animated
launcher. A clock/status area and persistent **Home / Stop app** control sit
outside the Lua app container.

Settings contains Connections, Display & touch, Clock, and Device access & OS
updates. Menus scroll vertically; some controls are below the first screen.

Brightness is adjustable from **10–100%** and persisted. C5 uses LEDC PWM on its
backlight pin; CrowPanel uses its STC peripheral. Orientation supports 0/90/180/270
degrees, with touch coordinates transformed consistently. Rendering uses a second
partial draw buffer rather than a full animated framebuffer. Rotation stops the
current Lua app. Keep a preview within **15 seconds**, or saved settings return.

Touch calibration collects five raw-coordinate targets, fits an affine mapping,
then offers a 15-second Keep/Revert preview. Collection times out after **60
seconds**. Holding anywhere for **2 seconds** cancels collection without depending
on a good existing touch mapping. A failed fit restores saved settings. Display
reset restores board defaults while retaining Wi-Fi, token, clock offset, apps
and app data. The bootstrap saves its working compiled calibration to NVS before
a generic OTA image can replace those compiled defaults.

Clock can be set on-device using `YYYY-MM-DD HH:MM`, with a UTC offset, or synced
from Safari using **Sync clock from this iPad**. SNTP uses `pool.ntp.org` when
internet is available. There is no battery-backed RTC: after power loss offline,
sync/set the time again. A fixed UTC offset persists; daylight-saving changes need
another sync. Lua uptime remains monotonic and separate from wall-clock time.

## Connection manager, CYDAP and the iPad workflow

Settings → Connections scans/joins networks, accepts hidden SSIDs, stores up to
**five profiles**, reconnects, forgets profiles and controls the protected CYD AP.
A new password is saved only after a matching successful DHCP event. Failed joins
do not intentionally replace the old working password. Saved settings live in
internal-flash NVS, not on the SD card. The old build-time credentials import once;
later touchscreen choices are authoritative.

Station means the CYD joins another AP. AP means the CYD creates a network. Both
can be enabled together, subject to the hardware/SDK's channel constraints.
Changing radio mode or scanning can temporarily disconnect AP clients.

The user named the local AP **CYDAP**; the factory code default is CYD-OS. Its
normal address is `http://192.168.4.1`, but the displayed address is authoritative.
The AP Wi-Fi password and dashboard token are different credentials. An initial
unconfigured AP password is generated and can be viewed on the device.

CYDAP creates a local LAN. **It does not provide NAT or forward internet from the
phone hotspot to the iPad.** AP+station operation does not make the C5 a travel
router. For an away-from-home workflow:

1. Join the phone hotspot on the iPad for ChatGPT and file downloads.
2. Save the app or OS update in the iPad Files app.
3. Join CYDAP, open the CYD address in Safari and upload the file.
4. Return to the phone hotspot when internet is needed again.

Alternatively, put the CYD and iPad on the same hotspot/LAN and use the CYD's
station IP if that network permits client-to-client traffic. ChatGPT's iOS app
does not directly send HTTP requests to the private CYD address; Safari performs
the transfer. The dashboard needs no CDN or internet connection once connected.

Enterprise/EAP, WEP, captive-portal sign-in, WPS and internet routing are not
implemented. Wi-Fi configuration is a physical touchscreen operation; the browser
can report connection status but has no Wi-Fi credential editor.

## Dashboard access and remembered login

All `/api/*` routes require bearer-token authentication, including status, app
uploads/downloads, logs, settings and OS updates. Public dashboard HTML contains
no embedded token. The token imports once into NVS or is generated, and remains
stable across ordinary OS updates. View or replace it on the physical device in
Settings → Device access & OS updates. Replacement requires confirmation and makes
old client credentials invalid.

The user's issue was repeatedly typing a long token on the iPad. In **0.3.2**,
“Remember this device” is checked by default. After successful authentication,
Safari stores the token in localStorage and automatically signs in on later visits.
An already authenticated old tab can migrate its session token without retyping.
Uncheck the option for tab-only access. Settings → Forget saved access clears
remembered access. An invalid token clears saved access and returns to login;
connection/service failures retain it. Blocked browser storage does not prevent
current-page sign-in, but the page explains it cannot remember access.

Remembering is per browser and exact origin. Use Safari consistently, ideally with
a bookmark to `http://192.168.4.1`. A station IP, `cyd.local`, another port, another
browser or a home-screen web app may use different storage. Clearing website data,
closing private browsing or replacing the token can require signing in again.
HTTP has no TLS here; use the protected local network. No cookie login, public API,
CORS allowance or token-in-URL scheme was added.

## Apps, folders, revisions and private data

Apps is an organized catalog rather than a raw file list. A **package ID** identifies
an app, a manifest **version** is its human-readable iteration label, and a
**revision ID** identifies one installed upload. Keep the package ID stable across
iterations so they stay grouped. Each successful upload currently gets a new
random 16-character hexadecimal revision ID, including repeated identical uploads;
there is no content-based deduplication or guaranteed chronological sort.

Create/rename folders, move an app group, open a group and select a version to run.
Deleting a folder returns its groups to the root; it does not delete versions or
private app data. Safari can also download the original package or delete an
individual inactive version. A running/starting revision cannot be deleted.

Folders are a **logical catalog**, not arbitrary physical SD directories. Existing
revision files remain at `/sdcard/cydos/projects/<revision>/`, including their
manifest, extracted source and retained `package.cydapp`. Private app data lives
separately at `/sdcard/cydos/data/<package_id>/` and survives revision deletion.

Folder metadata uses alternating generation/CRC journals under
`/sdcard/cydos/system/project-folders-a.json` and `project-folders-b.json`. It loads
the latest valid copy and can fall back to the preceding copy. Limits are 16
custom folders and 64 custom app placements. The touchscreen shows up to 64
groups/versions per view; Safari exposes the full list. Nested custom folders,
a general SD file browser, automatic retention and app-data quotas are not implemented.

## Lua applications and the `.cydapp` format

A `.cydapp` is a strictly validated **uncompressed ZIP (ZIP_STORED)** containing
`manifest.json` and textual Lua source at its root. Use the supplied packer; a
normal compressed ZIP renamed to `.cydapp` is rejected. Current limits are 512 KiB
per package, 48 files, 64 KiB per file, paths under 96 bytes, four directory levels,
and a 4096-byte manifest. Paths are restricted lowercase relative paths. Links,
parent traversal, duplicate entries, encryption and ZIP64 are rejected.

A compatible manifest example is:

```json
{
  "package_id": "com.local.hello",
  "name": "Hello CYD",
  "version": "0.1.0",
  "api_version": 1,
  "entry_point": "main.lua",
  "target": ["esp32c5", "esp32s3"],
  "permissions": ["storage"]
}
```

The source folder means a folder containing this manifest and the Lua entry point;
it is not necessarily the entire `cyd-os` firmware source tree. `apps/hello` is the
included example. It displays a label/button and persists a tap count.

Available Lua operations include screen/container creation, usable screen size,
labels, fixed-size buttons and click callbacks, label text updates, logging,
monotonic uptime and private basename-based storage read/write. Storage permission
is optional but required for storage calls. Each private data file is capped at
16 KiB; repeated files can still fill the card. See `docs/API.md` for exact signatures.

Only one foreground app runs. Entry code creates UI, registers callbacks and
returns; it must not sit in a permanent loop. Callbacks run serially on the Lua
task. Starting a version replaces the previous runtime; apps do not auto-run at
boot. Home requests stop. Errors release runtime widgets and allocations.

The allocator is capped at 1 MiB and user widgets at 96. An instruction hook checks
cancellation/execution budget every 1000 instructions; entry/callback budgets are
500 ms. Click queues are bounded. Arbitrary blocking C/SD calls cannot be forcibly
interrupted, so this is not a secure process sandbox or a hard real-time promise.
Desktop Lua `io`, `os`, package loading and debugging libraries are not exposed.

The released Lua API does not expose GPIO, UART, I²C, general network access,
Bluetooth, audio, charts, images, text inputs or sliders. Device Settings using a
native LVGL slider does not imply a Lua slider API exists. Foreign GitHub projects
must be assessed/ported to these actual APIs; they are not automatically installable.

## Native OS OTA

`.cydapp` changes a Lua app on SD. An OS `.bin` changes native firmware in internal
flash and requires a restart. They use separate upload endpoints and validators.
An arbitrary merged image, foreign bootloader or unrelated firmware is not an OS
update supported by this shell.

The 16 MiB layout includes NVS, OTA metadata, PHY data, and two **7 MiB app slots**:
`ota_0` at `0x20000`, `ota_1` at `0x720000`. OTA writes only the inactive app slot;
it does not update the partition table, bootloader or eFuses.

A pre-OTA 0.2 installation needs one USB bootstrap to install the updater and a
rollback-capable bootloader. Subsequent compatible updates use Safari:

1. Download the exact C5 application `.bin` into Files while online.
2. Connect to CYDAP/shared LAN and open the dashboard.
3. Open OS update, choose the file and Upload & verify.
4. Keep device power connected; after verification, choose Restart into update.
5. Rejoin CYDAP if needed, then check running version and startup confirmation.

The uploader validates chip, 16 MiB image header, CYD project, exact board profile,
protocol/settings compatibility, required rollback features, partition-layout hash
and complete image integrity. The image is staged before boot selection changes.
Cancel retains the running selection. A transfer interrupted before restart leaves
the running OS selected. Beginning an upload overwrites any older fallback in the
inactive slot. The reported ESP image hash differs from a whole-file hash; supplied
JSON metadata records both. Integrity/compatibility checks are not publisher signing.

Pending firmware must initialize core/NVS/access/device settings, board, API, UI
and network service. UI/network heartbeats must remain fresh for 10 continuous
seconds, within a 45-second startup deadline. Internet and an SD card are not
required to confirm startup. A failed pending startup attempts ESP-IDF rollback;
a reset before confirmation is also covered by bootloader rollback. A hard hang,
power fault or damaged bootloader can still require USB recovery. Software health
cannot prove that the physical display, touch mapping or radio works correctly.

Manual restoration is available when there is a compatible confirmed previous
image. A fresh USB bootstrap does not necessarily have one. The maintenance gate
stops Lua and blocks app/folder/display/network reconfiguration during a staged OS
update until cancellation or restart. NVS credentials/display settings and SD apps
are retained through normal compatible updates.

The ESP32 does not compile its native OS. A computer/build service still needs
ESP-IDF to produce future `.bin` files, even though installation is wireless:

```bash
source ~/esp/esp-idf/export.sh
cd ~/cyd-work/cyd-os
python scripts/build_ota.py --board nm_cyd_c5 --version 0.3.3
```

Here 0.3.3 is a future example, not a release already implemented. The helper uses
a separate build directory, blanks initial credentials, validates descriptor/hash
and writes the `.bin` plus metadata to `dist/`. A version bump alone adds no features.

## USB setup, CLI and recovery

The current bootstrap download contains the guarded installer. Extract into Linux
Downloads, close any serial monitor with Ctrl+], then run in a normal terminal:

```bash
python3 -m zipfile -e ~/Downloads/cyd-os-update.zip ~/Downloads
bash ~/Downloads/cyd-os-update/install_c5_update.sh
```

It defaults to `~/cyd-work/cyd-os`, `build-device-c5/sdkconfig` and `/dev/ttyACM0`.
It verifies payload hashes, backs up source/private config, enables rollback/fonts,
builds and validates the version/descriptor before flashing. It preserves local
app sources and private configuration. `.os-update-backup-*` directories stay
private and out of GitHub. Do not run erase-flash for routine updates.

After “Hash of data verified,” let the board reset or press RESET with BOOT released.
“Waiting for download” means ROM download mode, not a long-running app installation.
If connection fails, use the board's BOOT+RESET sequence; release BOOT for normal
startup. Most flash commands can reset automatically, so holding BOOT throughout
normal operation is unnecessary. A USB data cable and correct serial port matter.

The optional `cydctl` tool packages/deploys from Linux and supports discovery,
status, revision listing/run/stop/delete/download and logs. Example packaging:

```bash
python -m pip install -e tools/cydctl
cydctl pack apps/hello -o hello.cydapp
```

Use a private token file/environment rather than publishing a token in commands.
An earlier HTTP 400 was resolved using the correct authentication and bypassing
CLI environment proxies with `NO_PROXY='*' no_proxy='*'` when needed. HTTP 401
means authentication is required/invalid. SD-dependent app uploads need the card;
OS OTA does not. An installed upload is not automatically a running app, and a
202 run response only schedules startup; poll status for success.

Keep the native source, SD contents and private device configuration backed up.
The repository contains project assets, not a live SD-card backup or factory
firmware dump. USB recovery remains the route for serious firmware/bootloader
failures. See `docs/RECOVERY.md` and `docs/OTA.md` for detailed steps.

## Repository map and continuing in another chat

| Path | Responsibility |
| --- | --- |
| `main/` | Startup and subsystem health integration |
| `boards/`, `components/board/` | Separate C5/S3 hardware abstraction and selection |
| `components/app_manager/` | Package installation, revisions and folder catalog |
| `components/app_runtime/` | Bounded Lua runtime and exposed APIs |
| `components/network/` | Wi-Fi/AP profiles and connection policy |
| `components/device_access/` | Persistent API token and validation |
| `components/device_settings/` | Display calibration/orientation/brightness and clock |
| `components/ota_manager/` | Image policy, staging, startup checks and rollback |
| `components/ui/` | Native shell, Settings, Connections and Apps views |
| `components/web_api/` | Authenticated API and embedded Safari dashboard |
| `components/storage/`, `components/cyd_common/` | SD operations and shared parsing/helpers |
| `apps/hello/`, `tools/cydctl/` | Example application and desktop packer/client |
| `scripts/`, `tests/`, `docs/` | Build/install/release helpers, regression checks and references |

The important API families are `/api/status`, `/api/packages`, `/api/versions`,
`/api/run`, `/api/stop`, `/api/download`, `/api/logs`, `/api/projects`, `/api/device`
and `/api/os`. The UI says Apps; legacy internal paths/endpoints may still say
projects. Do not rename them casually and break compatibility.

Read `PROJECT_CONTEXT.md`, this document, `README.md`, `docs/API.md`, `docs/OTA.md`
and the validation record before changing the project. Treat code/current tests
as authoritative when an older milestone document conflicts. Keep C5 and S3
profiles separate, preserve the working C5 touch correction, and do not silently
invent unsupported Lua capabilities. Test software as appropriate and explicitly
record new physical observations rather than upgrading assumptions to facts.

The next practical milestone is a full on-device acceptance pass of 0.3.2: confirm
installed version, remembered Safari access, retained profiles/token/calibration,
brightness, rotation Keep/Revert, calibration cancellation/timeouts, clock sync,
folder persistence, OTA confirmation and controlled rollback. Future expansion can
then add richer Lua widgets, resource-managed hardware APIs, nested organization,
retention/backups and other board variants as deliberate new work.
