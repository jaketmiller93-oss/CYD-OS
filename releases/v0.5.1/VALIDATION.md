# 0.5.1 publication validation

- Full 245-test host regression suite PASS in 62.061s after the final firmware
  changes, including the seven offline OTA-plan tests. Real C/Lua/LVGL code uses
  ASan/UBSan, with browser dashboard checks. SDK, radio, flash and hardware calls
  are doubled where described in the tests; those are not physical acceptance.
- Both prepared 0.5.1 ESP-IDF 5.5.4 target builds pass. Actual ELF checks verify
  Lua integer/number ABI8, all five S3 embedded app source bytes, radio ownership,
  role/schema, qualified UART pins and initially blank private build fields.
- S3 target disassembly proves SD initialization calls the CMD59 retry wrapper.
  CRC is retained; only NOT_SUPPORTED on enabling CRC is retried once after10ms.
- Image chip, board, version, layout, full ESP SHA and paired-container hashes
  verified. C5 0.5.1 is unchanged from the earlier prepared candidate.
- The S3 0.5.1 recovery image was installed after explicit approval. Its complete
  readback matched; only the inactive application and other OTA selector sector
  were written. Settings/NVS, bootloader, partition table and old confirmed image
  were verified preserved. C5 was not flashed.
- Physical S3 startup: CMD59 retry accepted, storage initialization succeeded,
  firmware confirmed at12339ms; 120-second capture with one boot and no panic,
  resets or SD errors. Only sanitized evidence is published.

Remaining: native-app behavior after recovery, full paired OTA, both drives,
radio/peripheral workloads and the C5 immediate connected cold-start reset cause.
No CYD Studio emulation claim. Do not equate software tests with full physical
acceptance. See current project context and release README for device state.
