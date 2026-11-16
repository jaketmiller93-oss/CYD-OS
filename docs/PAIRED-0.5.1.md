# CYD-OS 0.5.1 — native app timing and local recovery

## Current installed result

The S3 now runs the 0.5.1 SD recovery image. Its CRC-enable command retry
succeeded, storage initialization completed, and startup confirmed at 12.339s.
No resets or SD errors occurred in a 120-second capture. Settings, partition
table, bootloader and previous firmware were preserved. C5 remains on 0.5.0
startup fix3. Native-app and coordinated OTA physical checks remain outstanding.
Use the refreshed [paired download](../releases/v0.5.1/UPLOAD-THIS.cydos), which
includes this SD correction, rather than the earlier pre-fix 0.5.1 package.
[SD recovery details](S3-SD-CRC-RECOVERY.md).

## Observed behavior

C5 0.5.0 startup fix3 is user-confirmed for standalone startup. The user reports
immediate repeated restarts when it powers up already wired to S3; attaching the
link after startup works. Wiring is reported as TX/RX/GND only, separate supplies.
There is no reset-reason capture, so power/reset/firmware causes remain unresolved.
The power-loss observation was separate, as the user explicitly clarified.

S3 Calculator works. Calendar briefly appears then closes; Clock, Notes and Contacts
also close. S3 has an SD card. The exact Calendar log is:

```text
[S3] 359748 [com.cydos.calendar] [string "main.lua"]:292: Execution budget exceeded (500 ms per callback)
```

This confirms a callback timing failure for Calendar. It does not establish the
individual errors of the other three apps. Those are exercised by regression tests,
but their physical result still needs confirmation.

## Execution budgets

The old wall-clock deadline charged time in bounded native UI functions and the
hook's own scheduler yield to the script's 500 ms allowance. Slow S3 rendering can
therefore terminate a finite drawing callback. The new accounting excludes those
UI calls/yields from that allowance and retains the 500 ms script-time check.
An independent **5-second wall limit** bounds each entry/callback. Stop is checked
at instruction hooks and before/after each wrapped UI call. Standard filesystem,
network or C-library calls remain governed by their existing native bounds; this
is not a preemptive process sandbox or an exact per-thread CPU-time counter.

The UI adapter uses private typed C-function upvalues; native argument positions,
permissions, widget quotas, allocator limits and callback ownership are unchanged.
There are no new app permissions, coroutines, dynamic loaders or background Lua
tasks. Existing applications keep their APIs and private data.

## Local recovery

S3 Home now has **Local logs**, independent of C5, Wi-Fi and SD. It shows the S3's
own bounded log ring and can be opened after an app exits. It makes no peer RPC.
C5 still owns Settings/Diagnostics and its normal Logs view; its home is unchanged.

## Startup isolation

Both directions reject paired service execution until the local OS reports
startup confirmed. UART liveness/handshake can still run. Required worker creation
and all existing health gates remain prerequisites to confirmation, and peer
presence remains optional. Rejected early calls are not queued for later mutation
replay. C5 profile/NVS/report collection and S3 manager polling wait for this same
local acceptance point, rather than running during initial local startup.

This reproduces the sequencing of work after startup at the software level.
It is a mitigation and lifecycle correction, **not proof that the immediate
connected-power-on reset cause is fixed**. If cold boot still resets, capture the
C5's USB-UART reset reason. Preparing the software did not change devices;
the subsequently authorized S3 installation is recorded above.

## Update

This is one **0.5.1** paired system release. Both images retain service schema1,
existing partitions/NVS, qualified pins and the C5 memory corrections from fix3.
Use the single `.cydos` container through an already running, connected 0.5 pair.
Keep both boards powered and the cable in place throughout the staged update.
The coordinator restarts C5 first, requires its fresh exact confirmed image, then
restarts S3. This restart will exercise the known connected-startup condition.
If C5 cannot confirm, the coordinator retains S3's current selected firmware and
reports failure; there is no atomic cross-chip update guarantee.

The bundled raw application images are separate bootstrap/recovery artifacts,
not merged USB images. USB application recovery needs the actual active/inactive
partition state; do not assume writing an arbitrary 0x20000 address selects an
update. The S3 recovery installation used its verified inactive slot. If normal connected startup is unreliable,
serial diagnosis before relying on a coordinated update is the preferable next
hardware step. Do not use power-rail changes as a software workaround.

## Acceptance

Verify both physical versions0.5.1 and startup confirmation, each native app,
S3 Local logs with C5 unavailable, normal app/record persistence, C5 portal and
both drives. Test a cold connected startup separately and retain its reset log
if it fails. Regression evidence and exact image digests are in the release record;
host SDK/task/radio/flash/display doubles are not physical acceptance.
