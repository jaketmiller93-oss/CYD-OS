-- Generic platform exercise. Network traffic begins only when its button is tapped.
local root = cyd.ui.create_screen()
local width, height = cyd.ui.size()
local page = root:add_view{ x=0, y=0, width=width, height=height, scroll=true, layout="column" }
page:add_label{ text="Platform API demo", x=0, y=0, width=width-24, font=20 }
local status = page:add_label{ text="Waiting", width=width-24, color=0x30d158 }
local progress = page:add_bar{ width=width-24, height=16, value=0, color=0x30d158 }
local ticks = 0
cyd.timer.every(1000, function()
    ticks=ticks+1
    progress:set_value(ticks % 101)
    local wifi=cyd.network.info()
    status:set_text(wifi.station_connected and ("Station "..wifi.station_ip) or (wifi.ap_enabled and "Local AP; station offline" or "Offline"))
end)
local decoded=cyd.json.decode(cyd.resources.read("sample.json"))
page:add_label{ text=decoded.message, width=width-24 }
page:add_image{ resource="icon.rgb565" }
local url=page:add_input{ text="https://example.com/", width=width-24 }
local response=page:add_label{ text="HTTP result", width=width-24 }
page:add_button{ text="HTTP GET", on_click=function()
    local request,error=cyd.network.request({url=url:get_text(),timeout_ms=5000},function(result)
        response:set_text(result.ok and ("HTTP "..result.status.." / "..#result.body.." bytes") or result.error)
    end)
    if not request then response:set_text(error) end
end }
local secret=page:add_input{ text="", masked=true, width=width-24 }
page:add_button{ text="Save app secret", on_click=function()
    cyd.credentials.set("demo_secret",secret:get_text())
    response:set_text("App secret saved; NVS is not encrypted by this build")
    secret:set_text("")
end }
local broker=page:add_input{ text="mqtt://192.0.2.1:1883", width=width-24 }
local client
page:add_button{ text="Connect MQTT", on_click=function()
    if client then client:disconnect() end
    client=cyd.mqtt.connect{url=broker:get_text(),on_state=function(state,error)response:set_text(state.." "..error)end,on_message=function(topic,body)response:set_text(topic.." / "..#body.." bytes")end}
    client:subscribe("demo/status/#")
end }
page:add_button{ text="Disconnect MQTT", on_click=function()if client then client:disconnect()end end }
cyd.ui.show(page)
