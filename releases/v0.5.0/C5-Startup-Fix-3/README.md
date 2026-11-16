# C5 0.5.0 startup fix 3 — recovery/history

The user reported successful startup confirmation with this fix. It retains the
HTTP/paired-service health gates, avoids an unused Apps worker, and places eligible
LVGL buffers in PSRAM while preserving internal DMA staging and task stacks.
The immediate reset on power-up already wired to S3 remains unresolved; connecting
after startup was reported to work. This image does not establish a cold-reset fix.

This is a C5 raw OTA application, not a merged USB image or a paired container.
Do not flash at a guessed address; use deliberate C5 recovery or verified inactive
USB application selection. Initial build metadata predates user acceptance.
The current paired source and future candidate are 0.5.1. Keep settings and cards.
