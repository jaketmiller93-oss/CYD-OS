# Hardware verification record

Research date: 2026-11-13. **No physical board was attached.** Product/revision
confirmation for the user’s device is pending. This is a manufacturer-source
profile for DIS01943A V1.1–V1.3, not a claim that the user owns that revision.

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
