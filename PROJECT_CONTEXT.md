# Installed result: S3 0.5.1 SD recovery confirmed

User explicitly approved installing the S3 SD recovery image over USB. Completed
2026-11-15 local / 2026-11-16 UTC: ESP32-S3,16MiB flash, no secure boot/encryption,
installed partition table exact-match, selected ota_1 VALID0.5.0 ELF463a69017.
Private backups retained. Wrote only inactive ota_0 at0x20000 with0.5.1 image
SHA256516780362b8ea1972312f968c82457f642fff107b236827a84ec80967b0d53d0,
verified complete readback, then other OTA sector0xf000 asNEW. Confirmed original
ota_1 app, NVS, bootloader, partition table and remaining protected bytes unchanged.
C5 untouched. No card formatting or settings reset. Normal firmware creates its
usual storage directories after mounting; no user files deliberately altered.

Reset/captured120s: 0.5.1 ELF d17f73b64; CMD59 first rejection0x106, retry accepted
after10ms withCRC enabled; storage initialization succeeded (continued past its
error check with no SD unavailable warning). At12339ms startup checks passed and
firmware confirmed. One boot banner, no panic/reset/reversion in120s. Serial closed.
The raw backups/logs remain private under work/private/s3-usb-recovery; publish
only sanitized evidence. Remaining physical work: user checks System(S3) drive,
native apps, later C5 update/paired OTA. C5 connected cold-start cause unresolved.
Use refreshed paired candidate from CYD-OS-0.5.1-S3-SD-Recovery outputs, not the
older pre-SD-fix0.5.1 bundle. Canonical paired upload also refreshed/old archived.

## Diagnosis and preparation record before authorized installation

# Captured S3 SD CMD59 failure; recovery candidate prepared

User confirmed S1=1/S0=1, FAT32 and card inserted before startup. Passive USB
capture at115200 identified S3 0.5.0 / ELF463a69017; initial mount fails with
sdmmc_init_spi_crc / sdmmc_send_cmd_crc_on_off 0x106 before filesystem access.
No reset or ROM commands sent, no flashing/settings/card changes. Raw capture
is private and includes network information; publish only sanitized SD evidence.
Host sandbox hides ttyUSB0; approved host serial capture sees it normally.

Prepared S3-only 0.5.1 CMD59 retry backport matches newer Espressif behavior:
retry NOT_SUPPORTED once after10ms, keep CRC enabled, propagate second failure.
Target build PASS and actual disassembly proves init calls the wrapper. Full238
host tests PASS(61.702s), including seven wrapper scenarios; seven additional
offline OTA-plan tests PASS. Physical mount not yet tested. C5 image unchanged.
See docs/S3-SD-CRC-RECOVERY.md. Installed paired uploader needs S3 SD staging;
one-time inactive-slot USB installation requires explicit authorization beyond
this passive capture. Preserve confirmed image/metadata, NVS and partition table.
No device writes authorized/performed in this diagnosis. Separate immediate C5
connected-power-on reset remains unresolved. Keep power rails unconnected.

## Earlier selector investigation

User reports S3 cards unavailable and paired update refuses with exactly
S3 not ready to receive upload; check its SD and connection. On clarification,
S0 is0. CrowPanel Advance V1.3 requires S1=1/S0=1 for MIC & TF/SD routing.
This is the first hardware correction to test; do not assume a firmware SD-driver
bug or format cards. Instruct power off both before changing selector, insert SD
before boot, set both1 and check System(S3) drive before retrying0.5.1 upload.
S1/current card format not newly confirmed, and successful mount not yet observed.
No hardware, device settings, private config, SD contents or firmware changed here.
Existing C5 connected-cold-start concern remains separately unresolved. Keep power
rails unconnected. Prepared0.5.1 release and prior compiled images are unchanged.

## Earlier app/startup work

# Current work: native apps, local logs, connected cold-start diagnosis

User clarified Calculator works onS3 0.5.0. Calendar brieflyopens then exits with
exactlog: [S3]359748[com.cydos.calendar][string main.lua]:292 Execution budget
exceeded(500ms per callback). Clock/Notes/Contacts alsoexit; S3 SD is inserted.
User also reports C5fix3 immediately loops when powered already wired toS3, but
connecting after startup works. Current wiring confirmed TX/RX/GND only, separate
power, no5V/3V3wire. No reset-reason capture, so cold-reset root cause is unknown.
Do not call it the old45s startup rollback or claim a firmware/power cause proven.
C5 USB-UART normal-boot capture requested; no serial port yet visible here.

Prepared0.5.1: 500ms script allowance excludes bounded UI calls and own scheduler
yields; independent5s callback wall cap and Stop checks remain. S3 Home Local logs
works with noC5. Paired RPC send/execute, C5profile/reports and S3sync wait for LOCAL
boot confirmation. Startup checks/stacks/partitions remain; no remotepeer needed.
This isolates startup work but is not proof the immediate cold-reset is fixed.
C5fix3 memory corrections retained. All five native app layouts pass slowUI host
launches. No live device/settings/serial/private-config access or flashing done.
Final0.5.1 software record: 237 full tests PASS(109.820s), both IDF targets
PASS, real ELF LuaABI8 and embedded app source equality/radio ownership verified.
Bundle3089520B SHA256 feeff1c6810b24ff4ede99be0fcb382d29e78d98627f1bfba1baedddbbbb944b. Not installed here.

Power assessment: sharedsupply possible but DO NOT bridge current wirelesssocket
5V pins. S3J9pin7/IOT_5V goes through L3 markedNC beforeVIN inV1.3 schematic+PCB.
One fixed5V supply feeding separateUSBinputs is the simple no-solder plan. Any
board-to-board rail harness needs actualpopulation/voltage/current/backfeed checks.
No powerwirechange authorized/performed. See docs/SHARED-POWER.md and PAIRED-0.5.1.md.

## Earlier startup acceptance and fixes (narrow scope, superseded status)

# Current status: C5 fix 3 startup confirmed by user

