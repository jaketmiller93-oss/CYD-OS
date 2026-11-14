# CYDOS 0.4.0 reusable Lua API

API version remains **1**, with additive capabilities and explicit permissions.
Existing labels/buttons, storage paths, textual entry loading, allocator/hook
limits, OS Home/Stop and board settings remain. The new generic example lives in
`apps/platform_demo`; it is a separately packaged app, not installed by the OS.

## Manifest and inspection

Supported permissions: `storage`, `network`, `mqtt`, `credentials`. An empty list
is valid. Unknown permissions and API versions are rejected. Packages retain the
512 KiB / 48 files / 64 KiB per-file ZIP_STORED limits, manifest/entry/API/target
validation and path/CRC checks.

```bash
cydctl inspect apps/platform_demo
cydctl pack apps/platform_demo -o platform-demo.cydapp
cydctl inspect platform-demo.cydapp --json
```

Safari inspects the selected package through authenticated
`POST /api/packages/inspect`, shows its identity and permissions, and asks before
installation. Inspection makes no SD writes and works without an SD. Installation
validates the bytes again through the same C loader. The legacy authenticated
install endpoint remains usable for scripted deployments. Permission declaration
is not publisher authentication; inspect the app code and identity too.

## OS-managed Wi-Fi: `network` permission

```lua
local info = cyd.network.info()
-- station_enabled, station_connected, station_ip, ap_enabled, ap_ip
```

Only a connected **station** counts as station connectivity. A local AP can be
available while the station is offline. Its address is returned separately. The
API reuses the OS connection and cannot scan, join, disconnect, configure the
radio or obtain saved Wi-Fi passwords. Internet reachability is not inferred from
station association/IP alone.

## Async HTTP(S): `network` permission

```lua
local request = cyd.network.request({
    url = "https://api.example.com/status",
    method = "GET",                    -- GET or POST
    timeout_ms = 5000,                  -- absolute whole-request deadline
    headers = "Accept: application/json\r\n",
    -- body = '{"query":"status"}',    -- POST only; binary strings supported
    -- ca_pem = cyd.resources.read("private-ca.pem"),
    -- verification_name = "api.internal.example"
}, function(result)
    if not result.ok then
        print(result.error)
        return
    end
    print(result.status, #result.body)
    -- result.ok means transport/framing completed; 4xx/5xx remain HTTP responses.
end)
-- request:cancel() suppresses pending completion and closes its transport.
```

One outstanding HTTP slot per app. A busy slot/invalid serialized request returns
`nil,error` without replacing the active request; invalid argument types/ranges
and missing permissions raise runtime errors. Request body ≤4096 bytes, response body ≤8192,
URL ≤768 with path ≤510, request headers ≤768, response headers ≤2048. Serialized
request headers plus body must also fit the 5120-byte transmit buffer. Timeout is
100–30000 ms. Failures are callbacks with `ok=false`, status -1, empty body and a
bounded error. Successful framing returns status/body with `ok=true`, including
HTTP error statuses. Cancelled/replaced handles cannot deliver stale completions.

Header text is explicit CRLF-terminated `Name: value` lines. Host, Content-Length,
Transfer-Encoding and Connection cannot be supplied by apps. No redirects or
HTTP authentication retries are followed; credentials are never forwarded to a
redirect target. HTTP/1.0–1.1 content-length, connection-close and strict chunked
bodies are supported. Interim 1xx responses, chunk extensions/trailers, HTTP/2,
WebSocket, streaming and content decompression are not implemented; unsupported
framing fails or the raw body is returned as documented. No HTTP headers are
returned in this version. Use status and raw body explicitly.

HTTPS uses the SDK certificate bundle by default, or the provided bounded PEM CA.
The URL host is the default verification name/SNI; use `verification_name` for a
private service contacted by IP. Certificate and name checks are mandatory.
Insecure/skip/verify override options are rejected; there is no plaintext fallback.
A valid wall clock is needed for certificate validity checks. Set/sync the OS clock
before TLS when offline after power loss.

The ESP adapter resolves IPv4/DNS names asynchronously through bounded lwIP slots,
then performs a nonblocking ESP-TLS connection to the resolved address with the
original verification name. IPv6/IDN URL syntax, proxies and client certificates
are not supported. Timed-out/cancelled DNS requests leave only a bounded static
resolver slot until lwIP finishes; they retain no app pointer. New requests can
fail with DNS-slot-busy while those slots drain.

