# CYDOS 0.4.0 validation

Recorded 2026-11-14 UTC on branch `codex/platform-0.4.0`, based on main
`339d324`. These results apply to the source accompanying this record. Previous
artifacts were inspected as references; their reported tests were not reused.

Environment: ESP-IDF 5.5.4, pinned Lua 5.4.8 and LVGL 9.1.0, native GCC,
OpenSSL, CMake/Ninja, Python and Chromium/Playwright on Linux. No attached device
was flashed, configured or used for acceptance.

## Executed checks

- Fresh-checkout baseline: **98 tests passed**.
- Final `IDF_PATH=/workspace/deps/esp-idf python -m unittest discover -s tests -v`:
  **130 tests passed in 27.644 seconds, no skips**. Build tools were on PATH.
- Actual Lua/LVGL runtime and generic demo rendered at **320×240 and 800×480**
  under **AddressSanitizer and UndefinedBehaviorSanitizer**. This executes native
  bindings, protected callbacks, quotas, cancellation hooks, allocator failures (including legacy storage read buffers),
  private metatables, stale handles, dynamic UI/keyboard/image lifetimes, API 1
  compatibility, JSON and permission checks. The pinned LVGL masked-text update
  leak is avoided in the binding and covered by the sanitizer run.
- Portable HTTP/MQTT/timer implementation executed with deterministic nonblocking
  IO doubles: fragmented messages, framing errors, body limits, deadlines, slow
  writes, cancellation, reconnect/backoff, subscription restoration, queue pressure,
  coalescing/fairness, stale epochs/handles and complete session cleanup.
- Five loopback network tests used **real TCP sockets and OpenSSL TLS** with the
  actual service engine: HTTP GET/POST/status/fragments, trusted private HTTPS,
  wrong verification name, TLS failure without plaintext fallback, slow response
  timeout/close, MQTT TCP/TLS authentication/subscription/message delivery. These
  use a native OpenSSL adapter, not the ESP transport or public internet brokers.
- Five ESP transport tests execute the actual adapter with **lwIP/ESP-TLS SDK
  doubles**: deferred DNS and late callbacks, bounded resolver pool, errors,
  bundle/private CA/name configuration, EAGAIN/EOF and cleanup.
- Four credential tests execute the actual record implementation against an
  **NVS transaction double**: isolation, binary values, quotas, deletion, corruption,
  commit errors, and two full IDs sharing the same first 32 SHA-256 bits.
- Four package-preview tests execute the shared firmware validator and host tools:
  permission inspection, legacy manifests, rejected API/target/entry points,
  300 mutated packages and generic demo inspection. Existing ZIP regression/fuzz,
  loader/storage/catalog/OTA/device-settings and authenticated dashboard tests
  also ran. Browser tests use Chromium with a simulated device API.
- Python syntax checks and `git diff --check` passed. Board drivers, shared-bus
  code, OS UI/settings/network/access implementations and partition layout have
  no diff against the base revision. Credentials/configuration are not bundled.

## Real firmware builds

Both `bash scripts/build_check.sh nm_cyd_c5` and
`bash scripts/build_check.sh crowpanel_s3` completed with ESP-IDF 5.5.4. They
compile/link the actual firmware and enabled board drivers. Their synthetic
hardware-confirmation configuration makes them **compile checks**, not device
updates. They are not distributed as OTA artifacts.

| Compile target | Application bytes | SHA-256 |
| --- | ---: | --- |
| NM-CYD-C5 / ESP32-C5 | 1706192 | `a5f5aa060379d4d0bb324982c60db13c0de5244a418af185fd167a449c583071` |
| CrowPanel / ESP32-S3 | 1529712 | `c3615598a5fb2f77a577e2a4100acf1ff5a4edfbb22780cb9d19bcffac50a373` |

A separate `python scripts/build_ota.py --board nm_cyd_c5 --version 0.4.0`
also completed, using the existing board-specific OTA builder and validator.
All four private configuration fields are blank in that dedicated build. Its
chip, board, version, compatibility descriptor, partition hash and full ESP image
SHA-256 passed validation. No partition changes were made.

- Prepared C5 OTA: `cyd-os-0.4.0-nm_cyd_c5-ota.bin`, **1706192 bytes**.
- File SHA-256: `8bb1bf0faf81d6d03475eb2f7be0a2d7652e8628a414b5a0b592ffda9b6372c7`.
- Embedded ESP image digest: `7136f54666047d8e99f180120e51d63b4ffc8fafdf191056bd209c7c1b5852ac`.
- Partition CSV SHA-256: `0213e45d3f84048bd6ee6d0861b8f2800d9199642cc6053246a83c03a983ab33`.

## Distribution bundle check

The generated source/bootstrap ZIPs passed ZIP CRC checks, payload SHA-256
verification and required-source completeness checks. Applying the actual extracted
bootstrap to a disposable fixture retained its private configuration, existing app
source and partition file, created the private backup and enabled the required
TLS settings. The packaged C5 image was revalidated against its metadata. This
was a host fixture, not an installation on a device. The separately packed
generic demo is 3086 bytes and its requested permissions were inspected.

## Remaining acceptance and limits

**No hardware acceptance was performed.** A controlled C5 pass must verify OTA
staging/confirmation/rollback, retained settings and SD data, radio/DNS/real TLS
interoperability, disconnect/stop during traffic, heap fragmentation under three
TLS contexts, responsive Home/Stop, rotation/touch/shared-bus behavior and credential
persistence/power loss. CrowPanel additionally needs physical revision confirmation
and board acceptance. Physical iPad Safari behavior needs testing too.

MQTT is a documented bounded MQTT 3.1.1 subset: outgoing QoS 0, incoming QoS 0/1,
clean sessions and restored subscriptions, no durable/exactly-once delivery. Network
URLs are IPv4/DNS only; redirects, HTTP/2, client certificates and general image
codecs are not provided. Slow abandoned DNS lookups can temporarily occupy bounded
resolver slots. Queue pressure drops whole MQTT messages with observable counters.

SDK TLS/LVGL allocations add to the fixed service and Lua quotas; there is no
per-app SDK allocator accounting or proof against all memory fragmentation.
Credentials are **not encrypted at rest**. The Lua sandbox is hardened, not process
isolation or a formal security guarantee. See [security/limits](SECURITY-0.4.md),
[API](LUA-API-0.4.md), and [installation/rollback](UPGRADE-0.4.md).