User reports latest C5 revision fired up and displayed the startup/firmware
confirmation message: "Ok it all fired up and ‘software confirmed or whatever’
displayed. I think we lined it out". Treat C5 0.5.0 c5-paired-startup-fix-3 as
user-confirmed for startup/OTA acceptance. No independent serial capture, running
ELF hash verification, later power-cycle test or broad RF/storage/peripheral
acceptance was performed. Do not describe this as full hardware validation.
S3 remains on user-confirmed booting 0.5.0; its native apps do not launch and that
issue was explicitly deferred. Do not assume apps or a coordinated OTA now work.
Latest supplied C5 file SHA256:
fa08294b5bc518650376f59b08bfbb0867c4599f27a943c3c2331adbbea341b9.
No device/settings/flash access performed while recording this user report.

## Prepared fix 3 implementation and earlier failure record

# Current status: C5 reached paired services; revision 3 prepared

User reports fix2 now says Paired service initialization failed. In app_main that
means HTTPD and UART initialized. Exact code/heap/substage not provided; do not
claim a confirmed RPC stack versus NVS failure. User clarified power loss was a
separate observation, not the explanation of the ongoing automatic rollback.
S3 0.5.0 boots; native apps still fail and are explicitly deferred. Leave S3 alone.

C5-only 0.5.0 revision c5-paired-startup-fix-3: omit unreachable C5 Apps view/queue
and its8192B worker; draw LVGL inPSRAM, copy/rotate every flush into one15360B
internal DMA buffer. Eliminates23552B internal allocation requests plus overhead,
not measured live heap. Preserve all task stack sizes and mandatory startup gates.
RPC/NVS initialization now logs precise substages and preserves original NVS error
instead of changing all manager failures toNO_MEM. Failed NVS/report start cleans
its resources; no records erased. Retain early HTTPD allocation and PSRAM defaults.
C5 may remain disconnected; no peer presence required to confirm local startup.
No serial/private configuration/live settings/flash access performed in this fix.
Physical acceptance pending. See docs/C5-0.5-PAIRED-SERVICE-FIX.md and fix3 release.
Final fix3 C5 build PASS; 236 full host tests PASS (103.876s). Actual target
disassembly verifies prepare-before-UI and routes-after-UI. Image verified with
SHA fa08294b5bc518650376f59b08bfbb0867c4599f27a943c3c2331adbbea341b9. Current fix3 binary in
outputs/CYD-OS-C5-0.5.0-Startup-Fix-3/UPLOAD-THIS.bin. Not installed here.

## Prior fix 2 record (HTTPD passed; later service failure, superseded)

# Current status: C5 HTTPD failure captured; fix 2 prepared

User photo confirms C5 UI initialized at1528ms, Web API failed at1533ms with
ESP_ERR_HTTPD_TASK(0xb008), largest internal block9216B. HTTPD requests16384B.
At5475ms health missing0x04, UIage3ms/network250ms, stable0; no confirmation.
User also reports 0.4 fallback on power loss: expected pending-verify bootloader
rollback. Fix1 physically failed. S3 0.5.0 boots; native apps fail and are deferred.

Prepared C5-only 0.5.0 c5-web-startup-fix-2: reserve HTTPD before UI, no routes
until UI ready, reuse prepared instance on route registration. C5 general malloc
PSRAM preference threshold0; retain fix1 external logs/WiFi allocations. Internal
stacks/DMA, 32KiB reserve, rollback/10s stability/45s deadline remain. API readiness
still requires web routes + local UART/services; no physical peer required.
C5 may be tested independently, no wiring/settings changes needed. Update onlyC5
through working fallback's numericIP. Keep S3 unchanged. No device or private
configuration access/flash during this fix; physical acceptance pending.
See docs/C5-0.5-HTTPD-RESERVATION-FIX.md and the fix2 release validation.
Final fix2 C5 build PASS; 235 full host tests PASS (110.768s). Actual target
disassembly verifies prepare-before-UI and routes-after-UI. Image verified with
SHA 14df40cf78cc0dbf9494790bebac4677aa1883a45eef43adcc422c9cdef3ab0a. Current fix2 binary in
outputs/CYD-OS-C5-0.5.0-Startup-Fix-2/UPLOAD-THIS.bin. Not installed here.

## Prior fix 1 record (failed physical startup; superseded)

# Current status: S3 0.5.0 installed; C5 startup fix prepared

2026-11-15: User confirms S3 0.5.0 boots. Its native apps currently do not work;
user explicitly deferred that issue. C5 0.5.0 reverts to its prior firmware after
startup missing 0x04; user confirmed Web API initialization failed before that.
Exact HTTPD error code/heap values were not provided. C5's last previously
confirmed working version was 0.4.3c. Do not assume paired 0.5 acceptance.

Prepared C5-only 0.5.0 revision c5-web-startup-fix-1: log history moves 20480B
from internal BSS to PSRAM; paired C5 prefers eligible Wi-Fi/LwIP data in PSRAM.
Required stacks/health gates/rollback, wiring, calibration and private data stay
unchanged. Main startup now preserves exact UART/service error stages/codes.
Keep the installed S3 image. Update C5 through its fallback's own numeric IP.
No serial/device/configuration/flash access performed during this fix. Physical
acceptance pending. See docs/C5-0.5-WEB-STARTUP-FIX.md and the fix release record.
Final C5 build PASS; 232 full host tests PASS (107.801s). Actual ELF log
symbol is 4B (previously20480B); Lua ABI8, paired descriptors, initial blank private
fields and unchanged UART profile verified. Candidate file SHA d8acfe85251e4b2c5482f9a132db1e91f41270c4f68bc17dd1f8843210b15b9e.

## Earlier 0.5 implementation record (superseded installation status)

# Prepared candidate: CYD-OS 0.5.0 paired tablet

User superseded the independent Offline Essentials release with one paired 0.5
system. S3 owns foreground/native apps; C5 owns settings, diagnostics, Wi-Fi/BT,
Files and gateway. Native essentials embedded/top-level. One logical release and
.cydos bundle after an initial per-board 0.5 bootstrap. DO NOT assume installed.
Current physical baseline remains C5 0.4.3c / S3 0.4.2s. No device access/flash in
this task. See docs/PAIRED-TABLET-0.5.md. Work is under CONFIG_CYD_PAIRED_SYSTEM;
standalone 0.4 defaults/previous artifacts are preserved. New candidate builds in
build-paired-0.5.0-{nm_cyd_c5,crowpanel_s3}. 227 full host tests and both IDF target builds PASS. Package is outputs/CYD-OS-0.5.
No device access, configuration changes or flash performed. Physical acceptance pending.

# Start here: CYD OS

