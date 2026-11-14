# Hardware verification record

Research date: 2026-11-13. **No physical board was attached.** The user confirmed the product family on 2026-11-14 by supplying the
[DIS01943A product wiki](https://www.elecrow.com/pub/wiki/CrowPanel_Advance_4.3-HMI_ESP32_AI_Display.html).
The user subsequently confirmed PCB **V1.3**. This establishes a match to the
manufacturer-source profile; physical firmware operation and cable qualification
remain untested.

## Official sources

- [Elecrow product wiki](https://www.elecrow.com/wiki/CrowPanel_Advance_4.3-HMI_ESP32_AI_Display.html)
- [Elecrow native IDF V1.3 course](https://www.elecrow.com/wiki/CrowPanel_Advance_4.3inch_ESP_IDF_Course_V13.html)
  explicitly identifies ESP-IDF **5.5.4** and the ESP32-S3.
- [Official repository, pinned revision](https://github.com/Elecrow-RD/CrowPanel-Advance-4.3-HMI-ESP32-S3-AI-Powered-IPS-Touch-Screen-800x480/tree/6c8611ed51e44b0e2a9e92c23e89f3993df2742f)
- [V1.3 schematic PDF](https://github.com/Elecrow-RD/CrowPanel-Advance-4.3-HMI-ESP32-S3-AI-Powered-IPS-Touch-Screen-800x480/blob/6c8611ed51e44b0e2a9e92c23e89f3993df2742f/Eagle_SCH%26PCB/1.3/ESP32-Display-4.3-inch-V1.3.pdf)
  and adjacent Eagle `.sch` netlist. The schematic is labelled ESP32-S3-N16R8.
- Under that revision:
  `example/V1.1_and_V1.2_and_V1.3/IDF_Code/Lesson03-LVGL_Control_LED/main/`
  (`main.c`, `waveshare_rgb_lcd_port.h`, `waveshare_rgb_lcd_port.c`) and
  `Lesson05-SD_images/main/SD_Display_Images_70_50_43.cpp`.

The wiki explicitly groups V1.1/V1.2/V1.3 pinouts, while V1.0 uses PCA9557
backlight controls. The verified source profile targets the later group. Verify
against the schematic of your exact physical revision before enabling it. The
V1.3 Eagle schematic and pin list are the primary reference for the implemented
profile; identical behavior across the earlier revisions remains a physical test.

## Display and touch

The display is **parallel RGB565, 16 bits**, not SPI. The official example uses
ESP-IDF `esp_lcd_new_rgb_panel`, so no invented SPI controller initialization is
used. The manufacturer includes an ST7265 datasheet, but this profile follows the
native RGB example’s timing and interface.

| Signal | GPIO |
|---|---|
| RGB D0–D15 | 21, 47, 48, 45, 38, 9, 10, 11, 12, 13, 14, 7, 17, 18, 3, 46 |
| HSYNC, VSYNC, DE, PCLK | 40, 41, 42, 39 |
| I²C SDA, SCL | 15, 16 |
| GT911 address | 0x5D |
| STC8H1K28 control address | 0x30 |

800×480, 16 MHz pixel clock, HSYNC/VSYNC pulses 4, front/back porches 8, falling
pixel clock. Source: the pinned `waveshare_rgb_lcd_port` files. Later-revision
`main.c` sends byte **250** to 0x30 to enable touch and **0** for backlight on;
this code follows those commands. It does **not** copy the stale CH422G 0x24/0x38
helper functions left in that example. There is no invented touch-reset GPIO.

GT911 component 1.1.3 and LVGL 9.1.0 are pinned. The implementation uses IDF’s
current I²C master bus API; it does not mix legacy and new I²C drivers.

## SD and shared peripherals

| SD signal | GPIO / connection |
|---|---|
| MOSI / DI | 6 |
| MISO / DO | 4 |
| SCK | 5 |
| CS / DAT3 | No MCU connection in V1.3 schematic; R33 10K to GND; R29 10K/NC to 3V3 |

The official Arduino SD example declares `SD_CS=0`, but the V1.3 Eagle netlist
contains **no GPIO0–SDCS connection**. CYD uses the supported
`SDSPI_SLOT_NO_CS` setting rather than treating BOOT/GPIO0 as a real SD select.
This follows the actual schematic and still requires a physical mount test.
The SPI bus is dedicated to the card while the function selector is in SD mode.

The wiki specifies **S1=1, S0=1** for MIC & TF Card on V1.1–V1.3. Power off before
changing the physical DIP selector. V1.0’s TF setting is different; that revision
is deliberately unsupported. GPIO4/5/6 are shared with audio/wireless functions;
audio and external hardware APIs are not enabled. GPIO19/20 can be multiplexed
with microphone/UART functions; they are not available to ordinary apps here.
I²C15/16 and all display pins remain OS-owned.

## Flash, PSRAM, recovery and unverified features

The official schematic identifies **N16R8: 16 MiB flash, 8 MiB octal PSRAM**.
The manufacturer IDF defaults use QIO flash, octal 80 MHz PSRAM, and 240 MHz CPU.
CYD uses those defaults, checks PSRAM initialization, and reports detected PSRAM
at runtime. The bootloader’s flash identification must agree with the physical
module. Do not use this 16 MiB partition table on a smaller device.

USB-C uses a USB-to-serial/autodownload circuit in the manufacturer schematic;
check your Linux serial enumeration. BOOT/GPIO0 and RESET/EN provide the normal
ROM download path. See RECOVERY.md. RTC, battery measurement validity, audio,
BLE and the optional wireless module have not been configured or verified.
No battery percentage is fabricated.

NM-CYD-C5 now has a separate SPI HAL based on its manufacturer V1.0 schematic.
See [HARDWARE-C5.md](HARDWARE-C5.md); do not reuse this RGB/S3 configuration.

## Paired-device UART follow-up, 2026-11-14

The user's supplied wiki identifies **DIS01943A**, resolving the model-family
question. It covers V1.0, V1.1 and V1.2/V1.3; the user then separately confirmed
PCB **V1.3**. The existing display/touch/backlight profile remains V1.1–V1.3 only.

The manufacturer lists the following signal assignments (not a complete cable):

| Board port | MCU RX | MCU TX | Constraint |
| --- | --- | --- | --- |
| UART0-OUT / UART0-IN | GPIO44 | GPIO43 | Shares the CH340 USB serial path; OUT and IN also differ in power interface |
| UART1-OUT | GPIO19 | GPIO20 | Multiplexed with microphone/wireless functions; MCU pins also have native USB functions |

Source: [the supplied Elecrow wiki](https://www.elecrow.com/pub/wiki/CrowPanel_Advance_4.3-HMI_ESP32_AI_Display.html).
Do not infer connector orientation or power-pin wiring from this signal table.

The pinned **V1.3 Eagle schematic** adds a restriction not obvious from the port
names: J12 RX/TX connect to U9 A1X/B1Y, while MCU IO19/IO20 connect to U9 AX/BY.
The MIC routes connect to A0X/B0Y and A3X/B3Y. U9 and the other peripheral selector
U11 share SEL0/SEL1. Thus the documented S1=1/S0=1 MIC+TF setting does not select
J12's UART route. Selecting the wireless/UART route also changes the SPI selector;
do not assume UART1 and the current onboard SD route work simultaneously.

J2 UART0-OUT is on ESP_RXD0/ESP_TXD0. Those same nets connect through Q2/Q1 to
RXD0_H/TXD0_H and through R10/R8 to CH340 TXD/RXD. A second transmitter on that
shared path needs an electrical coexistence/isolation plan, particularly with USB
connected. UART0-IN is on the high-side nets; it is not interchangeable with the
3.3 V OUT interface based solely on a four-pin connector. These are netlist
observations, not a measured voltage or an approved wiring procedure.

Primary schematic: [V1.3 Eagle source at the pinned manufacturer revision](https://github.com/Elecrow-RD/CrowPanel-Advance-4.3-HMI-ESP32-S3-AI-Powered-IPS-Touch-Screen-800x480/blob/6c8611ed51e44b0e2a9e92c23e89f3993df2742f/Eagle_SCH%26PCB/1.3/ESP32%20Display%204.3%20inch%20V1.3.sch).
The current S3 build uses native USB Serial/JTAG console configuration, which also
needs review before GPIO19/20 can be allocated to a link. No guard, console config,
DIP setting or GPIO assignment has been changed based on the product URL alone.
See PAIRED-ARCHITECTURE.md for the remaining revision/cable qualification.