## App-owned MQTT: `mqtt` permission

```lua
local client = cyd.mqtt.connect({
    url = "mqtts://192.0.2.20:8883",
    verification_name = "broker.internal.example",
    ca_pem = cyd.resources.read("broker-ca.pem"),
    -- username = "account", password = cyd.credentials.get("broker_password"),
    -- client_id = "my-explicit-client-id",
    on_state = function(state, error)
        print(state, error) -- connected / reconnecting
    end,
    on_message = function(topic, body)
        print(topic, #body) -- complete binary payload only
    end
})
assert(client:subscribe("monitor/status/#"))
-- Once connected and subscription restoration has progressed:
local accepted = client:publish("monitor/query", "status", false) -- retain flag
local state, error, dropped_messages = client:state()
client:disconnect()
```

`mqtt://` uses plain TCP; `mqtts://` uses verified TLS. TLS options match HTTP.
Username is optional; password requires username. Credentials are copied into
owned native state and are not logged. Omitted client IDs are generated uniquely
per connect call (20 ASCII characters); explicit IDs are ≤64 bytes. At most two
clients per app; connection-capacity errors return `nil,error`. Client ownership/handle generation survives reconnects, while a
separate connection epoch invalidates messages queued before disconnect/reconnect.
A disconnected/replaced client handle cannot affect a replacement.

This implements a bounded **MQTT 3.1.1 subset**: outgoing publishes/subscriptions
use QoS 0, incoming QoS 0/1 PUBLISH is handled (QoS 1 is acknowledged), and SUBACK,
CONNACK and keepalive PINGRESP are validated. QoS 2, outgoing QoS 1, unsubscribe,
will messages, MQTT 5 and persistent broker sessions are not exposed. Publish
acceptance is enqueue acceptance, not broker/application delivery confirmation.

Topics/filters are printable ASCII ≤128 bytes; filters support valid `+`/`#`
levels. Username ≤128 bytes; password ≤256. Payloads ≤2048 bytes; assembled packets
≤2308 bytes. Two bounded outgoing packets per client; queue-full/invalid/not-ready
publish returns false. There are eight saved subscriptions per client, restored
after each successful CONNACK. Backoff doubles 1,2,4,8,16,30 seconds and caps at 30.
Connect/CONNACK deadline is 10 seconds; each outgoing packet has a 5-second
write deadline; incomplete packets have a 5-second assembly
deadline. Keepalive is 20 seconds, with PING after 10 seconds of receive inactivity
and a 5-second response deadline. Fragmented/incomplete/oversized frames are never
truncated into callbacks. Malformed/oversized input closes the connection and
reconnects. Outgoing queued publishes are discarded on disconnect.

Under callback backpressure, whole incoming messages can be dropped. `state()`
reports the cumulative dropped count and last error. State callbacks are also
bounded/best-effort; poll state when a current snapshot is required. This API does
not promise exactly-once delivery or persistence across an app stop.

## Timers and serial dispatch

```lua
local once = cyd.timer.after(1000, function() print("once") end)
local repeating = cyd.timer.every(1000, function() print("tick") end)
repeating:cancel()
```

Intervals are 20 ms–24 hours. Sixteen timers per app; missed repetitions coalesce,
and fair rotating scheduling prevents lower-numbered timers monopolizing slots.
One-shot events wait for queue room rather than firing partial callbacks. Timers,
network and touch callbacks all run serially under the existing protected Lua
execution hook/budget. Timer-capacity exhaustion returns `nil,error`. There are no app threads, tasks
or coroutine APIs.

The shared event queue has eight slots; two are reserved from ordinary timer,
touch and message traffic for network completions/state. Input changes coalesce;
excess clicks/messages may be dropped. HTTP completions wait for queue space.
App shutdown cancels timers/network, waits for the native worker, prevents late UI
producers, then tears down Lua and native widgets. No old app's event enters a new
app's queue.

## UI additions

All widgets belong under the existing app root, preserving the OS-owned Home/
Stop control. Use `cyd.ui.size()` rather than assuming full display dimensions.