Prepared Offline Essentials platform **C5 0.4.4c / S3 0.4.3s**, NOT installed.
Last confirmed C5 is0.4.3c (serial-confirmed/peer-connected); S3 remains0.4.2s.
Six offline app packages1.0.0 prepared; see docs/OFFLINE-ESSENTIALS.md/latest notes.
Diagnostics now belongs only to Settings,
not a launcher shortcut. Defaults in versions/<board>.txt; read docs/BOARD-RELEASE-LINES.md.


This repository is the shared source/context for continuing CYD OS in another chat.
For 0.4.0 also read docs/ARCHITECTURE-0.4.md, docs/LUA-API-0.4.md and
docs/SECURITY-0.4.md. The 0.3.x reference is historical where superseded.
Read [the comprehensive project reference](docs/PROJECT-REFERENCE.md), then inspect
the relevant implementation and tests. Earlier chat workspaces are not shared
memory. Do not assume the user has installed the latest prepared firmware.

- Latest prepared C5 platform: **0.4.5** (not confirmed installed). Primary hardware: user's working RockBase
  NM-CYD-C5, ESP32-C5HR8, 16 MiB flash/8 MiB PSRAM, ST7789 320×240/XPT2046.
- Secondary profile: CrowPanel Advance DIS01943A V1.1–V1.3, ESP32-S3, 800×480/GT911.
  Builds pass; CrowPanel physical revision/operation remain unverified.
- Goal: iPad/ChatGPT app development, Safari `.cydapp` uploads over Wi-Fi/CYD AP,
  organized app versions and wireless native OS updates; USB for bootstrap/recovery.
- User confirmed C5 display/touch, Wi-Fi, SD app deployment, Connections and CYDAP.
  New 0.3.x features have software checks, not a complete physical acceptance report.
- Implemented: bounded Lua runtime, SD packages/revisions, authenticated dashboard,
  saved Wi-Fi/AP manager, watch-inspired Home, Settings, brightness/orientation/
  calibration/clock, logical Apps folders, A/B OTA and startup rollback.
- 0.3.2 adds default-on **Remember this device**, persistent browser auto-login,
  legacy session migration, tab-only opt-out and Forget saved access. Same browser
  and origin are required. Connection failures retain credentials; 401 clears them.
- CYDAP normally uses `http://192.168.4.1`; mDNS hostname is `cyd.local`. CYDAP has
  no NAT/internet forwarding. iPad may switch between phone hotspot and CYDAP.
- ESP-IDF 5.5.4, LVGL 9.1.0, Lua 5.4.8. User's Debian source:
  `~/cyd-work/cyd-os`; private config `build-device-c5/sdkconfig`; USB `/dev/ttyACM0`.
- Preserve credentials, calibration, SD apps and data. Never commit generated
  sdkconfig/private backups, tokens or real network passwords. Keep board builds
  separate. The C5's BOOT pin is GPIO28, not the S3's GPIO0.
- Apps are textual Lua with the actual limited API, not arbitrary ESP32 firmware.
  Same package ID groups versions; every upload gets a fresh random revision ID.
- Current regression evidence: 130 tests, C5/S3 builds and credential-free validated
  C5 OTA artifact. Actual Lua/LVGL runs under ASan/UBSan; portable services use IO
  doubles and real loopback TCP/OpenSSL integration. ESP transport/NVS use SDK
  doubles. Browser tests use Chromium/simulated API. None is physical acceptance.
  Exact results and limitations: docs/VALIDATION-0.4.md.

Current API details: [API](docs/API.md). Updates/recovery: [OTA](docs/OTA.md),
[Recovery](docs/RECOVERY.md). UI/catalog: [Settings and Apps](docs/SETTINGS-AND-APPS.md).
Hardware sources and acceptance gaps: [Validation](docs/VALIDATION.md).

0.4.0 is application-independent. New app permissions are network, mqtt and
credentials; API version remains 1. OS settings/partition schemas are unchanged.
The generic apps/platform_demo is packaged separately; no third-party app is part
of the firmware. See docs/UPGRADE-0.4.md and the current validation record for the
new test/build results and remaining hardware limitations.

C5 file-picker follow-up: user reports greyed-out files in iPad Safari. Local
0.4.1 patch removes accept attributes from both file inputs; labels and strict
server validators remain. Built directly on 0.4.0/PR #1, not experimental pairing.
130 regressions pass. OTA prepared, NOT installed; current C5 remains user-reported
0.4.0. See docs/IPAD-FILE-PICKER.md. Calculator's ZIP-named upload copy is byte-identical
to its .cydapp. Native iPad picker unverified; upload/reset incident is separate.

C5 0.4.2 candidate follows Calculator's successful install but launch request
ESP_ERR_NO_MEM. See docs/C5-APP-LAUNCH-MEMORY.md: app ownership/Lua/service data
explicitly use PSRAM; offline timers/touch use the Lua task without a second 12 KB
service stack. Network workers and internal task stacks remain. Picker fix retained.
130 regressions + Calculator on the updated runtime pass; OTA prepared, not physically
installed/verified. User is away on iPad. No calculator re-upload is needed if its
revision is already installed. Keep S3 bring-up deferred.

C5 0.4.3 candidate: user confirmed 0.4.2 now reaches Calculator main.lua:94 but
errors Invalid color. Actual C5 ELF proves Lua ABI mismatch in prior builds:
private LUA_USE_C89 made library lua_Integer 4 bytes, bindings expected 8. Corrected
to standard consistent 64-bit integer/double configuration, with component build
guards, runtime size handshake and real RV32 compiler regressions. 134 tests pass;
finished C5 ELF library/bindings both 8 bytes. See docs/LUA-ABI-FIX.md. Candidate not
physically verified; same Calculator package/revision can be reused after OTA.
Apply this fix to any future S3/paired build; LP64 host checks alone hid this bug.

User confirmed Calculator now opens successfully on C5 0.4.3. C5 0.4.4 prepared:
responsive native Apps folder/app icon grid, portrait three columns/landscape four,
folder actions under gear, icon-based move chooser. Existing catalog and apps are
retained; Safari dashboard unchanged. 135 regressions pass, including real LVGL
point input and live resizing across four sizes. C5 OTA built; not installed or
physically verified. S3 black-screen bring-up deferred. See docs/APPS-ICON-GRID.md.

