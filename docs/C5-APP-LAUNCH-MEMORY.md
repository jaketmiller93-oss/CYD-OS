# C5 app launch memory candidate: 0.4.2

User report, 2026-11-14:

    446421 [com.cydos.calculator] Installed revision
    476986 [projects] ESP_ERR_NO_MEM
    479553 [projects] ESP_ERR_NO_MEM

Installation succeeded. The error comes from the synchronous revision-launch
request (manifest loading/parsing, app-context allocation or task creation), not
the calculator's arithmetic or UI callbacks. Physical heap diagnostics and serial
output are unavailable while the user is away from home. Do not claim the exact
failing allocation or physical repair has been confirmed.

The release C5 ELF reports sizeof(app_t)=15,328 bytes. Its generated sdkconfig
prefers internal RAM for ordinary allocations up to 16,384 bytes. The old launch
path used ordinary calloc for app_t, followed by a 12,288-byte internal-stack Lua
task. The runtime then created another 12,288-byte internal-stack service task for
every app, including permission-free calculators. Small Lua allocations also
preferred internal RAM. This is avoidable internal-memory pressure even while
PSRAM remains free.

Changes:

- Allocate app ownership data, service-engine data and Lua heap explicitly in
  byte-addressable PSRAM. Existing board initialization already requires PSRAM.
  Keep RTOS stacks, mutexes and hardware DMA allocations internal; do not move
  stacks into PSRAM or reduce their sizes.
- Offline apps (no network or mqtt permission) tick their bounded timer/touch
  engine on the existing Lua task. They no longer create a separate service task.
  Timer registration advances the engine clock to actual registration time;
  missed callbacks coalesce and remain serialized as before.
- Network/mqtt apps retain the existing independent IO worker and permissions.
  Failed starts and teardown close/wipe/free service state, and retain the
  existing completion-semaphore lifecycle.
- Record the failed launch stage with free/largest internal and PSRAM blocks.
  Set the requested revision identity before allocation so failures describe the
  new request. Add internal_largest_block to the authenticated status response.
- Keep the iPad picker fix from 0.4.1. Keep settings/partition schemas, API version,
  app permissions, Lua quota, Home/Stop and OTA validation/rollback unchanged.

Validation: 130 regressions passed, no skips. The actual Lua/LVGL runtime is built
with ASan/UBSan. Extended lifecycle checks assert PSRAM allocation capabilities,
offline timer/touch delivery without a second task, correct late-registered timer
deadlines, network and MQTT worker creation, context/engine/task allocation failure
cleanup and successful relaunch. SDK/RTOS calls in these host checks are doubles.

Calculator's unchanged 1.0.0 source passed 24 arithmetic/error checks and 20 unclipped
keys at each of 320x240, 240x320, 800x480 and 480x800, using the updated actual runtime.
The C5 target compiles with ESP-IDF 5.5.4 and passes the OTA image descriptor,
chip, board, version, partition-layout and SHA-256 validators. Four initial private
credential fields are blank. No real/private sdkconfig was imported.

This is a prepared C5-only OTA candidate, not a physical installation result.
The calculator does not need to be uploaded again if its revision is installed.
After a normal compatible OTA update, launch that revision and check the result.
If it still fails, collect the new allocation-stage/memory log and status values.
The earlier Pocket Pal upload flash/reset incident is not established as resolved.
S3 hardware bring-up is deferred. Carry these runtime fixes into a future S3/paired
branch deliberately; this candidate is based on 0.4.0, not the pairing branch.
