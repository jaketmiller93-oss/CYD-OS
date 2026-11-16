# CYD-OS 0.5.1 — paired tablet with S3 SD recovery

[Download paired package](UPLOAD-THIS.cydos) · [Checksums](SHA256SUMS.txt)

Use GitHub's **Download raw file** for binary downloads. `UPLOAD-THIS.cydos` contains
one application for each chip; it is not a USB merged binary. Native essentials
are embedded and need no separate app uploads. This is the refreshed SD-fix build,
not the earlier 0.5.1 candidate that lacked the CMD59 retry.

## Observed device state

S3 0.5.1 was installed via its verified inactive slot. The SD CRC-enable retry
succeeded, storage initialized, and firmware confirmed at 12.339 seconds. No
resets or SD errors occurred in the 120-second capture. NVS, bootloader, partition
table and previous application were verified preserved. C5 was not flashed and
remains on user-confirmed 0.5.0 startup fix3. C5 0.5.1 is a software-tested candidate.
The C5 immediate connected-power-on reset remains undiagnosed. Native app and
coordinated-update acceptance after S3 recovery still need physical checks.

## Installation

For an already running, connected 0.5 pair with a ready S3 card, open the C5's
own dashboard and upload `UPLOAD-THIS.cydos` in System update. Keep both boards
powered and UART connected during validation/staging. On explicit restart, the
S3 coordinator updates C5 first and requires a fresh exact confirmed C5 image
before restarting S3. Transfers at 115200 baud take many minutes. A failure can
leave a compatible mixed release; cross-chip updates are not atomic.

Cold connected startup needs diagnosis before relying on coordinated recovery.
The separate [C5](Bootstrap/C5) and [S3](Bootstrap/S3) raw applications are for
initial migration or deliberate recovery. They have no universal USB address.
The installed updater needs S3 SD staging, so an SD initialization fault can need
USB inactive-slot recovery before normal paired OTA is usable.

## Contents and evidence

- [Architecture and constraints](../../docs/PAIRED-TABLET-0.5.md)
- [0.5.1 app timing, Local logs and startup isolation](../../docs/PAIRED-0.5.1.md)
- [S3 SD diagnosis and recovery](../../docs/S3-SD-CRC-RECOVERY.md)
- [Physical installation evidence](S3-installation-verification.json)
- [Sanitized startup excerpt](S3-startup-sanitized.txt)
- [Software/build validation](VALIDATION.md)
- [Source snapshot](source.zip); the repository source is authoritative.
- [Previous C5 recovery image](../v0.5.0/C5-Startup-Fix-3)

Build-verification files record the pre-install build phase. The separate S3
installation evidence records the later physical result. No private settings,
NVS backups, raw serial logs, saved networks or personal SD contents are published.
