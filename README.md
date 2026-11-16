# CYD OS — wireless Lua application development

Current source: **CYD-OS 0.5.1 paired tablet** — native apps, a C5 device/wireless
manager, shared drive access, S3 Local logs and coordinated update packages.
The CrowPanel S3 now runs the 0.5.1 SD recovery image: its CMD59 retry succeeded,
storage initialized and startup confirmed, with no resets in a two-minute capture.
The C5 remains on its startup-confirmed 0.5.0 fix3 firmware. Its immediate resets
when powered already wired to the S3 remain undiagnosed; attaching after startup
was reported to work. Native-app timing corrections are included, but app and
paired-update acceptance after this recovery still need physical checks.
[Release details](docs/PAIRED-0.5.1.md) · [SD recovery evidence](docs/S3-SD-CRC-RECOVERY.md).
[Architecture, upgrade and limits](docs/PAIRED-TABLET-0.5.md).

Build both roles with `scripts/build_paired.py`; `versions/paired.txt` is their
common version. Initial 0.4-to-0.5 migration needs one board-specific OTA each;
subsequent paired releases use one `.cydos` package through C5 to the S3 coordinator.
Standalone 0.4 board defaults remain available; [historical release lines](docs/BOARD-RELEASE-LINES.md).

**Continuing in a new chat?** Read [PROJECT_CONTEXT.md](PROJECT_CONTEXT.md) and
the [complete project reference](docs/PROJECT-REFERENCE.md) first.

**Downloads:** [Firmware, setup ZIPs and examples](releases/README.md).

The compatible 0.4 Lua API supplies app-owned HTTP/TLS/MQTT, timers, expanded UI,
JSON/resources, scoped NVS credentials and package permission inspection.
Read [the Lua API](docs/LUA-API-0.4.md), [security/quotas](docs/SECURITY-0.4.md)
and [upgrade/rollback instructions](docs/UPGRADE-0.4.md).

An ESP-IDF MVP for **Elecrow CrowPanel Advance 4.3-inch DIS01943A V1.1–V1.3**
and **RockBase NM-CYD-C5 V1.0 with ST7789**.
Edit an application on Linux, upload a `.cydapp` over Wi-Fi, and start a new Lua
context without flashing or rebooting the ESP32.

The user's V1.3 CrowPanel and C5 have confirmed display/touch operation; the
three-wire link and C5 0.4.3c startup confirmation were observed in prior work.
**Hardware acceptance is partial**, as described above. Generic source builds retain
board-confirmation guards. CrowPanel V1.0 and other C5 display profiles are not
supported. Keep settings, calibration and SD data; use normal application OTA.

## Included

- Native ESP-IDF 5.5.4 project; S3 RGB565 800×480 / GT911 and C5 SPI
  ST7789 320×240 / XPT2046 HALs, with separate target memory settings.
- LVGL 9.1 watch-inspired dark shell: round Apps/Settings/Diagnostics/Logs buttons,
  clock/status bar and persistent Home / Stop.
- [Settings and Apps](docs/SETTINGS-AND-APPS.md): brightness, four orientations, safe
  touch calibration, clock, saved folders and grouped app iterations.
- [Authenticated OS OTA](docs/OTA.md): iPad upload/verify/restart, persistent access,
  startup health checks and ESP-IDF rollback. One USB bootstrap from 0.2.0 is needed.
- Touchscreen [connection manager](docs/CONNECTIONS.md): scan/join Wi-Fi, five
  saved profiles, on-screen keyboard, reconnect/forget, and protected AP control.
- FAT SD storage, station/AP/AP+station operation, separate IP display, mDNS.
- Authenticated browser dashboard and deployment API: upload, list, run, stop,
  delete and download revisions; polling logs and memory/storage diagnostics.
- Lua 5.4.8: labels/buttons, bars/containers/views/inputs/images, serial callbacks,
  managed HTTP(S)/MQTT, timers, bounded JSON, resources, scoped credentials, storage, logging,
  bounded memory, instruction hooks, and one foreground application.
- `cydctl`: reproducible stored-ZIP packages, deployment, confirmed startup,
  revision selection, logs, download, and machine-readable output.
- Staging/commit installation: incomplete staging is removed at boot; previous
  revisions and application data are retained. Apps do not auto-run at boot.

This is an incremental MVP. Complete acceptance still requires the
[physical test procedure](docs/TESTING.md), including the remaining paired checks.

## Build and first flash on Linux

Install ESP-IDF’s documented Linux prerequisites (including CMake, Ninja, Git,
GCC, Python and a virtual environment), then:

```bash
git clone --branch v5.5.4 --depth 1 --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf
~/esp/esp-idf/install.sh esp32s3
. ~/esp/esp-idf/export.sh
cd /path/to/cyd-os
python scripts/fetch_lua.py
idf.py set-target esp32s3
idf.py menuconfig
```

For the C5, use the **separate build commands in [HARDWARE-C5.md](docs/HARDWARE-C5.md)**.
The commands above default to the CrowPanel.

Read [HARDWARE.md](docs/HARDWARE.md) before enabling **CYD OS → verified board**.
Confirm the physical product and revision. For V1.1–V1.3, power down and set the
manufacturer’s function selector to **S1=1, S0=1 (MIC & TF Card)**. Insert a FAT32
microSD card. Do not format automatically; the firmware never does so.

Under **CYD OS**, optionally set initial station SSID/password and an initial AP
password (12–63 characters). These are imported once into NVS. Afterward, use
**Settings → Connections** on the touchscreen; see [CONNECTIONS.md](docs/CONNECTIONS.md).
With no station credentials, a protected AP starts; a blank initial AP password
generates one that can be viewed in physical AP settings. Set an independent deployment bearer token (24–240
characters; for example generate one with `python -c 'import secrets;
print(secrets.token_hex(24))'`). A blank deployment token disables the web API. Configurations and build outputs contain secrets; keep them private.