```lua
local root = cyd.ui.create_screen()
local width, height = cyd.ui.size()
local view = root:add_view{x=0,y=0,width=width,height=height,
                           scroll=true,layout="column"}
local box = view:add_rectangle{width=240,height=60,background=0x202020}
local label = box:add_label{text="Connected",color=0x30d158,font=20,width=220}
local bar = view:add_bar{width=240,height=16,value=25,color=0x30d158}
local field = view:add_input{width=240,text="",masked=true,
    on_submit=function(text) print("Submitted bytes: "..#text) end}
label:set_text("Updated")
label:set_color(0xff9500)
box:set_background(0x101010)
bar:set_value(75)
local text = field:get_text()
cyd.ui.show(view)
```

Methods include add_label, add_button, add_bar, add_container/add_rectangle,
add_input, add_view and add_image. Containers support `layout="column"/"row"` and
`scroll=true`. Views must be direct children of the app root. Showing one hides
other views; showing the original root hides the views and preserves API 1 behavior.
`set_text` updates labels, buttons and input; `get_text` reads them. `delete()`
removes a widget and descendants, invalidating retained handles. The root cannot
be deleted. Values are 0–100; colors are 0x000000–0xffffff; fonts are 14/20/28;
x/y are 0–800 and dimensions 1–800. Parent clipping still applies. There are 96
user widgets; helper button labels and one input overlay are separately bounded.
Text is valid UTF-8, ≤1024 bytes; input is ≤256 bytes. The native input overlay
stays above Home/Stop, supports masking and emits bounded change/submit callbacks.
The pinned LVGL 9.1 masked-text allocation bug is avoided by a synchronized wrapper.

Images use a small raw format rather than an unbounded general decoder:
`root:add_image{resource="icon.rgb565",x=0,y=0}`. The resource is `CIMG`, then
little-endian uint16 width/height, then exactly width×height little-endian RGB565
pixels. Maximum 128×128, two live images/app, natural size only. Native data and
LVGL cache entries are released on widget deletion/stop; stale widget handles
cannot access replacement images. No filesystem/global image decoder is exposed.

## JSON, resources and credentials

`cyd.json.decode(bytes)` accepts ≤8192 bytes, nesting ≤8, ≤256 parsed nodes and a
bounded lexical complexity. It rejects malformed/trailing data, invalid UTF-8,
NUL aliases, duplicate keys and non-finite numbers. Invalid input returns `nil,error` rather than executing a callback error; allocator
exhaustion still raises. JSON null is `cyd.json.null`;
numbers are Lua numbers (large integer precision is not guaranteed). Parse memory
is released even when Lua conversion runs out of allocator space.

`cyd.resources.read("relative/path")` reads only the current installed revision,
up to 64 KiB/file. It is read-only, uses the existing safe-path policy, refuses the
retained package archive and does not expose another revision/app or general SD
paths. Missing/unreadable resources return nil. Resource strings count against the
Lua allocator. The older `cyd.storage` API and private SD data paths are unchanged.

With `credentials` permission, use `cyd.credentials.set(key,bytes)`, `.get(key)` and
`.delete(key)`. Get returns nil when missing; mutations return success booleans or
raise bounded NVS/quota/record errors. Keys are 1–32 lowercase letters/digits/_/-;
values ≤256 binary bytes. Four keys per identity, eight identities device-wide.
Deleting the last key releases its identity slot. Records carry and compare the
full package ID, avoiding truncated-hash identity aliases. No namespace selector
or arbitrary NVS access is exposed.

`cyd.credentials.encrypted_at_rest()` returns false for this platform: records are
outside the removable SD, but this upgrade does **not** supply NVS/flash encryption,
change partitions or program eFuses. Credentials also exist in RAM/Lua strings.
Installing different code with the same package ID grants that code the same app
identity; packages are not publisher-signed. See SECURITY-0.4.md.

Lua retains its 1 MiB allocator, instruction hooks, 500 ms entry/callback budget
and cancellation. C-call nesting is capped at 32. Application __gc/__close are
rejected because Lua disables hooks for finalizers; user metatables are copied
and protected against late finalizer injection. Ordinary __index-based object
patterns work. IO/package/debug/coroutine/load/pcall APIs remain unavailable.
Native SDK/filesystem/C-library calls are not arbitrary preemptible processes;
this remains a constrained app runtime, not a hostile-code security boundary.
