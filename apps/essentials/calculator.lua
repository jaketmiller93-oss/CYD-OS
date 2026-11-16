-- Offline, immediate-execution calculator for CYD-OS 0.4.x.
-- All arithmetic stays in this app; no network, storage or credential access.
local entry, accumulator, pending = "0", nil, nil
local waiting, result_ready = false, false
local repeat_operator, repeat_operand, failure = nil, nil, nil
local function clear()
    entry, accumulator, pending = "0", nil, nil
    waiting, result_ready = false, false
    repeat_operator, repeat_operand, failure = nil, nil, nil
end
local function number_text(n)
    if n == 0 then return "0" end
    return string.format("%.12g", n)
end
local function calculate(a, operator, b)
    local n
    if operator == "+" then n = a + b
    elseif operator == "-" then n = a - b
    elseif operator == "*" then n = a * b
    elseif b == 0 then failure = "Divide by 0"
    else n = a / b end
    if n and (n ~= n or n == math.huge or n == -math.huge) then
        failure = "Overflow"
    end
    if failure then
        entry, accumulator, pending = "0", nil, nil
        repeat_operator, repeat_operand = nil, nil
        waiting, result_ready = false, false
        return nil
    end
    return tonumber(number_text(n))
end
local function press(key)
    if key == "AC" then clear(); return end
    if failure then
        if key:match("^%d$") or key == "." or key == "CE" then clear()
        else return end
    end
    if key:match("^%d$") or key == "." then
        if result_ready then clear() end
        if waiting then entry, waiting = "0", false end
        repeat_operator, repeat_operand = nil, nil
        if key == "." then
            if not entry:find("%.") then entry = entry .. "." end
        else
            local digits = entry:gsub("[^0-9]", "")
            if #digits >= 12 then return end
            if entry == "0" then entry = key
            elseif entry == "-0" then entry = "-" .. key
            else entry = entry .. key end
        end
    elseif key == "CE" then
        entry, waiting, result_ready = "0", false, false
        repeat_operator, repeat_operand = nil, nil
    elseif key == "BS" then
        if waiting then return end
        if entry:find("[eE]") then entry = "0" else entry = entry:sub(1, -2) end
        if entry == "" or entry == "-" then entry = "0" end
        result_ready, repeat_operator, repeat_operand = false, nil, nil
    elseif key == "+/-" then
        if waiting then entry, waiting = "0", false end
        if entry:sub(1, 1) == "-" then entry = entry:sub(2) else entry = "-" .. entry end
        result_ready, repeat_operator, repeat_operand = false, nil, nil
    elseif key == "=" then
        if pending then
            local rhs = waiting and accumulator or tonumber(entry)
            local operator = pending
            local answer = calculate(accumulator, operator, rhs)
            if not answer then return end
            entry = number_text(answer)
            repeat_operator, repeat_operand = operator, rhs
            accumulator, pending = nil, nil
        elseif result_ready and repeat_operator then
            local answer = calculate(tonumber(entry), repeat_operator, repeat_operand)
            if not answer then return end
            entry = number_text(answer)
        end
        waiting, result_ready = false, true
    else
        local value = tonumber(entry)
        if pending and not waiting then
            value = calculate(accumulator, pending, value)
            if not value then return end
            entry = number_text(value)
        end
        accumulator, pending, waiting, result_ready = value, key, true, false
        repeat_operator, repeat_operand = nil, nil
    end
end

-- Use the available app area: OS Home/Stop remains visible and functional.
local root = cyd.ui.create_screen()
local width, height = cyd.ui.size()
root:set_background(0x101216)
local compact = height < 220 and width > 250
local value_x, value_y, value_width, value_font
local status_x, status_y, status_width
local grid_x, grid_y, grid_width, grid_height
if compact then
    local left = math.max(88, math.floor(width * 0.32))
    value_x, value_y, value_width, value_font = 0, 43, left, 14
    status_x, status_y, status_width = 0, 23, left
    grid_x, grid_y, grid_width, grid_height = left + 8, 0, width - left - 8, height
else
    local area = math.min(width, 600)
    local origin = math.floor((width - area) / 2)
    value_font = height >= 280 and 28 or 20
    value_x, value_y, value_width = origin, 36, area
    status_x, status_y, status_width = origin, 18, area
    grid_x, grid_y, grid_width, grid_height = origin, 74, area, height - 74
end
root:add_label{text="Calculator", x=value_x, y=0, width=value_width, font=14, color=0xaab2bf}
local status = root:add_label{text="Ready", x=status_x, y=status_y, width=status_width, font=14, color=0xffad42}
local display = root:add_label{text="0", x=value_x, y=value_y, width=value_width, font=value_font, color=0xf5f7fa}
if compact then
    root:add_label{text="Offline", x=0, y=height-18, width=value_width, font=14, color=0x778292}
end
local function repaint()
    local shown = entry
    if compact and #shown > 10 then shown = string.format("%.4g", tonumber(entry)) end
    display:set_text(shown)
    display:set_color(failure and 0xff6b6b or 0xf5f7fa)
    if failure then status:set_text(failure)
    elseif pending then status:set_text(pending .. (waiting and " pending" or ""))
    elseif compact and shown ~= entry then status:set_text("Rounded view")
    else status:set_text(result_ready and "Result" or "Ready") end
end
local keys = {"7","8","9","/","AC", "4","5","6","*","BS",
              "1","2","3","-","+/-", "0",".","=","+","CE"}
local gap = compact and 3 or 5
local cell_width = math.floor((grid_width - gap*4)/5)
local cell_height = math.floor((grid_height - gap*3)/4)
local font = cell_height >= 42 and cell_width >= 58 and 28 or 14
for index, key in ipairs(keys) do
    local col, row = (index-1)%5, math.floor((index-1)/5)
    local operator = key == "+" or key == "-" or key == "*" or key == "/"
    local bg = key == "=" and 0x237b55 or operator and 0x975616 or
               (key == "AC" or key == "CE" or key == "BS" or key == "+/-") and 0x39414e or 0x242b35
    root:add_button{text=key, x=grid_x+col*(cell_width+gap), y=grid_y+row*(cell_height+gap),
                    width=cell_width, height=cell_height, font=font, background=bg,
                    on_click=function() press(key); repaint() end}
end
cyd.ui.show(root)