```bash
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Choose your actual serial device. This first USB flash installs the bootloader,
partition table and OS. USB power can stay connected afterward. Future **Lua app**
deployments need no USB data connection or reset.

The shell shows the IP; open `http://<device-ip>/` and enter the token. In AP mode,
join the displayed AP name (initially `CYD-OS`) with its configured/generated password, then open `http://192.168.4.1/`.
`http://cyd.local/` depends on the client/network supporting mDNS.

## Wireless development loop

```bash
python -m venv .venv
. .venv/bin/activate
python -m pip install -e tools/cydctl
read -rs -p 'CYD token: ' CYD_TOKEN; export CYD_TOKEN; echo
export CYD_DEVICE=192.168.4.1   # or the station IP / cyd.local
cydctl status --json
cydctl deploy apps/hello --device "$CYD_DEVICE" --run --logs --json
cydctl logs --follow
```

Modify `apps/hello/main.lua`, bump `manifest.json`’s version if desired, and repeat
`deploy`. Each upload receives a new immutable revision ID even when its semantic
version is unchanged. `--run` stops the previous app, creates a new Lua state, and
waits for `running`; startup errors return a nonzero exit status. Old revisions
remain selectable in Apps and the browser.

```bash
cydctl versions --json
cydctl run 0123456789abcdef             # use a real saved revision
cydctl stop
cydctl download 0123456789abcdef -o hello.cydapp
cydctl delete 0123456789abcdef          # explicit permanent revision deletion
cydctl pack apps/hello -o /tmp/hello.cydapp
```

Use `--token-file /private/path/token` instead of the environment if preferred.
`devices` resolves the configured hostname; it is not a network scanner. `projects`
is an alias for the flat revision listing in this MVP. Select an older revision
with `run` to roll back executable code; app data is shared across revisions of
the same package and is not rolled back. `download` exports one package, not data.

## Architecture

`components/board` selects either `boards/crowpanel_s3` or `boards/nm_cyd_c5`;
each HAL owns its display/touch/SD pins. Common services do not assume a chip or resolution. `storage` owns the mounted
filesystem. `app_manager` validates and stages packages. `app_runtime` owns the
Lua state, bounded allocator, lifecycle and event queue. `ui` owns the LVGL task
and a recursive lock; all app widget operations use that lock. `network` configures
Wi-Fi and mDNS. `web_api` authenticates every API request before accessing software
or logs. The common component defines the interface and strict ZIP reader.

A separate Lua task executes the entry point and callbacks. UI events enqueue Lua
registry references; they never execute Lua on the UI task. The permanent shell
navigation requests cooperative cancellation. Shutdown blocks further API calls, closes Lua while handles are still valid, then
destroys app widgets/handlers before allowing the next launch. Each launch reopens private data under `/sdcard/cydos/data/<package_id>`.

See [API.md](docs/API.md), [HARDWARE.md](docs/HARDWARE.md),
[HARDWARE-C5.md](docs/HARDWARE-C5.md), [TESTING.md](docs/TESTING.md),
[RECOVERY.md](docs/RECOVERY.md), and the
[original specification](docs/SPECIFICATION.md).

## Current limitations and next phases

- The new connection manager and CrowPanel still need physical acceptance tests.
  The RGB path uses a PSRAM framebuffer and
  internal bounce/draw buffers; tearing is possible and must be measured under
  SD/Wi-Fi load. Touch orientation and fixed-low SD select need physical checks.
- Lua is a controlled API, not a security boundary. Hooks limit Lua instructions;
  C library calls and blocked SD I/O are not forcibly preempted. A stop timeout
  prevents a second app from starting; recovery may require reset. No untrusted
  code or untrusted-network deployment is intended.
- Only stored ZIP archives are accepted; use `cydctl pack` or the Python ZIP_STORED
  format. Limits: 512 KiB/package, 48 files, 64 KiB/file, 4 KiB manifest,
  lowercase ASCII relative paths. No ZIP64, encryption, links or compressed ZIPs.
- Storage/network/MQTT/credential permissions are supported. UART/I²C/GPIO APIs and resource leases,
  arbitrary image decoders, lists, sliders and charts remain future phases; unsupported
  manifest permissions are rejected instead of silently granted.
- No arbitrary file browser, nested folders, pinned
  home apps, saved layout, automatic last-known-working selection, retention
  policy, named project metadata, full backups or app-data quotas yet. An app’s
  storage files are each capped at 16 KiB; repeated writes can still fill the SD.
- Wi-Fi/AP profiles are editable on the touchscreen and stored in NVS. The
  deployment token is imported once then persisted in NVS. HTTP uses bearer auth
  over a trusted protected local network; it supplies no transport encryption.
  No API redirects or CORS access are enabled. Core code never logs credentials;
  applications are responsible for not printing sensitive application data.
- Logs are a volatile 64-entry ring. Dashboard polling refreshes the whole ring;
  no WebSocket stream, debugger, battery-backed RTC, battery, audio or BLE support yet.
- OS OTA accepts compatible CYD OS application images only. Standalone third-party
  firmware launching is not implemented. Do not upload `.bin` as a Lua app.
- SD updates use flushed files and a directory rename; FAT is not a transactional
  filesystem. Power loss can corrupt a card. Keep desktop source and backups.

The roadmap follows the specification: validate Phase 1 hardware, validate the
implemented Phase 2–4 path on-device, expand project management and hardware
APIs, then add further board variants. Both initial HALs
are implemented; see the validation records for completed software checks and
remaining physical tests.