C5 0.4.5 prepared (not installed): Settings development-tablet expansion, saved
idle dimming/idle brightness, consumed wake gesture, 12/24h clock, 15-minute offset
steps, live system monitor (also Home Diagnostics), raw/native touch test, display
colors, developer pin reference and board/chip information. No new task or direct
GPIO/module API; preferences in separate cyd_prefs namespace preserve calibration.
141 host tests pass, C5 credential-free OTA built. Native UI tested with SDK/board
doubles at four sizes; device acceptance pending. S3 bring-up/link still deferred.
See docs/DEVELOPMENT-TABLET.md for current capabilities, limits and extension path.

CrowPanel 0.4.5 candidate: latest shared Settings/Lua/app-grid source plus disabled
experimental device-link health/echo integration. Settings > Connected boards and
authenticated /api/status peer_link expose role/state/fresh remote health. UART
release-disabled, no pins assigned, no app/network offload. Separate S3 OTA and
merged USB bootstrap prepared; no flash performed. Existing S3 black screen is
unresolved; startup-stage logs added without changing HAL timing/wiring. 169 host
regressions pass and fresh/stale peer-page rendering checked. Both targets built
from the shared source; existing C5 deliverables unchanged. Read docs/CROWPANEL-0.4.5.md
before installation/wiring. Next: physical S3 boot log and display/touch/SD checks,
then qualify an accessible link that coexists with USB and SD. Never infer that
prepared images were installed. Canonical latest source is this CrowPanel worktree.

2026-11-15: user reports S3 0.4.5 is completely black after flashing. Existing
CYD Flash last-install record at the Local firmware - cyd-os-0.4.5-crowpanel_s3-merged
bundle records verified ESP32-S3 write to /dev/ttyUSB0. Compared its 16 MiB
UPLOAD-THIS.bin with our 1,684,416-byte merged release: exact prefix match, all
remaining bytes 0xff. Bootloader, partitions and application match; not a C5/wrong
image issue. No /dev/ttyUSB0, /dev/ttyACM0 or /dev/serial/by-id visible even through
host read-only inventory at investigation time. No port opened/reset/flashed.
Boot log still required. User confirmed blank appearance but did not supply logs.
Fetched pinned Elecrow Lesson03 main.c and waveshare_rgb_lcd_port.[ch] into
work/crow-vendor-reference. RGB pins/timing, STC 250/0 and GT911 0x5d agree. Vendor
waits 50ms after bus init before STC commands; ours waits after 250; not proven
causal. Ours makes GT911 init fatal before RGB init, and UI allocates two internal
800x24x2 buffers after Wi-Fi initialization; logs needed to distinguish I2C/touch
failure, panel init, memory failure or earlier boot failure. No speculative driver
change or replacement image made. Next: connect S3 USB and obtain 115200 boot log.

2026-11-15 live diagnosis: S3 connected /dev/ttyUSB0, passive 115200 capture saved
work/crow-serial-diagnosis/passive-115200.log. No reset/flash sent. PSRAM memtest,
GT911 reads and RGB init succeed; no SD gives expected 0x108 then Wi-Fi starts.
At 1303ms Starting UI, 1307ms UI initialization failed; app_main returns. This
localizes startup failure; exact old allocation stage unavailable. S3 UI had
2x38,400-byte internal/DMA scratch buffers after Wi-Fi; changed S3-only buffers
to explicit PSRAM because actual IDF RGB draw_bitmap synchronously CPU-copies
into framebuffer. C5 retains internal DMA. Added per-stage memory errors and
scratch/display cleanup. Real-shell LVGL regression with >20KB internal blocks
unavailable passes both targets/all rotations plus injected failures. S3 0.4.6
candidate prepared; physical validation and flashing authorization still pending.
Read docs/CROWPANEL-UI-STARTUP-FIX.md. Existing 0.4.5 output bundles unchanged.


2026-11-15 installation completed with explicit user authorization. S3 application
0.4.6 written only at verified active ota_0 (0x20000); esptool hash verified.
Private pre-write recovery snapshot saved under work/private; protected partition,
NVS, OTA and PHY region readback is byte-identical before restart. Post-flash log:
UI initialized 1398ms, OS startup complete 1405ms, startup checks/firmware confirmed
11764ms. USER CONFIRMED CYD-OS HOME SCREEN VISIBLE. Thus previous UI startup/black
screen failure is resolved. SD absent produces nonfatal warning. Touch interaction,
SD and sustained-load acceptance still pending. No C5 accessed/changed; UART link
disabled/unwired. Outputs contain INSTALLATION-RESULT.md and installation-result.json.
Current S3 is now confirmed installed 0.4.6; do not treat it as factory demo/0.4.5.

2026-11-15: User OTA-installed 0.4.7; black ~45 seconds then rollback to 0.4.6.
No failed-boot capture; exact stage unknown. .7 WITHDRAWN, do not retry/force it.
User wants normal updates over Wi-Fi; do not require USB as routine OTA workflow.
New canonical worktree work/crow-0.4.8 starts from exact working .6 source/archive.
.8 retains ONLY changed-text redraw suppression and UI timing Logs. Board/touch,
I2C speed, input timer, task placement, memory defaults and main boot match .6;
no new raw touch sampling or pinning. Timing string buffer static to avoid UI
stack growth. 170 host tests, S3 build/identity/ABI checks; .8 OTA prepared, NOT
installed or physically verified. Confirmed device remains .6 with working touch.
See docs/CROWPANEL-0.4.8.md. Never disable startup rollback to force .7/.8.


2026-11-15: User reports SAME black/rollback on .8. .8 now WITHDRAWN too. Confirmed
working fallback is .6; no new candidate should be issued before failed-boot data.
Generated .8 sdkconfig matches .6 exactly; static DRAM heap start shifts 176 bytes.
Only two UI production files differ. An OTA/slot/pending-verify startup difference
is now a hypothesis, not established root cause. Reviewed health gate: unchanged
45s timeout with rollback, requires core/board/API/UI/network and fresh heartbeats.
Host tests use SDK scheduling/OTA doubles and did not validate physical pending
boot + full UI startup together. No serial device available (host enumerates only
ttyS0-3). Asked user to temporarily reconnect USB for serial logging WHILE RETRYING
OTA over Wi-Fi; no USB flashing. Waiting for ability/constraint answer. Do not
keep asking user to try speculative performance builds. No setting/flash changed.

