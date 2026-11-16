# Experimental diagnostic device link

First physical cable profile and enabled OTA candidates: [PAIRED-FIRST-LINK.md](PAIRED-FIRST-LINK.md)
(S3 0.4.2s / C5 0.4.1c). Ordinary standalone builds remain disabled as described
below. The user reports the cable connected; electrical operation is not yet verified.

Later C5 capture confirms the UART peer connects, but .2c rolls back because its
web service fails. Current C5 candidate and resource ownership changes:
[C5-WEB-STARTUP-0.4.3c.md](C5-WEB-STARTUP-0.4.3c.md). S3 stays on its existing image.

This branch adds transport/session groundwork for a paired S3/C5 system. It does
not provide remote networking or background applications yet. Existing .cydapp
API and OS service/storage behavior remain standalone. Architecture and remaining
integration: [PAIRED-ARCHITECTURE.md](PAIRED-ARCHITECTURE.md).

Current integration/release state: [CrowPanel 0.4.5](CROWPANEL-0.4.5.md).
This protocol was developed in earlier experimental work and is now included
with the latest shared settings/runtime source; the release keeps it disabled.

## Wire format and ownership

Each frame is COBS-encoded, followed by a zero delimiter. The decoded body is:

| Offset | Field |
| ---: | --- |
| 0 | Protocol version, currently 1 |
| 1 | HELLO=1, HELLO_ACK=2, PING=3, PONG=4 |
| 2 | Sender role: S3 host=1, C5 companion=2 |
| 3 | Reserved flags, must be zero |
| 4 | uint64 little-endian sender boot nonce |
| 12 | uint32 little-endian request ID; zero for handshake |
| 16 | uint16 little-endian payload length, at most 256 |
| 18 | Payload, followed by CRC-32/ISO-HDLC over header and payload |

Maximum complete encoded frame allocation is 282 bytes. HELLO carries uint32
capabilities; HELLO_ACK carries the initiator's uint64 boot nonce and uint32
capabilities. The only supported capability is diagnostic echo, bit 0. Different
roles and a valid protocol/nonce echo are required. A correctly echoed ACK can
arrive before HELLO; both startup orders are supported. This is liveness/format
negotiation, **not cryptographic peer authentication**.

An engine owns its own fixed state. IDs never wrap; exhaustion rejects further
requests until a new boot session. Peer boot changes invalidate pending operations
and turn queued old successes into peer-reset errors. The immediately retired
session is rejected. This prevents accidental stale deliveries on a trusted link;
it is not replay protection against an active physical attacker. No Lua raw-link
API, app payload transport, dashboard token or secrets are exposed by this version.

PING payload is echoed byte-for-byte. It has no side effects and is not retried as
a side-effecting operation. Future network commands need their own full app-owner,
permission, cancellation and idempotency semantics, not reuse of diagnostic ping.

## Bounds and failures

- Engine state is compile-time limited to 8 KiB; no engine heap allocation.
- Four pending pings, four queued results, eight outgoing frames.
- Results: 0 success, 1 timeout, 2 peer lost, 3 peer reset.
- Payload <=256 bytes, deadline 100-5000 ms, no truncated payload callbacks.
- Completion queue pressure retains completed pending entries until drained.
- Cancellation suppresses pending/queued results. It cannot unsend wire bytes.
- HELLO every 250 ms before readiness and every 1000 ms afterward.
- Missing valid ACKs for 3500 ms marks the link down and fails pending operations.
- Oversized/noisy input is discarded to the next delimiter; CRC/format/role/version
  failures are counted. Full queues fail explicitly or drop diagnostic traffic;
  no dynamic growth, unrestricted tasks or native callbacks into Lua.

The UART adapter owns a 2048-byte SDK receive ring, a bounded engine context
allocated once explicitly in PSRAM, a 4096-byte internal task stack and bounded
local RX/TX arrays. Context allocation/startup failures clean up; driver overhead
is additional. Required web startup precedes this optional adapter. A dedicated
native task uses nonblocking RX and FIFO TX, including partial writes. It holds
only the engine mutex while updating state; no UART operation runs on the LVGL
thread. It sends a five-byte probe every two seconds and records RTT/results.

## Configuration and status

Default `CONFIG_CYD_LINK_ENABLE=n`: no UART driver, task, pins or pairing state is
initialized. Compile-time options are under Experimental S3/C5 diagnostic link.
GPIO defaults are **-1**, and the explicit wiring-confirmed flag defaults false.
The runtime additionally rejects equal pins, invalid/input-only TX and known
board conflicts. This guard does not prove that other GPIOs are exposed or safe;
check the exact board schematic, module memory pins and connector first. UART1
is used, preserving the USB console/recovery path. Pin choices in host tests are
SDK fixtures, not wiring recommendations.

When enabled, S3 identifies as host and C5 as companion. These labels currently
change only handshake identity; they do not disable local services. Failure to
configure/connect leaves the existing OS available and does not block OTA startup
health. The existing authenticated `/api/status` gains `peer_link`, including
configuration/running/connected flags, protocol/role, last echo result/RTT and
error/reset counters. `last_echo_result=-1` means no result yet. `service_offload`
and `authenticated` are explicitly false. No counterpart is required to boot.

Physical baud starts at 115200 (8N1); higher configured rates up to 921600 need
real acceptance. No user-facing wiring or flash instructions are supplied until
the actual cable, accessible pins, selector routing and USB coexistence are
qualified. The user has confirmed the Elecrow DIS01943A V1.3.

## Test boundaries

`test_link.py` executes the actual C engine with independent Python COBS/CRC frames,
fragmented/corrupted traffic, queue limits, timeouts, cancellation, reset/stale
results and both handshake orders. A real POSIX pseudo-terminal exercises duplex
byte IO; it is not electrical UART testing. A native ASan/UBSan executable mixes
50,000 malformed/valid traffic iterations.

`test_peer.py` executes the actual UART adapter/worker with explicit SDK/RTOS
doubles for disabled/unconfirmed/conflicting configs, allocation failures and
partial/blocked FIFO writes. Both target roles perform real engine echo exchanges.
No pin, cable, display/SD concurrency or physical peer has been tested.

Current test evidence and acceptance gaps: [CrowPanel 0.4.5](CROWPANEL-0.4.5.md).

## Board-health extension (0.4.1-pair.1)

The original diagnostic-only framing above is retained. The new optional
capability adds STATUS=5 with a fixed 56-byte payload and separate sequence;
see the status layout below. Legacy capability-1 peers
still exchange echoes. Fresh public health appears in peer_link.remote and on
Settings > Connected boards in both target builds. This is not remote network/app execution or pairing
security. No wiring-specific image is prepared until the connection is qualified.

STATUS has a nonzero increasing per-boot sequence in the request ID and a fixed
56-byte little-endian payload: receiver boot nonce uint64 at 0, schema=1 at 8,
flags at 9 (SD ready bit0, network address available bit1), reserved zero bytes
10-11, uptime uint64 at 12, free heap uint32 at 20, firmware ASCII string[32] at
24 (NUL terminated/zero padded). Status expires after 3000ms; wrong sessions,
out-of-order sequences, malformed values and peer restart cannot publish stale
health. There are no credentials, network addresses or app payloads in STATUS.
