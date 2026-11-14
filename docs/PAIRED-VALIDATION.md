# Paired-device foundation validation

Recorded 2026-11-14 UTC, branch `codex/paired-device-foundation`, based on CYDOS
0.4.0 source commit 9194fed. The architecture assessment was written before code.
This is a diagnostic transport increment, not a functioning distributed app OS.

## Executed host tests

Final command, with native CMake tools on PATH and IDF_PATH set to ESP-IDF 5.5.4:

```sh
python -m unittest discover -s tests -v
```

**151 tests passed in 50.119 seconds, no skips.** The prior 130-test 0.4.0 suite
ran on this checkout, including actual Lua/LVGL sanitizer execution and real
loopback network integration. An earlier attempt hit the workspace disk limit;
after removing old generated SDK object caches it was rerun successfully. Previous
failed/skipped/incomplete runs are not counted as passes.

New tests:

- **16 link tests** execute the actual portable engine under UBSan. Independent
  Python COBS/CRC frames verify the format rather than comparing source strings.
  Coverage includes both handshake orders, wrong role/version/nonce, binary
  byte-fragmented payloads, CRC/noise/oversize resynchronization, window/queue
  limits, undersized output buffers, cancellation, timeouts, peer loss/reset,
  stale replies/retired HELLO frames, queued-success invalidation and ID exhaustion.
- One of those tests uses a **real POSIX pseudo-terminal** for duplex byte traffic.
  A PTY is not a UART peripheral/cable or proof of physical signal integrity.
- A separate native **ASan/UBSan** executable completes **50,000 interleaved
  malformed/valid traffic iterations** with bound assertions.
- **Five adapter tests** compile and execute actual `peer.c` with explicit UART,
  GPIO and RTOS doubles. They cover disabled/unconfirmed configurations, known
  pin conflicts, UART/mutex/task allocation failures and cleanup. The actual worker
  runs both roles against a real peer engine with three-byte RX fragments,
  seven-byte TX writes and temporarily blocked TX FIFO; heartbeat/echo succeeds.
  Fixture GPIO values are synthetic, not connector/wiring recommendations.

## Real firmware builds

Using ESP-IDF **5.5.4**, all four actual firmware compile/link builds succeeded:

| Target/profile | Link disabled, default | Link enabled, synthetic compile configuration |
| --- | --- | --- |
| ESP32-C5 / NM-CYD-C5 | Passed | Passed |
| ESP32-S3 / CrowPanel | Passed | Passed |

The disabled checks used `bash scripts/build_check.sh <board>` from fresh build
folders. Then only the generated compile-check sdkconfigs were changed to enable
the diagnostic link, wiring flag, UART1 and synthetic unused-in-driver GPIO pairs
(C5 4/5; S3 1/2) at 115200, and the same build commands were rerun. These values
exercise the compiled UART path and are **not approval that those pins are exposed
or safe on either actual board**. No real/private device sdkconfig was changed.

The enabled binaries were produced solely for compile/link checking:

| Target | Bytes | File SHA-256 |
| --- | ---: | --- |
| C5 | 1728848 | `4fe6da866fb812b78c76ffdced7602bbc88d6b8ac8a4a6763a99dbb13b8e3591` |
| S3 | 1547680 | `a2d7119c5611859b56f1edbc46155fbe45d397f26d151be27f0fdbbf095d8ca9` |

No warnings/errors occurred in the final four build logs. **These binaries are not
published as device updates.** The old release downloads still contain 0.4.0;
they do not contain this experiment. No OTA builder/package was produced for this
foundation, no flashing occurred and no device settings were changed.

`git diff --check` passed. There is no diff in board drivers, partitions, existing
network/runtime/services, UI, device settings/access or OTA implementations.
The new link defaults disabled and adds only an authenticated status field plus
an optional diagnostic task during startup. It is not a health prerequisite for
confirming an otherwise healthy OTA boot.

## Remaining limits and acceptance

Exact new Elecrow model/revision and connector pinout remain unknown. Physical
UART, grounding, baud, noise, reconnect/reset, hotplug/power and display/SD
concurrency are untested. Both nodes retain standalone application/network behavior;
only their diagnostic role labels and link can be selected in this increment.

There is **no service offload, routed app upload, paired credentials, background-job
permission/API, cross-chip storage migration or coordinated OTA** in this patch.
CRC and boot sessions do not authenticate/encrypt the wire. No secrets or Lua app
requests use this protocol. See [DEVICE-LINK.md](DEVICE-LINK.md) for limits and
[PAIRED-ARCHITECTURE.md](PAIRED-ARCHITECTURE.md) for the remaining staged integration.

The user's 0.4.0 Pocket Pal upload screen-flash/reset report remains open. Native
full installer execution with the actual package succeeded in a POSIX/SDK fixture;
it does not reproduce the hardware report. Capturing the device crash output
later remains necessary. This architecture change is not claimed to fix it.
