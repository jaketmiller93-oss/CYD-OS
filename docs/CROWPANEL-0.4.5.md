# CrowPanel Advance 4.3-inch V1.3 — 0.4.5

This release combines the latest shared C5 settings/runtime/apps work with the
existing CrowPanel ESP32-S3 HAL and the earlier experimental device-link engine.
It is a build candidate, not a hardware acceptance report. The user's previous
S3 firmware booted to a black screen; this build does not claim to fix that.
The C5's last confirmed Calculator success was 0.4.3; later prepared updates
must not be assumed installed.

## Images

- `cyd-os-0.4.5-crowpanel_s3-ota.bin`: application only, for a working CrowPanel
  CYD-OS web updater with the matching 16 MiB A/B partition layout. Not for C5.
- `cyd-os-0.4.5-crowpanel_s3-merged.bin`: USB bootstrap/recovery at address 0x0,
  containing bootloader, partition table, initial OTA data and the app. Use the
  ESP32-S3 option in CYD Flash. It overwrites flash settings in its written range;
  it is not an OTA upload and does not preserve prior NVS credentials/calibration.
  No SD card contents are included or formatted. User previously waived the S3
  factory backup. Neither image has been flashed by this work.

The compiled profile is DIS01943A V1.1–V1.3 (user confirmed V1.3), 16 MiB flash,
8 MiB octal PSRAM, RGB 800x480/GT911. The source retains the existing panel timing,
control commands, SD wiring and memory configuration. Added startup-stage logging
identifies how far initialization proceeds: board, PSRAM, control/touch I2C, STC
commands, GT911, RGB, saved settings, SD, network, UI and web API. Capture the full
115200-baud boot log next time if the screen remains black. Default S3 console is
UART0, with the SDK's secondary USB Serial/JTAG setting; these pins remain reserved.

Settings, icons, offline Lua apps, app-memory and Lua ABI fixes are included.
Use responsive app layouts and manifests targeting esp32s3. Neither a compiled
image nor a successful handshake test proves the panel/touch/SD work physically.

## Connected-device groundwork included

Earlier framing/health work is integrated with the latest source, rather than
restarting a separate incompatible implementation. The intended roles remain S3
application/UI host and C5 network/module companion. Both remain independent local
OS instances today; no package, app, network or sensor service is offloaded.

Settings > Connected boards shows role, protocol, UART/peer state, echo timing,
error/drop/reset counters, and remote firmware/heap/SD/network-availability status
when fresh. Stale remote status is hidden. The authenticated `/api/status` includes
`peer_link`. This release does not add a Safari pairing card or a pairing button.

Release `CONFIG_CYD_LINK_ENABLE=n`: the adapter initializes no UART, changes no
link pins, allocates no engine/driver/task, and does not require a peer to boot.
GPIO defaults remain -1 with wiring confirmation false. The protocol code is
present in source and tested with both roles; no physical connection is active.
If enabled later, it uses framed COBS/CRC messages, bounded queues, boot nonces,
heartbeats, echo probes and expiring public board-health messages. CRC/session
checking is not cryptographic authentication. Details: [DEVICE-LINK.md](DEVICE-LINK.md).

## Connection qualification before enabling a cable

Boards are not wired together. Do not infer free UART pins from chip labels.
On CrowPanel V1.3, UART1 GPIO19/20 is selector-routed/shared, while UART0 GPIO43/44
shares the USB serial bridge. Display, touch, SD, flash/PSRAM, BOOT and recovery
pins must stay reserved. The pin guard deliberately rejects these known conflicts;
it is not a certification that every other pin reaches a free connector. No DIP
selector changes or console rerouting are made here.

The next hardware step is to get S3 display/touch/SD working and capture its boot
log. Then qualify accessible pins, selector routing and USB coexistence on both
actual boards. Only after that prepare a wiring-specific link build and validate
handshake, repeated resets, unplug/reconnect, sustained framed traffic and
concurrent display/touch/SD activity. No wiring diagram is approved by this release.

After the physical link works, implement a permission-aware service broker with
bounded requests, cancellation, cleanup and authenticated trust before moving
network/modules to C5. Keep display framebuffers and Lua execution local to S3;
this connection is for commands/data, not shared RAM. The architecture assessment
in PAIRED-ARCHITECTURE.md is a roadmap, not implemented distributed OS behavior.

## Validation boundaries

169 host tests pass, including the existing settings/runtime suites, 23 portable
link tests and 5 UART adapter tests. They cover malformed/fragmented traffic,
queue bounds, timeout/cancel, both handshake orders, reset/stale health and driver
allocation/partial writes. PTY tests exercise duplex byte IO; SDK/RTOS/NVS/board
inputs are doubles. Actual LVGL checks include the Connected boards page at four
sizes, fresh-health display and stale-health removal. Host tests do not test
cables, electrical behavior, panel output, SD or physical dual-board operation.

ESP-IDF 5.5.4 builds both board targets from this source. Private initial credential
fields are blank in isolated build configs. Image descriptors, hashes, Lua integer
ABI (library/bindings 8 bytes) and merged segment placement are checked. The link
is confirmed disabled in the built releases. Existing C5 release files and private
configuration are not changed. C5 compatibility build artifacts are retained in
work only; no replacement C5 install is necessary for this CrowPanel candidate.

Reproduce with ESP-IDF 5.5.4 and its S3 toolchain activated:

    python scripts/build_crowpanel.py --version 0.4.5 --output dist/crowpanel

This builds files and never flashes hardware. `Source.zip` includes all shared
source and link tests. Build/test logs and checksums accompany the images.
