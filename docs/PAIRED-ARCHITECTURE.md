# S3 application host + C5 connectivity companion

Assessment recorded before firmware changes, based on 0.4.0 commit 9194fed.
Inspected main boot sequence, Lua/runtime ownership, asynchronous service IO
interface, Wi-Fi manager, web upload/OTA path, board pin usage and host tests.
The requested design is feasible, but it is a distributed device, not shared RAM
or a way to combine both chips into one CPU. Both run at up to 240 MHz; the S3's
second core and separation from TLS/radio work help responsiveness. Its RGB screen
also uses substantial memory bandwidth. Existing Lua heap/time budgets still apply.

## Intended division

| Responsibility | S3 application host | C5 connectivity companion |
| --- | --- | --- |
| Main touchscreen, app Lua, foreground event dispatch | Owns | Local connection/service status screen |
| App packages, revisions, private SD data, catalog | Owns its SD | Authenticated upload front end forwards to host |
| Saved Wi-Fi profiles, station/AP, radio configuration | Reads remote status | Owns; retains existing C5 profiles/settings |
| DNS/TCP/TLS/HTTP/MQTT | Requests managed operations | Runs network workers, bounded queues/caches |
| Background services | Owner manifests, permission/lifetime control | Approved polling/subscriptions continue independently of a view |
| Clock | Local clock, synced from companion when available | Network clock source |
| Updates/recovery | Its own board image and A/B slots | Its own board image and A/B slots |

No display framebuffer crosses the link. Each board draws locally. Commands,
network bodies, service events and chunked files are the link's traffic. Neither
chip can directly dereference the other's memory. An application is loaded once
on S3; it is not automatically divided across CPUs. Future heavier native jobs
need explicit bounded APIs, not unrestricted tasks or hidden Lua threads.

## Transport choice

Start with full-duplex 3.3 V TTL UART, common ground and crossed TX/RX; do not
connect RS-232 voltages or treat power pins as UART. Begin at 115200, then qualify
460800/921600 with real wiring and optional RTS/CTS. With 8N1, 115200 permits at
most 11.52 kB/s and 921600 at most 92.16 kB/s before framing/protocol overhead.
A full 800x480 RGB565 frame is 768 kB. UART is suitable for management/small network
payloads, not high-rate video or large continuous streams; SPI remains a later
transport option if actual traffic justifies it.

Exact exposed connector pins are not established. The S3 is identified only as
Elecrow; DIS01943A V1.1-V1.3 is the existing assumed profile, not confirmation of
the newly bought board. No pin assignments are inferred from chip names. Display,
SD, touch, USB, flash/PSRAM and bootstrapping pins must be excluded. No cable or
power wiring is prescribed until the actual model/revision/connector is verified.

## First implemented increment

The `device_link` component implements a bounded portable COBS/CRC framing engine,
role/version handshake, 64-bit boot sessions, periodic liveness, diagnostic echo,
timeouts, cancellation and bounded queues. An opt-in UART task integrates it with
both existing firmware targets. The authenticated status response reports link
state. The default build does not initialize UART or claim pins.

**This increment is a tested link foundation, not complete network offload.**
Both boards still run their existing standalone OS services. Role labels identify
peers for negotiation; they do not move Lua, disable local Wi-Fi, forward uploads,
share credentials or start background applications. Only diagnostic capability is
advertised. There is no Lua/raw-wire API. No installable paired-system update is
published until the remaining end-to-end work is implemented and tested.

## Remaining integration, in dependency order

1. Qualify the actual UART wiring and S3 board. Exercise reconnect/reset/noise and
   measure throughput/latency with display/touch/SD active. Resolve the reported
   0.4.0 app-upload flash/reset separately; its cause is still unknown without
   hardware crash evidence. The host installer succeeded; that is not a fix.
2. Add authenticated pairing and a versioned service broker. CRC detects accidental
   corruption; it is **not authentication or encryption**. Before transmitting
   app credentials/secrets, establish a unique paired identity and authenticated
   encrypted records, or explicitly restrict the product to a trusted physical
   link with the documented exposure. Never send the dashboard bearer token as
   an implicit pairing key or overwrite the existing device token.
3. Add app/session ownership, full package identity, permissions and generation IDs
   to remote HTTP/MQTT requests. Keep the existing bounded API/callback shape; use
   proxy backends and stream/chunk limits with cancellation acknowledgement. Do not
   retry a side-effecting POST/publish after an uncertain result as a new request.
   Preserve TLS chain/name checks on C5. Reject lost-link operations clearly.
4. Make C5's existing portal forward packages to S3 with bounded chunks, final
   SHA-256 and S3's unchanged validator/staging/atomic revision commit. An offline
   S3 returns a clear unavailable response; never silently install to the C5 SD.
   App package/data/catalog authority stays S3. Do not wire both MCUs to one SD bus.
5. Add an explicit background-service permission/manifest contract and owner quotas.
   Initial jobs should be declarative poll/subscription/cache services, not arbitrary
   unsandboxed Lua on C5. Persist approved job definitions with full owner identity;
   bound retained results, reconnects and service count. View close and service stop
   are different actions. Legacy API 1 Stop still cleans up all owned resources.
   Stopped S3 Lua cannot receive callbacks; deliver bounded cached snapshots on resume.
   Provide per-app stop/revoke controls, including removal/update and a missing host.
6. Coordinate board-specific OTA through the same portal while retaining separate
   A/B validation/rollback. Support adjacent protocol versions during staged updates.
   There is no atomic cross-chip flash operation. A peer failure must not invalidate
   a healthy OS, erase settings or boot-loop while waiting for the other board.

## Preservation and acceptance

Existing C5 Wi-Fi/AP profiles, token, calibration and SD content remain on C5.
Moving app storage to S3 requires an explicit verified migration/copy, not an erase
or automatic rename. Each board keeps its own settings and image. Standalone mode
remains available for recovery. The pet remains a separate package, not firmware.

Tests must cover real portable link execution, corrupted/fragmented frames,
backpressure, timeouts, boot epochs and stale events, then real SDK builds for both
chips. Host transports/SDK doubles and physical two-board acceptance are distinct.
Later acceptance must include remote permissions, lost-link cancellation, service
cleanup, large upload interruption, OTA skew/rollback, and UI responsiveness under
network/SD traffic. No physical wiring, flashing or device settings are changed here.
