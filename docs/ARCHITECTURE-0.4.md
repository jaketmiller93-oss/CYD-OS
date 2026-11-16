# 0.4.0 architecture assessment (before firmware changes)

Inspected origin/main at 339d324, PROJECT_CONTEXT.md, project reference, runtime,
manifest/ZIP loader, storage, shell/board interfaces, packaging client, OTA builder
and existing tests. No applicable AGENTS.md or candidate 0.4 patch was found. The
old /workspace/cyd-os tree and binaries are 0.3.x reference artifacts, not evidence
of tests passing on this checkout.

The runtime already serializes execution in one Lua task, caps its allocator at
1 MiB and uses cancellation/instruction hooks with 500 ms callback budgets. UI
calls lock LVGL. Its raw registry-reference touch queue has no owner/generation
and cannot carry safe asynchronous data. Native widget pointers have no deletion
tracking. Manifest permissions currently expose only a storage boolean. SD data
is separated by full package identity, but no packaged resource or NVS credential
API exists. The ZIP loader is strict; its existing format/limits should remain.
Wi-Fi and UI are OS services; neither needs a board change for app networking.

Design: add an app-owned, bounded asynchronous session with a single native
nonblocking worker, generation-tagged events and explicit cleanup before Lua/UI
teardown. Timers, HTTP and MQTT completions enter the same protected serial Lua
dispatch as touch. Use verified TLS with bounded buffers and absolute deadlines;
never call network I/O on LVGL's task. Keep transport/protocol state machines
portable for actual implementation tests, with a separate ESP-TLS/lwIP adapter.
DNS callbacks use stable bounded OS slots rather than pointers into freed apps.
Provide MQTT 3.1.1 with explicit supported QoS, bounded packet assembly, reconnect
backoff and restored subscriptions; replace/disconnect invalidates queued data.

Add native UI bindings through the existing lock and retained app container,
tracking widget/image ownership and delete events. Bound JSON before parsing and
convert into the limited Lua allocator under protected execution. Add read-only
packaged resource access. Store small credentials in a dedicated NVS namespace
with full package identities in records, not a truncated identity hash. No
partition, encryption, eFuse or existing NVS-schema changes are needed.

Permissions are additive to API 1: network, mqtt and credentials, with enforcement
at every privileged binding. Package preflight/inspection must use the actual
loader validation, and host tools must accept the same manifest policy. Existing
apps, SD paths, OS settings and OTA compatibility remain unchanged. A generic
platform demo exercises APIs without any third-party application dependencies.

Validation will run existing regressions on this checkout, new real-Lua/LVGL
host tests and protocol/transport checks, followed by both target builds. SDK
and DNS doubles, loopback network integration and hardware acceptance will be
reported separately. Previously reported test/build outcomes are not reused.
