# C5 0.4.2c: OTA heartbeat sampling race and startup evidence

The user reports that the 0.4.1c wired link works, touch and the C5 dashboard remain
responsive, but the C5 automatically falls back after about 45 seconds. That
matches the OS startup deadline. No C5 serial port is available here and no failed
boot log has been captured, so the exact failing gate on the device is unproven.
Do not disable rollback or claim hardware acceptance from the host reproduction.

## Reproduced code defect

The health task sampled `now` before loading the UI/network heartbeat timestamps.
The higher-priority UI task can preempt it between those operations (another S3
core can also publish concurrently). If UI publishes a heartbeat one millisecond
after the sampled `now`, unsigned age becomes 0xffffffff milliseconds instead of
fresh. That resets the ten-second stability window. Repeated occurrences can keep
an otherwise responsive system pending until its normal 45-second rollback.

The task now loads both heartbeat timestamps before sampling `now`. The readiness
bits, 2500 ms freshness threshold, ten-second stability requirement, confirmation
API, 45-second deadline and rollback action are unchanged. No link, UART, board,
memory, Wi-Fi, settings or scheduler configuration is altered for this update.

A native test injects UI preemption while the clock is sampled. Reintroducing
the old time-first order makes that test fail (native check line 173); the new
order confirms the same healthy pending image. This proves the code defect and
the regression's sensitivity, not that it caused this user's particular failure.

## If another gate still fails

Every five seconds during unconfirmed startup, the OS update status and Logs now
show a bounded diagnostic line:

```text
Startup: missing=0x00; UI=...ms; network=...ms; stable=...s/10s; confirm_err=0x...
```

`missing` bits: core 0x01, board 0x02, API 0x04, UI 0x08, network 0x10. A zero
mask means all readiness bits have reported; heartbeat ages must also stay below
2500 ms. Stable seconds measure the continuous healthy window. `confirm_err` is
the last confirmation API error, initially zero; zero alone does not prove that
confirmation has been attempted. Successful confirmation replaces this message
with the normal confirmed/updates-available message. No credentials or network
identifiers are added to the diagnostic.

Host checks cover missing API and stale UI reports, a confirmation API error,
healthy confirmation and the preserved rollback paths. Final host and target
build evidence accompanies the candidate. The actual device result is pending.

Final result: **194 host tests pass** and the C5 ESP-IDF target build passes.
The 1,757,680-byte image has file SHA-256
`a9b37ac6a72a2770256bc217510a73728941b5971f5b7ef85405ef9b6cd92d94`.

## C5-only OTA acceptance

Leave the working S3 firmware and cable as they are. On the **C5's own dashboard**
install `cyd-os-0.4.2c-nm_cyd_c5-paired-ota.bin` through **OS update**. Check that the
page identifies board `nm_cyd_c5` and version **0.4.2c**, especially if using a
hostname: both boards advertise `cyd.local`. Use each board's displayed IP address
to distinguish them if necessary.

Keep both boards USB-powered. After restart wait at least a minute. Success is
startup **confirmed**, version still 0.4.2c, and the peer link still connected.
If the status keeps checking, capture its `Startup:` line or the `[os]` Logs at
about 20-30 seconds, before the 45-second rollback clears the RAM log. This gives
the next diagnosis actual gate/error evidence instead of another speculative
performance build. Settings and SD contents are retained by normal application OTA.

Reproduce a clean build with ESP-IDF 5.5.4:

```sh
python scripts/build_ota.py --board nm_cyd_c5 --version 0.4.2c --link-profile crowpanel-v13-c5-p5
```

For preparation here, the existing credential-free .1c build directory was reused
incrementally with explicit `PROJECT_VER=0.4.2c`; the previous published .1c image
was preserved. Final .2c embedded identity, chip/board/layout/hash, exact UART
configuration and blank initial private fields are independently validated.