2026-11-15 FAILED OTA TRACE OBTAINED: passive USB read while user OTA-retried .8.
At 973ms STC BACKLIGHT I2C TRANSACTION TIMEOUT / ESP_ERR_INVALID_STATE in board init;
app_main returns before GT911/panel/UI. At 46149ms health fails, 46280 rollback,
then .6 board/UI/API succeed and health confirms. Thus this failed attempt is not
UI redraw/core/touch code failure. Underlying electrical warm-reset cause unknown.
Private raw capture work/crow-serial-diagnosis/ota-failed-boot-capture.log contains
network identifiers; do not publish raw. New .9 candidate: startup-only I2C clear,
settling delays, max3 retries for TIMEOUT/INVALID_STATE on existing STC 250/0
commands. Uses IDF bus_reset before bus consumers start. No weakened health or
pin/selector/settings changes. Eight real-sequencer host tests; full suite178.
Read docs/CROWPANEL-OTA-STARTUP-0.4.9.md. .9 NOT yet installed/verified; .6 fallback
still confirmed. User prefers OTA; USB is logging only. Next: user OTA .9 while
passive capture running, observe STC completion/UI/health and home/touch. Do not
claim .7's performance experiments caused the captured rollback.


2026-11-15 .9 RUNTIME OBSERVED: user says OTA restarted. Original 420s capture had
expired with zero data before restart. Follow-up serial read sees .9, POWERON reset
reason, STC touch/backlight complete1045ms, UI1587ms, API1850ms, health confirmed
12784ms. No explicit reset or USB flash sent; do not claim complete warm-OTA
transition was captured. .9 is confirmed running; visible home confirmation and
scroll smoothness pending. Raw followup private due SSID/network identifiers;
public output has sanitized excerpt and runtime-observation.json. Initial paint
handler peak883ms, next idle handler2ms/touch2ms/zero redraws; no scroll benchmark.
Next: user confirms home/version, compares scroll with Logs ui-perf as needed.
Keep .7/.8 withdrawn, .6 fallback retained. Canonical latest source work/crow-0.4.8.

Paged UI task: user observes line-by-line redraw and requests pages + fixed bottom
Back/Home/Next. New canonical source work/crow-pages, baseline .9. Prepare .4.10
OTA for S3 and C5. Native Settings/Connections/Apps/Logs use discrete complete-item
pages, no drag/momentum/elastic scroll; long labels sliced via clipped viewport
without text copies. Footer Back=previous page/parent at first; Home retains app
stop + launcher; Next disabled at end; Home page counter. Header Up is hierarchy
shortcut. Settings choices tap-cycle rather than scrolling dropdowns. Keyboards
Back-cancel; calibration retains full-plane hold2s cancel. Legacy Lua custom
scrolling layouts/API not repacked; Next disabled in an app, Back/Home stop it.
Startup board/control/memory/OTA paths unchanged from .9; no USB/device changes.
182 host tests PASS and both credential-free target builds PASS; prepared files are not installed/physically verified.
Read docs/PAGED-UI-0.4.10.md; Validation logs and target/image verification accompany the outputs.


Board-version split requested by user: next S3=0.4.1s, current C5 baseline=0.4.0c.
New builds embed suffixes in actual app descriptors; no old .4.0 code downgrade.
Launcher Settings/Apps/Logs only; Settings > Diagnostics replaces Live system
monitor label and old standalone shortcut. Back first page -> Settings, page counter
is section-local. Shared source, independent per-board release text files consumed
by CMake and OTA/recovery scripts. Historical explicit versions still supported.
187 host tests PASS; both targets built with their actual suffix version descriptors. Initial private fields blank, image/hash/ABI checks accompany deliverables.
No device write or settings changes. Preserve .4.10 artifacts and installed-version
uncertainty (S3 prior pages user-confirmed; C5 installation not independently checked).


User now confirms S3 0.4.1s looks good. Treat it as the working S3 release. User
asks how to combine S3/C5 into one logical device and whether hardware or software
first. Existing diagnostic/health link remains disabled/unwired; no network/app
offload implemented. Next dependency is qualifying the physical transport/power
path while preserving CrowPanel V1.3 SD and USB routing, then a minimal handshake/
health link before a permission-aware service broker. Do not assume C5 0.4.0c
was installed merely because its build was provided. No device/pin change authorized
by this advisory question.

2026-11-15 pairing work started; user explicitly prefers NO SOLDERING. Manufacturer
V1.3 Eagle schematic AND PCB reveal a candidate that preserves built-in SD and
USB: empty wireless socket J11:6=GPIO8 (proposed S3 UART1 TX), J11:5=GPIO2 (RX),
and OTHER socket row J9:6=GND. GPIO2/8 are direct module connections outside U9/U11
selector. C5 official V1.0 schematic P5:2=GPIO4 RX, P5:3=GPIO5 TX, P5:4=GND,
P5:1=power (leave disconnected). Firmware candidate pins S3 TX8/RX2, C5 TX5/RX4.
Do not confuse J11:6 GPIO8 with J9:6 ground. Keep SD selector S1=1/S0=1. The
earlier wireless-vs-external-SD question is superseded by this newly found route.
Actual wireless-socket occupancy, connector/cable fit and orientation, C5 physical
PCB revision and electrical link operation are NOT verified; asked if socket empty.
No device write/settings/pin initialization or wiring performed. No enabled OTA
prepared. See docs/SOLDER-FREE-PAIRING.md for evidence, power boundaries and next
physical/firmware acceptance. Initial bench power: each USB separately, TX/RX/GND
only, no tied supply rails. Do not use touch IRQ GPIO1 as spare output.

User supplied S3 and C5 rear photos in that order. S3 DIS01943A V1.3 and EMPTY
wireless socket now visually confirmed; original photo S3 USB right/SD bottom.
Module socket right column bottom-up: NC, IO8 (TX), IO2 (RX); left column second
from bottom=GND. C5 is NM-CYD-C5 Colorful; exact PCB revision not legible. Original
C5 photo antenna left/USB right: P5 is white four-contact right-edge UART connector
below BOTH USB ports, not the two top-edge IO sockets. Contacts top-to-bottom by
schematic pin order: GND/P5:4, GPIO5/P5:3, GPIO4/P5:2, 5V-O/P5:1 (leave open).
See outputs/S3-C5-solder-free-link/PHOTO-WIRING-GUIDE.md and WIRING-DIAGRAM.svg in
this chat workspace. No user photo was edited for delivery; diagram is drawn from
scratch. Actual pigtail housing fit, wire contact order/continuity and electrical
operation are still pending. Link defaults still disabled; no enabled OTA built
and no device setting/firmware changed. Do not infer a completed physical cable.

