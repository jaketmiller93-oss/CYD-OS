local screen = cyd.ui.create_screen()
local count = tonumber(cyd.storage.read("count.txt") or "0") or 0
local label = screen:add_label({text = "Hello from CYD OS! Saved taps: " .. count, x = 20, y = 20})
screen:add_button({text = "Tap me", x = 20, y = 70, on_click = function()
    count = count + 1
    label:set_text("Button pressed " .. count .. " times")
    cyd.storage.write("count.txt", tostring(count))
    cyd.log.info("Tap " .. count)
end})
cyd.ui.show(screen)
print("Hello app started without resetting the ESP32")
