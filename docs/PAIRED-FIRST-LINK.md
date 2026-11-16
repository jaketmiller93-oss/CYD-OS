# First wired pair: S3 0.4.2s / C5 0.4.1c

The user reports the three-wire cable is connected. This release enables the
existing diagnostic UART1 transport for that cable, with separate board images.
It is the first electrically unverified candidate for this actual pair. It does
not yet route Lua, network, MQTT or sensor services to the other processor.

## Exact hardware profile

`crowpanel-v13-c5-p5` requires the CrowPanel Advance 4.3-inch DIS01943A V1.3 empty
wireless socket. S3 J11 pin 6 / GPIO8 is TX; J11 pin 5 / GPIO2 is RX; J9 pin 6
is ground. C5 P5 pin 2 / GPIO4 is RX; P5 pin 3 / GPIO5 is TX; P5 pin 4 is ground.
The fourth C5 P5 power contact is unused. No selector or supply-rail connection
change. See [the hardware evidence](SOLDER-FREE-PAIRING.md).

The connection uses 115200 baud, 8 data bits, no parity, 1 stop bit, no hardware
flow control. UART0 console, S3 USB19/20, SD, display and touch pins stay reserved.
The conflict guard now also reserves S3 GPIO1, which is physically connected to
the touch interrupt even though the driver polls it. The guard change does not
affect GPIO2/8. Adapter tests now exercise the actual candidate pin assignments.

## Build and preservation

With ESP-IDF 5.5.4 and the board compiler installed:

```sh
python scripts/build_ota.py --board crowpanel_s3 --link-profile crowpanel-v13-c5-p5
python scripts/build_ota.py --board nm_cyd_c5 --link-profile crowpanel-v13-c5-p5
```

Version defaults are 0.4.2s and 0.4.1c. The profile uses isolated build directories
and emits `*-paired-ota.bin` files plus JSON metadata. It validates the finished
chip/board/version/layout/ESP hash, blank initial private fields and exact enabled
UART pins/baud. Existing incompatible config is rejected. Omitting `--link-profile`
keeps ordinary standalone builds UART-disabled; no global pin default is changed.

These are application OTA images. Install through **OS update**, not Apps upload;
do not write them as merged USB images. No bootloader, partition, eFuse, calibration,
credential or SD app migration is required. Existing NVS settings and SD data
remain owned by each board. The firmware does not need the other board to boot,
serve its dashboard, pass OTA startup health or run its local apps.

## First acceptance on the user's devices

1. Keep both boards powered for the test; use their separate USB power inputs.
2. Open the C5's own Safari dashboard, select **OS update**, upload its 0.4.1c
   paired OTA, verify and restart. Confirm its local home screen. A waiting peer
   is expected until the S3 is updated.
3. Open the S3's own dashboard and repeat with its 0.4.2s paired OTA. Confirm the
   home screen and touch. Do not install either board's file on the other.
4. On each board open **Settings > Connected boards**. The detail title is
   **Board link**. Use Next if the readout spans multiple pages. Expect UART
   running **yes**, peer **connected**, echo result **0**, and the other board's
   version (S3 sees 0.4.1c; C5 sees 0.4.2s). Fresh remote health includes free heap,
   SD readiness and network-address availability. Echo RTT is milliseconds.
5. Leave it running for a few minutes, use touch and open an SD app. Report the
   readout if it stays waiting, has no fresh remote version, or echo errors grow.
   Logs should record `peer: Peer connected; board status available` after success.

The first upload order is only for clear observation; either startup order is
supported. Health updates are about once a second and echoes about every two
seconds. A missing peer is detected after roughly 3.5 seconds. Do not unplug a
board's power while it remains driven through the signal cable; for later power-loss
tests, disconnect its signal leads first or qualify protective circuitry. Software
session-reset tests already cover restart/loss/reconnect; electrical acceptance is
separate and must not be inferred from them.

No firmware is installed by building these artifacts. Routine updates remain OTA.
After both-way physical acceptance, the next increment can introduce one bounded
remote service and its permission/authentication/cancellation behavior.

## Prepared validation

190 host regressions pass, including actual C link engine/adapter, no-peer/reset/
stale-health/fragmented-IO behavior, the release profile checks, OTA policy and
existing app/UI tests. Final Settings wording was subsequently checked with the
real LVGL test at 240x320, 320x240, 800x480 and 480x800. SDK/board behavior is
doubled in host tests; the pseudo-terminal is not an electrical UART test.

Both final ESP-IDF 5.5.4 target builds pass. Their config differences from the
previous .1s/.0c build are only the five enabled-link options. Chip, board, embedded
version, layout, image SHA, exact link config and blank initial private fields
are checked. Actual ELF Lua integer/number functions return eight bytes, with
library/bindings compile-time ABI guards retained. Validation metadata and logs
accompany the prepared outputs. Physical installation and pairing remain pending.