User now reports all three wires connected and asks the next step. Prepared the
FIRST UART-enabled OTA pair: S3 0.4.2s, C5 0.4.1c. Explicit build option
scripts/build_ota.py --link-profile crowpanel-v13-c5-p5 uses isolated build configs,
S3 TX8/RX2, C5 TX5/RX4, 115200 8N1, wiring-confirmed=y. Profile omitted still
builds standalone UART-disabled. Builder rejects wrong/stale pins/config, validates
target/version/layout/hash and blank initial private fields, and records link
config in JSON. Each board's SDK config delta against .1s/.0c is ONLY the five
link settings. No board/display/PSRAM/console/SD/scheduler config change. Guard
also reserves S3 GPIO1 for physical touch IRQ; real adapter tests now use actual
candidate pins. Settings help no longer falsely claims no wiring/pairing assigned.
190 host tests PASS; final help-text change followed by LVGL Settings rerun at
all four sizes PASS. Both final ESP-IDF builds PASS; actual ELF Lua integer/number
functions return8 and library/bindings compile guards retained. Image verification:
S3 1575520B SHA d230c1fbbc585d237556beec1711e5b8874c5d2d6ff292d3486aecb330346ebf;
C5 1757456B SHA f50d67a9c0eaab1cc2dea8b03da5c362f93bd0c930379f93108919e7e4735567.
Artifacts outputs/CYD-OS-Paired-S3-0.4.2s-C5-0.4.1c with per-board *-paired-ota.bin.
NO DEVICE ACCESSED OR FLASHED; both new versions uninstalled/unverified on hardware.
User should OTA C5 then S3 using each own dashboard OS update (not Apps upload),
keep both USB-powered for test, inspect Settings > Connected boards on each for
UART running yes, peer connected, echo0, OTHER version and fresh remote health.
Use Next for readout pages. No service offload/authentication yet. Next task is
physical bidirectional acceptance; see docs/PAIRED-FIRST-LINK.md. Preserve private
NVS, calibration and SD app data. Do not claim the wired pair works until reported.

FOLLOW-UP: User says wired communication works but C5 .1c falls back. Confirmed
automatic ~45s rollback; user also says C5 touch AND its own Safari dashboard work
during the window. No USB serial ports visible even in host read-only inventory;
no log of the failed gate obtained. Found an actual race in OTA health_step:
time-first sampling can see a newer higher-priority UI heartbeat and unsigned age
wraps huge, resetting stability. Fixed by loading both timestamps BEFORE now.
Native preemption injection passes fix, fails when old ordering is reintroduced
(check line173). This proves code defect, NOT the user's exact causal failure.
New C5-only .2c candidate adds 5s startup diagnostics to existing OS update message
and Logs: missing bitmask, UI/network ages, stable seconds, last confirmation error.
No weaker thresholds (2500ms,10s,45s), forced confirmation, wiring, UART, network,
memory, board, scheduling or private-setting changes. S3 stays on its working build.
See docs/C5-OTA-HEARTBEAT-0.4.2c.md. User should install .2c on C5 own dashboard,
check nm_cyd_c5/0.4.2c to avoid duplicate cyd.local hosts, wait>60s; if still pending,
capture Startup line / [os] Logs at20-30s before rollback loses RAM logs. .2c not
yet physically installed/validated. Do not describe this as a confirmed device fix.

.2c final validation: 194 host tests PASS; C5 incremental target build PASS. Actual
ELF Lua integer/number functions return8; chip/board/version/layout/ESP hash, exact
115200 TX5/RX4 link settings and blank initial private fields checked. Output
outputs/CYD-OS-C5-0.4.2c contains OTA, source, patch, evidence and INSTALL.md.
Prior paired output has C5-ROLLBACK-REPORTED.md; original binaries preserved.
Final .2c OTA 1757680B SHA a9b37ac6a72a2770256bc217510a73728941b5971f5b7ef85405ef9b6cd92d94.

Next user report: Safari only says "requested reset" after the .2c instructions;
physical C5 About shows an older version after ~1min. .1c's prior automatic ~45s
rollback was confirmed, but do NOT infer .2c actually booted: it may have rolled
back or never started. Need capture to distinguish. Current dashboard feedback
only changes from Restart requested on confirmed success or polling error; pending
startup may still show that old feedback even as OS update card changes. fetch()
also has no timeout and can leave busy/polling stuck. No further firmware patch
or new image made for this latest report. No serial port visible in host inventory.
User AGREED to connect C5 USB for passive boot logging and asked boot mode/power.
Told NORMAL boot, don't hold BOOT; C5 USB-UART/CH340 to this computer, S3 separate
USB power, three signal wires remain, arrange cables with both off then power
normally. WAIT for connection and start capture BEFORE user retries existing .2c
OTA. USB is for logging ONLY, not flashing, reset commands or settings changes.
Ready tool script work/capture_c5_boot.py (SDK Python with pyserial): deassert
DTR/RTS before open, no toggles/commands, 115200, bounded180s, private0600 log under
work/private/c5-ota-diagnosis (0700). Inspect serial inventory after connected.
Raw boot log may contain SSIDs; don't publish raw or credentials. Capture exact
version/reset/startup diagnostics/confirmation error and actual fallback stage.

User says all hooked up. Host read-only USB inventory still shows NO C5 device,
NO ttyUSB/ttyACM/by-id node. USB sysfs only root hubs and Intel 8087:0a2a internal
device; cdc_acm and ch341 drivers present. Recent kernel journal filter yields no
USB attach/errors. Thus capture HAS NOT STARTED; no port opened/reset/flashed.
Next: known data-capable USB cable from C5 USB-UART/CH340 directly to this computer
(normal boot, no BOOT). Arrange changed cables with both boards off; then power
both normally, S3 separately, TX/RX/GND retained. Wait to OTA retry until actual
serial port appears and capture prints READY. Do not imply a driver fix or logging
already running. Alternative remains fresh Safari C5 numeric IP and .2c Startup line.

