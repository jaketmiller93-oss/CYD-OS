# CYD-OS 0.5 — one tablet, two board roles

0.5 is a new paired firmware line, not a renumbering of the independent 0.4 builds.
Both images carry the same system version. Firmware remains in each MCU's flash;
**System (S3 SD)** is the default app/data drive and **Device (C5 SD)** is the
management/report drive. Neither chip shares the other's physical SD bus.

## Screens and ownership

| S3 application host | C5 device manager |
| --- | --- |
| Calculator, Calendar, Clock, Notes, Contacts, Apps | Settings, Diagnostics, Connections, Files, Logs, Bluetooth LE |
| Runs foreground Lua and renders the main display | Owns Wi-Fi station/AP, saved networks and browser access |
| Owns installed packages, app data and default Documents | Brokers DNS/TCP/TLS; HTTP/MQTT remain bounded S3 app APIs |
| Coordinates a whole-system update staged on System SD | Stages its own inactive flash slot on coordinator requests |
| Follows C5 time, offset and 12/24-hour preference | Owns global clock settings; stores an S3 display profile |

The native essentials are embedded in firmware. They do not need separate app
uploads or a first-boot extraction/install. Calculator has no storage dependency;
SD is required for saved records, shared files and installed apps. The existing
bounded Lua/LVGL runtime, package permissions and one-foreground-app limit remain.
Clock alarms/timers still run only while Clock is open, with visual alerts.

C5 Settings includes **S3 display settings** for brightness, orientation preview,
idle dimming and starting touch calibration. Confirm an orientation within 15s.
The C5 stores the confirmed S3 public display profile in its own NVS namespace.
Each board retains its hardware calibration and a local recovery copy. Existing
calibration is not replaced by another board's data. Global Wi-Fi/AP settings,
dashboard token, new app credentials and Bluetooth bonds stay in **C5 NVS**, not
plaintext files on an SD card. Existing S3 private settings/credentials are retained;
there is no automatic deletion or bulk migration of old private data.

The C5 writes bounded current log/diagnostic JSON snapshots for both boards under
its `reports` folder about every 30s. These may contain SSIDs, addresses and app
errors; they are local device data and are never included in release packages.
Logs also shows a fresh bounded mix of recent C5 and S3 lines. Diagnostics uses
cached link health; radio/UART/SD work does not run on the LVGL thread.

## Two logical drives

C5 Files selects System or Device, then Documents, Reports, Projects, Data or
Backups. Projects/Data/Backups are read-only through this system file browser;
the package/runtime services own those directories. Documents and Reports allow
new short text files, editing, rename and confirmed delete. Names use the existing
lowercase CYD path rules. This is the CYD-OS area of each card, not an unrestricted
browser of arbitrary external SD folders.

Directory listings cap at 64 entries and report overflow explicitly. Readers use
2 KiB chunks, paged controls and UTF-8 boundaries; binary previews show 256 bytes.
Text editing caps at 240 bytes. Copy to the other drive's Documents is streamed,
SHA-256 checked and committed without overwriting an existing destination, up to
2 MiB. Home cancels a copy at a bounded service boundary. A timeout at commit is
an uncertain result: inspect the destination before retrying. Edit/copy staging
is recoverable or ignored, never executable. This does not guarantee immunity to
SD corruption or every possible power-loss failure.

Ordinary Lua `storage` stays package-private on S3; `documents` refers to the
S3 Documents area. General two-drive operations are privileged system file-manager
services, not a new raw filesystem permission silently granted to every app.
Old C5 packages/data remain on its card; they are not erased, moved or auto-imported.

## Connections

S3 does not start its Wi-Fi or Bluetooth radio. C5 retains the existing Wi-Fi
manager and authenticated Safari dashboard at its own address. App installation,
execution and downloads there route to S3. C5 global settings remain available
when S3 is disconnected. The main status API reports S3 app/storage resources,
with separate host/manager details and a combined readiness flag; it does not
report the C5 idle runtime as the tablet's foreground app. There is still no AP internet/NAT forwarding to the iPad.

The S3's existing HTTP/TCP/MQTT engines use a nonblocking proxy. The C5 performs
DNS and verified TCP/TLS operations with the same certificate bundle/custom CA and
server-name checks. Stable connection slots, partial writes, session ownership,
finite queues and cancellation prevent stale handles from crossing board resets.
There is no unbounded app worker on C5 and no automatic retry of uncertain writes.

Bluetooth LE is enabled on demand from the C5 screen, preserving boot resources.
The initial manager supports discovery, an explicitly selected central connection,
disconnect and stored bonds. After the six-second scan, press Refresh to show the
results; a newly enabled controller may ask for Scan again once it is ready. It is not a generic audio, keyboard, serial-profile
or GATT sensor application. A device needing passkey interaction is rejected
rather than silently paired; peripheral-specific services need their own drivers.
Radio coexistence and actual peripherals still require physical acceptance.

