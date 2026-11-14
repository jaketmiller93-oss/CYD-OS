# RockBase NM-CYD-C5 board profile

The user confirmed their C5 is NM-CYD-C5. The exact physical revision/panel
variant has not been inspected. The implemented profile follows the official
**V1.0 schematic and ST7789 configuration**. No physical device testing is claimed.

## Manufacturer sources

- [RockBase product](https://rockbase.shop/products/nm-cyd-c5)
- [Setup wiki](https://wiki.rockbaseiot.com/docs/products/nm-cyd-c5/)
- [Official repository, pinned commit](https://github.com/RockBase-lab/NM-CYD-C5/tree/a1366c3d63363019371dac476a03a01b821ec5de)
- [V1.0 schematic](https://github.com/RockBase-lab/NM-CYD-C5/blob/a1366c3d63363019371dac476a03a01b821ec5de/Documention/2-sch/SCH_NM-CYD-C5-v1.0.pdf)
- The same commit’s `Demos/Platformio/nm-cyd-c5/pins_arduino.h`, `interface.cpp`,
  `Demos/Platformio/_jsonfiles/nm-cyd-c5.json`,
  `Demos/Arduino/libraries/TFT_eSPI/User_Setup-NM-CYD-C5.h`, and
  `Demos/MicroPython/rotations/tft_config.py`.

The schematic labels **ESP32-C5-WROOM-1-N16R8**: 16 MiB flash and 8 MiB PSRAM.
The official board JSON says `qio_qspi`; ESP-IDF 5.5.4 supports C5 quad PSRAM.
CYD uses QIO flash, quad 80 MHz PSRAM, 240 MHz CPU, and reports detected memory.
Do not copy the CrowPanel’s octal-PSRAM/RGB configuration to this board.

## Verified wiring

| Function | Wiring |
|---|---|
| Shared SPI SCK / MISO / MOSI | GPIO6 / GPIO2 / GPIO7 |
| ST7789 CS / DC / backlight | GPIO23 / GPIO24 / GPIO25 (active high BL) |
| LCD reset | Board RST; no invented software reset GPIO |
| XPT2046 touch CS | GPIO1; polling, no IRQ configured |
| SD CS | GPIO10 |
| Onboard addressable LED | GPIO27; not driven by CYD |
| BOOT | GPIO28 in the V1.0 schematic, **not GPIO0** |
| Native USB D− / D+ | GPIO13 / GPIO14 |
| UART USB bridge | CH340; use its USB-C connector for serial flashing/monitoring |

Both display and touch are SPI. This driver uses native ESP-IDF ST7789 support,
20 MHz display transfers, 2 MHz XPT2046 transfers, 10 MHz SD initialization/use,
and the driver’s shared-bus arbitration. All three CS lines are driven high before
bus initialization. SD enters SPI mode **before** LCD/touch traffic, per ESP-IDF’s
SD shared-bus requirement. No extra SPI device may be added without ownership rules.

The official examples configure ST7789 and inversion off. The HAL adds the panel
porch/power/gamma tuning from the official MicroPython example and applies
landscape rotation 3: XY swap with X mirror. It sends RGB565 bytes in the panel’s
wire order and acknowledges LVGL flush only after the SPI transfer completes.
The small display uses internal DMA draw buffers rather than a full RGB frame.

Some documents mention optional ILI9341 panels. That variant is **not** silently
selected or detected. Confirm ST7789 before enabling this profile. If colors,
orientation or initialization disagree, compare the exact installed panel and
manufacturer example before changing the driver.

## Resistive touch calibration

The official rotation-3 PlatformIO sample uses calibration `{225,3413,403,3334,1}`
and reverses both resulting coordinates in its application input handler. That
extra reversal produced opposite-side taps in the user's initial hardware test
with the native ST7789 orientation. CYD now swaps the raw axes without that extra
reversal, takes median-of-five readings, and checks touch pressure. Offsets and
spans are configurable under **NM-CYD-C5 touch calibration** in menuconfig.

Those values are a manufacturer example, not a measurement of your panel. Confirm
all four corners and the Home control on your hardware. There is no interactive
calibration wizard yet. Bad calibration is a remaining physical acceptance risk.

## Build separately from the CrowPanel

Install the C5 toolchain too: `~/esp/esp-idf/install.sh esp32c5`, then activate the
same ESP-IDF 5.5.4 environment used by the S3 project. From the project root:

```bash
python scripts/fetch_lua.py
idf.py -B build-device-c5 -D CYD_BOARD=nm_cyd_c5 -D IDF_TARGET=esp32c5 \
  -D SDKCONFIG="$PWD/build-device-c5/sdkconfig" menuconfig
```

Confirm the selected hardware in CYD OS settings and set initial Wi-Fi/token
values. After first boot, manage Wi-Fi on the touchscreen (CONNECTIONS.md).
Use the **same flags and build directory** for subsequent commands:

```bash
idf.py -B build-device-c5 -D CYD_BOARD=nm_cyd_c5 -D IDF_TARGET=esp32c5 \
  -D SDKCONFIG="$PWD/build-device-c5/sdkconfig" build
idf.py -B build-device-c5 -D CYD_BOARD=nm_cyd_c5 -D IDF_TARGET=esp32c5 \
  -D SDKCONFIG="$PWD/build-device-c5/sdkconfig" -p /dev/ttyUSB0 flash monitor
```

Use your real serial device and keep build configuration private. Never flash an
S3 image to a C5 or share generated sdkconfig files across architectures.

The same `cydctl deploy apps/hello --device <c5-ip> --run` works with this target.
The Hello manifest declares both architectures; runtime APIs and HTTP routes are
shared. The shell adapts to 320×240; small-screen menus and content scroll while
Home remains fixed. Apps can query the UI viewport (see API.md) to adapt layouts.

Run all physical acceptance steps in TESTING.md on this board too. In particular,
verify touch calibration, colors/orientation, SD/display coexistence, repeated
wireless redeployments and actual memory baselines. For ROM recovery, hold the
board’s **BOOT/GPIO28** control and pulse RESET; release BOOT and flash over the
CH340 UART port. Native USB behavior and physical recovery remain untested here.