ACTUAL C5 FAILURE CAPTURED after user corrected wrong port/bad cable. /dev/ttyACM0
USB JTAG/serial303a:1001 is visible; C5 secondary USB console works. Passive capture
work/private/c5-ota-diagnosis/20261115T191838Z-115200.log (0600) includes initial
0.4.5 confirmed, OTA software reset ->0.4.2c, UI3132ms, web API failure3134ms,
peer connected3144ms, repeated missing=0x04 with fresh UI/network ages, failed
health46930ms, software reset ->0.4.5 confirmed. Exact HTTPD error wasn't logged.
Thus .2c heartbeat race fix is not the observed missing gate. Browser response
was not proof new C5 API worked (old page/duplicate cyd.local possible). No reset
commands, USB flashing or private settings changes sent. Raw has SSIDs: keep private.

Prepared .3c: app_main starts required web and reports API health BEFORE optional
UART adapter. Protocol context explicitly heap_caps_calloc(SPIRAM|8BIT), no internal
fallback; frees on UART config/driver/mutex/task failure. C5 ELF internal engine
storage 4984B ->4B pointer. Driver RX2048B/task stack4096B unchanged. API error
now logs name/code/largest internal block. All readiness/rollback rules retained.
198 host tests and C5 target PASS. Actual ELF ABI8, board/chip/version/layout/hash,
blank initial private fields and TX5/RX4/115200 verified. OTA 1757920B SHA
600f8d58bf2ad0257b4bf6f7a044c38a8935b30375da0a65295c80ce7fb9dca7.
Output outputs/CYD-OS-C5-0.4.3c. Internal-memory pressure remains explanation, not
specifically observed error code; need .3c runtime. No new S3 image prepared.

Fresh passive capture RUNNING session81829 (exec) on /dev/ttyACM0 for300s, private
work/private/c5-ota-diagnosis/20261115T192654Z-115200.log. TOLD USER recording ready,
reload C5 own dashboard then OTA .3c and Restart. Async question pending user
restart notification. Monitor this capture for version .3c, web success/failure,
peer and confirmed >60s. If expired before user OTA, start a new capture FIRST.
No auto-install or USB flash authorized by this flow; user initiates OTA in Safari.

SUCCESS: user chose Restart into update for .3c while capture81829 was running.
Latest capture versions0.4.5 ->0.4.3c, no later reset/rollback. .3c UI3133ms, web API
starts3134ms, local OS startup complete3136ms, peer connected3148ms. Health reports
missing0x00/freshUI/network/stable3then8seconds, firmware confirmed13861ms. Same
boot continues beyond91659ms, latest .3c. This proves OTA confirmation + sustained
past45s deadline + physical peer connection, not exact pre-fix allocation error,
full visual/app load acceptance or service offload. Told user success and no more
update needed. All private settings/SD content retained by normal application OTA;
no resets/USBflash/device-setting commands issued here. Final release output
outputs/CYD-OS-C5-0.4.3c has source/patch,198-test/build evidence, sanitized .2c failed
and .3c successful traces plus runtime-observation.json. Keep raw two capture logs
underwork/private; they contain network identifiers. Older .2c annotated as failed.
Final published observation records same .3c boot through171675ms (>2min), still
confirmed/peer-connected. Passive capture81829 was stopped cleanly with SIGINT
to its exact recorder process only; closed at268s/32774bytes and serial port was
released. No reset commands or device settings changes. No further update needed
for this captured startup failure. Physical visual/touch/app-load followup remains
separate; do not claim those were independently observed in the successful boot.

NEW TASK: user requested basic tablet apps from phone/messaging/contacts/camera/
gallery/browser/email/calendar/clock/calculator/settings/store/files/media/notes/
backup list. User explicitly approved first offline scope: Calculator, Calendar,
Clock/Timers, Notes, Contacts, Files; retain Settings/installer; defer hardware or
service-dependent apps, no fake placeholder apps. No subagents or plugin skills.
Canonical implementation still work/crow-pages. apps/essentials has common Lua
prelude and5new apps plus preserved Calculator source. scripts/build_essentials.py
concatenates standalone main.lua + manifest, packages both targets via realcydctl.
Stable IDs com.cydos.<app>, CalculatorID/version unchanged. All no-network.

Platform additive API1: read-only cyd.time.now() OS validity/UTC epoch/local fields/
offset/12h preference; explicit documents permission for flat shared Documents,
list/read/write/rename/delete,64files,48lowercase filenamebytes,16KiB read/write,
no private app data/SD-root access. FAT interrupted rename recovery hidden.old/.new
and no implicit overwrite; path/NUL/traversal/symlink bounds. Nativeholders protect
C buffers on LuaOOM. cyd.ui.text_lines native14px wrapping,16KiBinput/512lines,
truncationflag; cached app pages<=1000bytes prevent1024widget truncationloss.
UI creation optional padding/border/radius, explicit labelheight honored; olddefaults
unchanged. Manifest C + teststruct + cydctl accepts documents; installer explains.
Runtime now depends on device_settings read-only. No NVS/settings/partition schema
change, no new app thread/task. Existing web-before-peer, PSRAM engine, heartbeat
fix and rollback intact. Both preparedfirmwares use qualified paired UARTprofile.

Apps: Calendar month/weekpages +24eventCRUD/leap validation/datefilter/JSONexport;
Clock OSwalltime +monotoniccountdown/stopwatch +visualdailyalarm, savedlast_day
preventsduplicatealarm on reopen; ONLYWHILEAPPopen/no sound/background/wake.
Notes16shorttitle48/body240bytes; Contacts16/name48/phone40/email80/note160;
eachprivateJSONtwo-slotvalidated snapshots<=7800bytes, CRUD/export sharedDocs.
FilesflatDocuments UI with64-filelist,textread16KiB/first512wrappedlines,binary
hexpreview256bytes,create/edit240bytes,rename/deleteconfirm. DataBOARDLOCAL, no
UARTfile/app/networkoffload or crossboardsync. SD required for packages/data.

