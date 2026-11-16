# C5 0.4.3c: required web service before optional link

## Captured failure

Passive 115200 capture on the C5's native USB serial port proves this sequence:

* Working fallback is **0.4.5**, which confirms startup.
* OTA restart starts **0.4.2c**; UI initializes at 3132 ms.
* Web API startup fails at 3134 ms. The existing message did not include its error
  code, so the precise HTTPD return value is unknown.
* Peer connects at 3144 ms. The physical UART link therefore works in this boot.
* Every startup report has `missing=0x04` (API), fresh UI/network heartbeats and no
  confirmation attempt. This is not the heartbeat sampling race fixed in .2c.
* At 46930 ms the OS fails startup checks, restarts and returns to **0.4.5**.

The sanitized excerpt accompanies the release. Raw capture remains private because
other startup lines include network identifiers. No reset commands or USB flash
were sent. A native USB connection/reset reason appears before the first fallback
boot; the OTA transition itself reports software reset.

The earlier claim that the C5 dashboard worked during the failed window must not
be taken as proof the new C5 web service was running. It was not. Both boards use
`cyd.local`, and an old browser page can remain visible after a failed reconnect.
Use the C5's own displayed numeric IP when identifying its dashboard.

## Change

The required web server now initializes and reports API readiness **before** the
optional UART adapter starts. Its 16 KiB task stack gets priority over optional
allocations. Peer allocation failure leaves the web service/OTA health available;
API failure still blocks confirmation and triggers the normal rollback.

The adapter's bounded protocol context moves from internal static RAM into one
explicit `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT` allocation. The .2c C5 ELF held
4984 internal bytes for `engine`; .3c holds a four-byte pointer there. Engine code
still allocates nothing, buffers/queues retain their bounds, and UART RX buffers
and task stacks keep their existing internal placement. No fallback to internal
RAM is used for the protocol context. Allocation or later startup failure frees
the context and leaves the adapter stopped.

Internal-memory pressure is the leading explanation supported by the added link
memory and HTTPD's internal-stack requirement. The original error code was not
logged, so do not claim a specifically observed ESP_ERR_HTTPD_TASK/NO_MEM result.
If startup still fails, the new log gives the exact web-service error name/code
and largest free internal block in both serial and on-device Logs.

The 115200 TX5/RX4 cable profile, .2c heartbeat ordering fix, startup diagnostics,
2500 ms freshness threshold, ten-second stable window and 45-second rollback
deadline are retained. No pin, selector, board, PSRAM SDK mode, scheduler, web
stack size, credential, calibration or SD schema change. S3 firmware stays as-is.

## Validation and device test

198 host tests pass, including the actual UART worker for both roles with external
allocation failure and cleanup at every startup stage. New tests execute actual
app_main with SDK doubles: required API before optional peer, API failure must not
start peer/report API healthy, and peer failure must not remove API health.
Existing OTA/heartbeat, ABI, UI, browser and service checks pass. These do not
replace physical acceptance.

C5 ESP-IDF 5.5.4 build and final image/board/chip/version/layout/hash, exact UART
profile, blank initial private fields and actual ELF Lua ABI were checked. ELF
`engine` is a four-byte pointer; the allocation is explicitly external. For this
build the dedicated credential-free C5 build directory was reused with explicit
PROJECT_VER=0.4.3c. Clean reproduction:

```sh
python scripts/build_ota.py --board nm_cyd_c5 --version 0.4.3c --link-profile crowpanel-v13-c5-p5
```

Install .3c on the **C5 only** through its own dashboard's OS update. Capture is
started before the user's OTA action. Keep the same USB and interboard wiring.
Success requires version .3c, web API initialized, peer connected and startup
confirmed, followed by staying on .3c past 60 seconds. Until that capture/result
exists, this is a prepared candidate, not a confirmed device fix.

## Observed OTA result

The user OTA-restarted .3c while the second passive capture was running. C5 boot
reports 0.4.3c, UI initializes at3133ms, web startup succeeds, local startup completes
at3136ms and peer connects at3148ms. Diagnostic mask is0x00, with fresh UI/network
heartbeats and increasing stability. Firmware is confirmed at13861ms. The same
boot continues through at least91659ms with no rollback/reset; latest version
remains .3c. This resolves the captured startup-confirmation failure in this test.

No serial reset commands or USB flash were sent. This is evidence for OTA startup
and peer connection; it does not assert full sensor/network offload, a load test
of all apps, or an exact pre-fix HTTPD allocation error that was never logged.
The paired images now have physical startup acceptance on the C5; the S3 was not
updated during this fix. Both sanitized boot excerpts and runtime-observation.json
accompany the release. Raw captures with network identifiers remain private.
