# 0.4.0 ownership, permissions and resource limits

The OS is application-independent. The only new sample is a generic API demo.
Board drivers, partition layout and existing Wi-Fi/device/access/catalog schemas
are unchanged. App credentials use a new namespace in the existing NVS partition.
No encryption provisioning or eFuse operations are performed.

| Permission | Authority |
| --- | --- |
| storage | Existing app-private removable-SD files, 16 KiB/file |
| network | Read station/AP status and issue outbound HTTP/HTTPS to app-chosen endpoints |
| mqtt | Own up to two outbound MQTT clients; subscribe/publish and inspect state |
| credentials | Read/write/delete records for this package identity in internal NVS |

Every privileged Lua function and handle operation checks the current manifest.
Resources, JSON, timers and UI need no privileged permission. API presence does
not grant permission. No app API exposes OS Wi-Fi credentials, access token, radio
configuration, raw sockets, arbitrary NVS namespaces or unrestricted tasks.

App identity is the full validated package ID. Credential records contain it and
are compared in full. The key namespace uses bounded slots, not an identity hash.
A hash collision cannot grant another identity's records. An attacker who persuades
the owner to install new code declaring the **same** ID is using that identity,
not defeating a hash: no publisher signature model exists. Review preflight identity
and permissions before installing code. Apps with network and credentials can
transmit their own saved credentials; this is the purpose and implication of those
permissions. Physical access to SD/unencrypted flash is outside this boundary.

Credential placement in NVS is not encryption. This build explicitly offers no
encrypted-at-rest credential guarantee. There is no secret-equivalent public API
or automatic logger for these values, but app code may log/exfiltrate its own data.
Neither Lua strings nor all third-party TLS allocations promise secure erasure.
Native service configuration buffers are wiped at teardown.

| Resource | Bound/policy |
| --- | --- |
| Lua heap | 1 MiB, existing allocator |
| Lua callback/entry | 500 ms cooperative hook budget, every 1000 instructions |
| Nested Lua/C transitions | 32; application finalizers disabled |
| App ownership state | Compile-time maximum 32 KiB |
| Async service state | Compile-time maximum 192 KiB, fixed buffers/queue |
| Worker stacks | Lua 12 KiB + app-services 12 KiB; no application task API |
| HTTP concurrency | One; 4096-byte request / 8192-byte response |
| HTTP headers | Request 768 / response 2048 bytes; full TX frame ≤5120 |
| HTTP deadline | 100–30000 ms, includes DNS/connect/read/write |
| MQTT concurrency | Two clients; one shared nonblocking worker with HTTP |
| MQTT payload/packet | 2048 / 2308 bytes; incomplete assembly deadline 5 s |
| MQTT outbox | Two complete bounded packets/client; no unbounded retry queue |
| MQTT subscriptions | Eight/client, restored after clean-session reconnect |
| MQTT reconnect | Exponential 1–30 s; connect/CONNACK 10 s |
| Events | Eight slots; ordinary producers limited to six, reserving two |
| Timers | Sixteen; 20 ms–24 h; missed repetitions coalesce; fair cursor |
| UI | 96 app widgets, bounded helper labels and one keyboard overlay |
| UI text/fonts | 1024 bytes UTF-8, input 256; built-in fonts 14/20/28 |
| Image buffers | Two × at most 32776-byte CIMG resources; no general decoder |
| JSON | 8192 bytes, nesting 8, nodes 256, lexical complexity bound |
| Resources | Current revision only, 64 KiB/file, safe relative paths |
| Credential records | Eight identities × four keys, ≤256-byte values |
| Pending DNS | Four stable OS slots, outliving cancelled app requests safely |

The service/ownership/image bounds are additional to Lua, LVGL and SDK allocations.
TLS uses at most three SDK contexts, fixed configured record buffers (16 KiB input,
4 KiB output in tested builds) and bounded supplied CA strings (4096 bytes). SDK
certificate/crypto allocations are additional native heap; there is no per-app
byte-accounting replacement for the SDK allocator. Allocation failures fail the
operation rather than bypassing verification. These bounds are not a guarantee
that every combination succeeds with fragmented/internal heap on real hardware.
The primary board has PSRAM; low-memory and physical performance still need testing.

Network state machines use absolute deadlines and nonblocking reads/writes.
HTTPS/MQTT TLS always validates chain and verification name/SNI; insecure overrides
and insecure SDK builds are rejected. No plaintext fallback, URL userinfo or
cross-origin redirect forwarding is allowed. IPv4/DNS names are supported; IPv6,
IDN syntax, proxies and client certificate authentication are not in this release.
Local/plaintext protocols are explicitly selected with http:// or mqtt:// and
carry no encryption guarantee.

One Lua task owns callbacks and registry references. IO workers never execute Lua
or LVGL. Widget operations use the OS lock; UI producers enqueue metadata only.
Native method tables do not expose their userdata metatable through an __index
self-reference. Options are read raw, so inherited __index fields cannot execute
reentrant constructors during native resource setup. Every handle has an owner/generation. MQTT has a separate connection epoch, so
queued pre-reconnect data is discarded without invalidating the live client handle.
App shutdown marks closing, stops/joins its worker, frees state under the service
lock, suppresses late producers, then closes Lua and removes widgets/keyboard under
the UI lock. Mutexes are destroyed only after UI producers have been removed.
Static DNS callbacks never point to an app or freed transport.

Queue pressure can drop complete MQTT messages/clicks; input changes coalesce.
MQTT drop counts/errors are observable through client:state(). HTTP completion
waits for space. No exactly-once or durable MQTT delivery is promised. One-shot
and repeating timers do not run in parallel. A hostile peer cannot cause a partial
oversized payload to enter Lua. Slow DNS can temporarily consume the bounded
resolver pool after cancellation, causing a clear busy error for later requests.

The runtime is not process isolation. Native filesystem/C-library/SDK calls are
not universally preemptible, Lua libraries have their own complexity, physical
flash is readable without encryption, and running trusted native firmware controls
the device. The new API removes the finalizer hook bypass; it does not claim a
formally secure sandbox. Continue to treat application installation as a trust
choice. Firmware OTA remains authenticated compatibility/integrity validation,
not publisher signing. It writes only the inactive app slot; existing rollback,
settings, SD data and metadata remain governed by the original OTA design.