Validation:199fullhosttestsPASS, then finalupdatedappworkflowcheckPASS(all6x4sizes
+4APIcases) after cached/byteboundedtextpaging and alarmreopen fix. RealLua/native
bindings/LVGL withASan/UBSan, actualkeyboard submit, CRUD/relaunch/export, leapdate,
timer/stopwatch/alarm persistence, Documents permission/traversal/symlinks/quota/
overwrite/native+LuaOOM/rename recovery,13KBdocument reconstructed across every
page withoutloss.52PPMhostpreviews rendered; source includes syntheticfixtures only.
Both finalIDFtargetsPASS; actualELFLuaABI8, chip/board/version/layout/ESP hash,
exactUARTconfig and blankinitialprivatefields verified. C5 .4c1762272B SHA
d866a04f0ba7fdb813486a10cec57c2e173b3f053e2b4ab51ed2946156ce190c;
S3 .3s1579760B SHAffbb3b897b27baa67b3b6d5f1da3c3b2e2f09c393efefd6f8ba590039785269e.
outputs/CYD-Offline-Essentials includes Apps/*.cydapp +Apps bundlezip, Firmware/
matchingOTA, Sources, Previews, validation, Guide/START-HERE, fullOSSource +patch.
No device/serial/flash/privateconfig accessed this task. NewOS/apps not installed.
RecommendstartS3 .3s +appuploads; C5canstayworking.3ccompanion unless appsonC5wanted.
Update OSFIRST(olderOSrejectdocuments); thenextractAppsZIPuploadindividualcydapp
viaApps, notOSupdate. UserprefersOTA; noUSBflash. Newfirst-launch appdataempty.

Final app-only acceptance rerun PASS (6.949s) after countdown Again/reset and
unchanged-label redraw suppression. CrowPanel controls scaled1.5x; native wrapping
supports14/20/28px fonts. Final target images above revalidated after font support.
All199 full regressions PASS;52final host previews. No device changes or installs.

0.5 final: one common version, paired descriptor feature8/reserved schema1+role.
Standalone .4 defaults remain; use scripts/build_paired.py for role builds and
package_paired.py for .cydos. Bootstrap C5 FIRST, wait >60s, then S3 using its OWN
old dashboard IP. After S3 .5 boots, S3 radio/IP stops by design; C5 IP is the one
portal. Installed0.4 cannot route bundles. No separate essentials uploads needed.
S3 native Calculator/Calendar/Clock/Notes/Contacts verified byte-for-byte in actual
ELF; C5 Files is privileged native C. C5 homes Settings/Diag/Connections/Files/Logs/
BluetoothLE; S3 fiveapps+Apps. .5 overrides .4 Diag-only-in-Settings preference.
Actual ELF Lua integer/numberABI8 on both. S3 has no local esp_wifi_init, NimBLE or
TLS-connect entry points; C5 has them. UART engine/channels/slots remain4B pointers
to PSRAM, JSONhooks toPSRAM installed BEFORE core init. C5 required web starts
before UART/services; .5 APIhealth waits for service init. Existing45s rollback
and10s stable UI/network gates unchanged. S3 network heartbeat represents local
broker health; peer connectivity itself is not a boot requirement.

Service link: old framing plus capability4 CALL/REPLY with receiver boot nonce,
serialized bounded8KiB request/32KiBresponse,192B chunks, no EXEC replay. Ordinary
exchange5s; flash/install/filecommit EXEC30s. Queued requests from stale peer boot
are discarded beforedispatch. Trusted INTERNAL wire, not encrypted/authenticated.
C5 owns existing WiFi/AP/token/SSIDs and new app vault/bonds in NVS; no plaintext
credentialSDexport. Existing S3 privateNVS retained unused, no secretbulk migration.
C5 saves confirmed S3 public displayprofile in cyd_pairprefs; board calibration
stayslocal. Sync C5 time/offset/12h toS3; C5 can recover time from still-powered S3.
Logs/Diag reports bothboards boundedcurrentJSON every30s in C5 reports.
C5 Files selects System(S3)/Device(C5); Documents/Reports writable, Apps/Data/
Backups read-only, paths restricted CYD_ROOT.64entries;2KiB readchunks preserving
UTF8 boundaries;240Btextedit;2MiB otherdrivecopy via staged SHA checked commit;
Home cancels. Not a raw SD-root Lua API. Existing C5 appdata retained/not moved.
Root dashboard app/heap/SD/defaultstorage refer toS3, C5 ip/network retained;
nested host/manager metadata and system_ready handle offline or mixed versions.
C5 settings/recovery stay accessible if S3 absent. BLE lazy scan/explicitcentral
connect/disconnect/bonds; refresh afterscan, no generic GATT/HID/audio/passkey UI.

One .cydos headerCRC/exactroles/offsets/version/perimageSHA; raw .bin bootstrap
separate. C5 header-checks then streams toS3SD. S3 verifies both full hashes +OTA
descriptors/schema/role before flash. On explicitRestart, stopS3apps and gate
launches, stageC5 thenS3, rebootC5, require FRESH bootid +EXACT C5 ELFhash +version
+bootconfirmed beforeS3reboot. Failedstaging cancels without selecting newS3boot;
failedconfirmation canleave compatiblemixedpair, explicitlyreported. No atomic
cross-chip promise. C5 /api/recovery/os[/update,/reboot,/cancel] deliberate rescue.

Final227unit testsPASS(59.598s), plus pairedpreview rerunPASS. Actual C/Lua/LVGL
withASan/UBSan; JSONscope composition, oldqueuedepochreject and allocationcleanup,
fullRPCassembly32KiB, sourceframe fuzz, malformedbundle, role/schema imagepackaging,
copy/hash/paths/symlinks/journal recovery, IOpartialwrites/uncertainwrites/reset
ownership, nativeUI4rotations+bothroles, pairedbrowser oldboot/fallback/offline,
coordinator6scenarios (corruptbundle, uncertainC5write, S3stagefailure, wrongELF,
taskfailure, correctorder). Flash/radio/SDK doubles are NOT physical acceptance.
No CYD Studio emulation claim. All initialprivatefieldsblank verified.
Final paired bundle 3087904B SHA d0df6c7569ef72ce576626960f8d550da4199fd6515af0d35c2d765c80051d68.

Final review: dashboard logs combine source-tagged recent lines from both boards.
Dashboard update confirmation checks the expected S3 ELF digest as well as version,
including same-version rebuilds. C5 top-level Diagnostics Back returns Home;
Diagnostics entered through Settings returns Settings. Short file contents appear
on the first reader page. Final227 suite and both firmware builds PASS after these
changes; physical acceptance remains pending.