The existing three-wire UART remains TX8/RX2 on S3, TX5/RX4 on C5, crossed plus GND;
115200 baud, no power wire and no selector changes. Framing adds session-bound
service calls to the existing health protocol. Request assembly is 8 KiB, retained
response 32 KiB, with finite transfers and no mutation replay after a lost reply.
Ordinary exchanges time out after 5s; install/flash/final file commit allows 30s,
so a legitimate erase or SD commit is not mistaken for a lost peer.
The cable is treated as the tablet's trusted internal interconnect. CRC and boot
nonces do not provide cryptographic authentication/encryption of the physical wire.
Bearer authentication stays at C5's dashboard; its token is not a pairing key.

## One update package

A `.cydos` container holds exactly one S3 and one C5 application image for one
system version. It is not an ESP merged binary and must not be written by a USB
flasher. Its 512-byte header has a CRC, fixed role/board entries, exact offsets,
sizes and per-image SHA-256 values. Both images must advertise paired service
schema 1 and their correct role in the existing CYD OTA descriptor. Partition,
settings, access and catalog schemas remain unchanged.

After both boards have 0.5, upload **one** bundle through C5's Safari dashboard:
C5 streams it to S3 SD; S3 verifies both complete image hashes and board descriptors;
on explicit Restart it stops apps, stages C5, stages S3, restarts C5, then waits
for a fresh C5 boot with the exact candidate ELF hash and startup confirmation.
Only then does it restart S3. The dashboard waits for the expected S3 version and exact ELF build hash;
it must not call the old confirmed boot a successful update. Transfers at the
existing UART speed can take many minutes; progress shows the current stage.

No cross-chip flash operation is atomic. A lost acknowledgement is not proof that
a mutation failed. A failed second stage can leave a compatible mixed release;
report that condition and retain the S3's selected working image. Each chip keeps
its own native A/B startup confirmation/rollback. Link availability is not a boot
confirmation requirement, so unplugging one board does not roll back a healthy
local interface. System services must initialize before the API health bit is set.

C5 required HTTP resources still start before UART/services. JSON and transport
contexts use PSRAM; task stacks/radio/DMA retain the appropriate internal memory.
Bluetooth is not initialized at boot. No credential namespace, calibration, SD
content, partition table, bootloader or eFuse is reset by normal application OTA.

C5 authenticated `/api/recovery/os`, `/api/recovery/os/update`,
`/api/recovery/os/reboot` and `/api/recovery/os/cancel` retain its
single-board OTA path for deliberate recovery. They are separate from the normal
System update UI. S3 retains USB and bootloader rollback recovery. Firmware bundles
are integrity/compatibility checked; they are not publisher-signed packages.

## One-time transition from 0.4

The installed 0.4 link cannot forward updates. Bootstrap each board once using
its own existing dashboard and its role-specific application `.bin`.

1. Record both current addresses. Update **C5 first** and keep S3 on its working
   firmware. Check the physical C5 is on 0.5.0, touch works and it stays running
   beyond one minute. S3 apps/system-update services will show unavailable during
   this temporary mixed state; C5 local settings still work.
2. Use the S3's **own old dashboard address** to install its S3 bootstrap `.bin`.
   After restart, that old S3 Wi-Fi address stops responding because C5 now owns
   radios. Check the S3 home and use C5's address for subsequent access.
3. Verify both versions, touch, native apps, both cards, Wi-Fi, shared Files,
   saved settings, Bluetooth discovery and recovery before treating this as the
   everyday device. The supplied artifacts are software-tested candidates, not
   evidence that either device has been flashed or physically accepted.

Do not upload a `.cydos` container to a 0.4 updater. Do not use the S3 image on C5
or the C5 image on S3. The hardware profiles and flash addresses remain distinct.

## Development and validation

Activate ESP-IDF 5.5.4 and build with `CONFIG_CYD_PAIRED_SYSTEM=y` plus the qualified
link configuration. Dedicated build folders blank initial private fields. Public
source includes the actual offline apps, embed generator, portable framing,
service assembly, storage broker, network proxy and update coordinator tests.

`python scripts/package_paired.py --s3 <S3 app.bin> --c5 <C5 app.bin> --version 0.5.0 --output <folder>`
validates both real images and emits the single container and metadata. Host tests
use actual C/Lua/LVGL and explicit SDK/radio/flash doubles. They do not establish
physical Safari/RF/SD power-loss behavior, C5 internal heap under real coexistence,
wire throughput or a real coordinated two-board reboot. See the release validation
record for the final test/build counts and remaining acceptance steps.
