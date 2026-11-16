# CrowPanel UI startup correction — S3 0.4.6

User connected the S3 without an SD card on 2026-11-15. A passive 115200-baud
capture (DTR/RTS deasserted, no reset command, no writes) shows:

- 8 MiB octal PSRAM detected and memory test passed.
- GT911 product/config reads succeed and RGB panel initialization completes.
- Missing SD returns 0x108; boot continues normally as intended.
- Wi-Fi/AP initializes, followed by `Starting UI` at 1303ms and
  `UI initialization failed` at 1307ms; app_main returns.

This localizes the blank UI to shell initialization. The old generic error did not identify the exact failing allocation. After the
scratch-buffer correction below, physical boot now completes successfully and
the user confirms the home screen is visible.

## Change

The shared shell allocated two width*24*2 buffers from internal DMA RAM. On S3
this is 76,800 bytes, after RGB bounce buffers and Wi-Fi allocations. The S3 RGB
driver uses its own PSRAM framebuffer; IDF 5.5.4 rgb_panel_draw_bitmap CPU-copies
external draw data into that framebuffer synchronously. Its actual RGB bounce
buffers still need internal DMA memory and remain unchanged.

S3 shell draw/rotation scratch buffers now explicitly use PSRAM/8-bit capability.
C5 SPI buffers retain their internal/DMA capability. Failed buffer allocations
release owned buffers/display and return a stage-specific memory report. UI mutex,
input object, queue, folder initialization, task and backlight failures also log
specific stages instead of only a generic startup failure. No pin, selector,
RGB timing, touch address, network credential or storage setting is changed.

## Checks and limits

170 host regressions pass. The added test executes the actual shell startup and
real LVGL, rejects internal allocations larger than 20,000 bytes, and renders all
four rotations for both targets. It also injects first/second scratch-allocation
failure and checks cleanup under ASan/UBSan. Board, heap-capability allocator,
RTOS and NVS are doubles. This does not prove the actual S3 heap's failing stage
or physical display output. The subsequent physical boot log confirms UI initialization and startup health;
the user confirms visible home-screen output. Touch/SD/load testing remains.

The S3 build uses ESP-IDF 5.5.4 and the same board/partition profile as 0.4.5.
Separate OTA/application and merged USB bootstrap files are prepared. The merged
image overwrites settings in its written range. The application image can retain
NVS when installed to the correct existing slot; it is not a merged image and
must never be written at address 0x0. After explicit user approval, the S3 application-only image was flashed at the
verified slot 0x20000. Write hash and protected-region preservation were verified.
The connected C5 is not accessed or updated. The link remains disabled.
