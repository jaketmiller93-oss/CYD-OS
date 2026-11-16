local mode="menu"
local timer_end,timer_left,timer_running=nil,300000,false
local timer_duration=300000
local stopwatch_start,stopwatch_total,stopwatch_running=nil,0,false
local alert=""
local valid_config=function(d)return type(d)=="table" and type(d.alarm)=="string" and #d.alarm<=5 and (d.last_day==nil or (type(d.last_day)=="string" and #d.last_day<=10)) end
local config=kit.store("clock",{alarm="",last_day=""},valid_config)
local main,clock_page,timer_page,stopwatch_page,alarm_page
local output,sub,alarm_field
local function duration(ms)
    local n=math.max(0,math.floor(ms/1000));return string.format('%02d:%02d:%02d',math.floor(n/3600),math.floor(n/60)%60,n%60)
end
local function update()
    local now=cyd.time.uptime_ms()
    if timer_running and now>=timer_end then
        timer_running=false;timer_left=0;alert="TIMER FINISHED"
        if mode=="timer" then timer_page();return end
    end
    local t=cyd.time.now()
    local date=t.valid and string.format('%04d-%02d-%02d',t.year,t.month,t.day) or ""
    if t.valid and config.data.alarm~="" and string.format('%02d:%02d',t.hour,t.minute)==config.data.alarm and config.data.last_day~=date then
        config:save{alarm=config.data.alarm,last_day=date};alert="ALARM: "..config.data.alarm
    end
    if mode=="clock" and output then
        local h=t.hour or 0;local suffix=""
        if t.clock_12h then suffix=h>=12 and " PM" or " AM";h=h%12;if h==0 then h=12 end end
        kit.update_text(output,t.valid and string.format('%02d:%02d:%02d%s',h,t.minute,t.second,suffix) or "Clock unset")
        kit.update_text(sub,t.valid and date or "Set/sync time in OS Settings")
    elseif mode=="timer" and output then kit.update_text(output,duration(timer_running and timer_end-now or timer_left))
    elseif mode=="stopwatch" and output then kit.update_text(output,duration(stopwatch_total+(stopwatch_running and now-stopwatch_start or 0))) end
    if mode=="clock" or mode=="timer" or mode=="stopwatch" then
        kit.notice(alert~="" and alert or "Alerts only while open")
    elseif mode=="menu" and alert~="" then kit.notice(alert) end
end
function main()
    mode="menu";output=nil
    kit.list("Clock",{{label="Clock",fn=clock_page},{label="Countdown timer",fn=timer_page},
        {label="Stopwatch",fn=stopwatch_page},{label="Daily alarm (while open)",fn=alarm_page}},function(item)item.fn()end)
    update()
end
function clock_page()
    mode="clock";kit.begin("Clock")
    output=kit.label("",4,kit.y,kit.w-8,30,28);sub=kit.label("",4,kit.y+31,kit.w-8,18)
    kit.footer("Back",main,"Dismiss",function()alert="";update()end)
    update()
end
function timer_page()
    mode="timer";kit.begin("Timer")
    output=kit.label("",4,kit.y,kit.w-8,32,28)
    kit.button("Set seconds",4,kit.y+31,kit.w-8,21,function()
        mode="set_timer";output=nil;kit.begin("Timer seconds")
        local field=kit.input(tostring(math.floor(timer_left/1000)),"1 to 86400 seconds")
        kit.footer("Back",timer_page,nil,nil,"Set",function()
            local n=tonumber(field:get_text())
            if not n or n%1~=0 or n<1 or n>86400 then kit.notice("Enter 1-86400 whole seconds");return end
            timer_running=false;timer_duration=n*1000;timer_left=timer_duration;alert="";timer_page()
        end)
    end)
    kit.footer("Back",main,timer_running and "Pause" or (timer_left==0 and "Again" or "Start"),function()
        local now=cyd.time.uptime_ms()
        if timer_running then timer_left=math.max(0,timer_end-now);timer_running=false
        else if timer_left==0 then timer_left=timer_duration end;timer_end=now+timer_left;timer_running=true;alert="" end
        timer_page()
    end,"Reset",function()timer_running=false;timer_left=timer_duration;alert="";timer_page()end)
    update()
end
function stopwatch_page()
    mode="stopwatch";kit.begin("Stopwatch")
    output=kit.label("",4,kit.y,kit.w-8,32,28)
    kit.footer("Back",main,stopwatch_running and "Pause" or "Start",function()
        local now=cyd.time.uptime_ms()
        if stopwatch_running then stopwatch_total=stopwatch_total+now-stopwatch_start;stopwatch_running=false
        else stopwatch_start=now;stopwatch_running=true end
        stopwatch_page()
    end,"Reset",function()stopwatch_running=false;stopwatch_total=0;stopwatch_page()end)
    update()
end
function alarm_page()
    mode="alarm";output=nil;kit.begin("Daily alarm")
    alarm_field=kit.input(config.data.alarm,"HH:MM, 24-hour; blank turns off")
    kit.notice("Alerts only while open")
    kit.footer("Back",main,"Dismiss",function()alert="";update()end,"Save",function()
        local value=alarm_field:get_text();local h,m=value:match('^(%d%d):(%d%d)$')
        if value~="" and (not h or tonumber(h)>23 or tonumber(m)>59) then kit.notice("Enter HH:MM, or leave blank");return end
        local ok,e=config:save{alarm=value,last_day=""};if ok then main() else kit.notice(e) end
    end)
    update()
end
main()
cyd.timer.every(250,update)
