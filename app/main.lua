-- Pocket Pal 0.1.0; generated from src/model.lua and src/ui.lua.
-- Pure care model; bundled into main.lua by tools/build.py.
local Pet = {}
local stat_keys = {"food", "joy", "clean", "energy", "health"}
local function integer(n, low, high)
    return type(n) == "number" and n == math.floor(n) and n >= low and n <= high
end
local function clamp(n) return math.max(0, math.min(100, n)) end
function Pet.name(text)
    if type(text) ~= "string" then return nil end
    text = text:match("^%s*(.-)%s*$")
    if #text < 1 or #text > 12 or text:find("[^%w %-]") then return nil end
    return text
end
function Pet.new()
    return {version=1, sequence=0, name="Mochi", food=80, joy=75, clean=85,
        energy=90, health=100, sleeping=false, age=0, remainder=0, care=0}
end
local function canonical(s)
    return string.format("%d|%s|%d|%d|%d|%d|%d|%d|%d|%d|%d",
        s.sequence, s.name, s.food, s.joy, s.clean, s.energy, s.health,
        s.sleeping and 1 or 0, s.age, s.remainder, s.care)
end
function Pet.checksum(s)
    local sum, bytes = 7, canonical(s)
    for i=1,#bytes do sum=(sum*31+bytes:byte(i)) % 2147483647 end
    return sum
end
function Pet.valid(s)
    if type(s) ~= "table" or s.version ~= 1 or type(s.name) ~= "string" or Pet.name(s.name) ~= s.name
        or not integer(s.sequence, 0, 1000000000)
        or type(s.sleeping) ~= "boolean" or not integer(s.age, 0, 1000000000)
        or not integer(s.remainder, 0, 59) or not integer(s.care, 0, 1000000000)
        or not integer(s.checksum, 0, 2147483646) then return false end
    for _,key in ipairs(stat_keys) do if not integer(s[key],0,100) then return false end end
    return s.checksum == Pet.checksum(s)
end
function Pet.restore(a, b)
    local av, bv = Pet.valid(a), Pet.valid(b)
    if av and (not bv or a.sequence >= b.sequence) then return a, "saved" end
    if bv then return b, "saved" end
    return Pet.new(), (a ~= nil or b ~= nil) and "recovered" or "new"
end
function Pet.encode(s)
    -- Names contain only bounded ASCII letters, digits, spaces and hyphens.
    return string.format('{"version":1,"sequence":%d,"name":"%s","food":%d,"joy":%d,"clean":%d,"energy":%d,"health":%d,"sleeping":%s,"age":%d,"remainder":%d,"care":%d,"checksum":%d}',
        s.sequence,s.name,s.food,s.joy,s.clean,s.energy,s.health,
        s.sleeping and "true" or "false",s.age,s.remainder,s.care,Pet.checksum(s))
end
function Pet.advance(s, seconds)
    seconds = math.max(0, math.min(300, math.floor(seconds)))
    s.age = math.min(1000000000, s.age + seconds)
    local total = s.remainder + seconds
    s.remainder = total % 60
    for _=1,math.floor(total/60) do
        s.food=clamp(s.food-(s.sleeping and 1 or 2))
        s.joy=clamp(s.joy-(s.sleeping and 1 or 2))
        s.clean=clamp(s.clean-1)
        s.energy=clamp(s.energy+(s.sleeping and 6 or -2))
        if s.energy == 0 then s.sleeping=true end
        local critical=0
        for _,key in ipairs({"food","joy","clean","energy"}) do
            if s[key]<20 then critical=critical+1 end
        end
        if critical>0 then s.health=clamp(s.health-critical*2)
        elseif math.min(s.food,s.joy,s.clean,s.energy)>=40 then s.health=clamp(s.health+1) end
    end
end
function Pet.stage(s)
    if s.age<1800 then return "Baby" end
    if s.age<7200 then return "Buddy" end
    return "Grown-up"
end
function Pet.mood(s)
    if s.sleeping then return "sleep", "Dreaming..." end
    if s.health<35 then return "sick", "Needs gentle care" end
    if s.food<25 then return "hungry", "A snack, please!" end
    if s.energy<25 then return "tired", "Getting sleepy" end
    if s.clean<25 then return "dirty", "Bath time?" end
    if s.joy<25 then return "bored", "Let's play!" end
    return "happy", "Happy to see you"
end
function Pet.act(s, action)
    if action=="sleep" then
        s.sleeping=not s.sleeping
        return true, s.sleeping and "Sweet dreams" or "Good morning!"
    end
    if s.sleeping then return false, "Wake me up first" end
    if action=="feed" then
        if s.food>=95 then return false, "My tummy is full!" end
        s.food=clamp(s.food+24);s.joy=clamp(s.joy+2)
        s.clean=clamp(s.clean-3);s.energy=clamp(s.energy-1)
    elseif action=="wash" then
        if s.clean>=95 then return false, "Already squeaky clean" end
        s.clean=clamp(s.clean+35);s.joy=clamp(s.joy+3);s.energy=clamp(s.energy-1)
    elseif action=="play" then
        if s.energy<15 then return false, "A nap before play?" end
        if s.food<15 then return false, "A snack before play?" end
        return true, "Tap the star!"
    elseif action=="star" then
        s.joy=clamp(s.joy+6);s.energy=clamp(s.energy-2)
        s.food=clamp(s.food-1);s.clean=clamp(s.clean-1)
        return true, "Nice catch!"
    else return false, "Unknown action" end
    s.care=math.min(1000000000,s.care+1)
    return true, action=="feed" and "Yum! Thank you" or "Fresh and fluffy!"
end

-- CYDOS 0.4+ UI. No network, credentials, wall clock or background task required.
local root=cyd.ui.create_screen()
if not cyd.timer or not cyd.json or not cyd.ui.size then
    root:add_label{text="Pocket Pal needs CYDOS 0.4.0 or newer.",x=0,y=0,width=260}
    return
end
root:set_background(0x11141f)
local W,H=cyd.ui.size()
local WIDTH=math.min(W,360)
local X=math.floor((W-WIDTH)/2)
local INK,MUTED,MINT,PEACH=0xf4f1e9,0xa8b1c4,0x76e0b2,0xffc28d
local handles,face,mood_label,needs_label,rest_label,sleep_button,notice={}
local page,frame,last_sprite,last_save,flash_until="home",0,nil,0,0
local detail_rows,detail_age,detail_care
local function active_age(s)
    return s.age>=3600 and (math.floor(s.age/3600).."h") or (math.floor(s.age/60).."m")
end
local function read_slot(name)
    local bytes=cyd.storage.read(name)
    if not bytes then return nil end
    local parsed=cyd.json.decode(bytes)
    return parsed or false
end
local state,origin=Pet.restore(read_slot("pal-a.json"),read_slot("pal-b.json"))
local function save()
    state.sequence=math.min(1000000000,state.sequence+1)
    cyd.storage.write(state.sequence%2==0 and "pal-a.json" or "pal-b.json",Pet.encode(state))
    last_save=cyd.time.uptime_ms()
end
local function track(h) handles[#handles+1]=h;return h end
local function label(text,x,y,w,font,color)
    return track(root:add_label{text=text,x=X+x,y=y,width=w,font=font or 14,color=color or INK})
end
local function button(text,x,y,w,h,callback,color)
    return track(root:add_button{text=text,x=X+x,y=y,width=w,height=h,
        font=14,color=INK,background=color or 0x29334a,on_click=callback})
end
local function clear()
    -- Delete top-level widgets; only one image is live at any time.
    if face then face:delete();face=nil end
    for _,h in ipairs(handles) do h:delete() end
    handles={};last_sprite=nil;detail_rows=nil
end
local function say(text)
    notice=text;flash_until=cyd.time.uptime_ms()+3500
    if page=="home" and mood_label then mood_label:set_text(text) end
end
local show_home,show_details,show_name,show_game
local function draw_face()
    local mood=Pet.mood(state)
    local sprite=mood
    if mood=="happy" then
        sprite=frame%8==7 and "blink" or (frame%4<2 and "happy" or "happy2")
    end
    if sprite~=last_sprite then
        if face then face:delete() end
        face=root:add_image{resource="sprites/"..sprite..".rgb565",x=X+2,y=25}
        last_sprite=sprite
    end
end
local function refresh_home()
    local _,mood=Pet.mood(state)
    mood_label:set_text(cyd.time.uptime_ms()<flash_until and notice or mood)
    mood_label:set_color(state.health<35 and PEACH or MINT)
    needs_label:set_text(string.format("Food %d   Joy %d",state.food,state.joy))
    rest_label:set_text(string.format("Rest %d   Clean %d",state.energy,state.clean))
    sleep_button:set_text(state.sleeping and "Wake" or "Sleep")
    draw_face()
end
local function care(action)
    local ok,text=Pet.act(state,action)
    if ok then save() end
    say(text);refresh_home()
end
show_home=function()
    clear();page="home"
    label(WIDTH<260 and #state.name>8 and (state.name:sub(1,8).."..") or state.name,0,0,WIDTH-64,WIDTH<260 and 14 or 20)
    button("More",WIDTH-58,0,58,26,show_details)
    mood_label=label("",90,29,WIDTH-90,14,MINT)
    needs_label=label("",90,55,WIDTH-90,14,MUTED)
    rest_label=label("",90,75,WIDTH-90,14,MUTED)
    local cols=WIDTH<260 and 2 or 4
    local bw=math.floor((WIDTH-(cols-1)*6)/cols)
    local rows=cols==2 and 2 or 1
    local by=math.max(94,H-rows*34-(rows-1)*6)
    local function action(i,text,fn,color)
        return button(text,((i-1)%cols)*(bw+6),by+math.floor((i-1)/cols)*40,bw,34,fn,color)
    end
    action(1,"Feed",function()care("feed")end,0x574033)
    action(2,"Play",show_game,0x2c4b47)
    action(3,"Wash",function()care("wash")end,0x304566)
    sleep_button=action(4,"Sleep",function()care("sleep")end,0x443650)
    refresh_home()
end
show_name=function()
    clear();page="name"
    label("Name your pal",0,0,WIDTH-64,WIDTH<260 and 14 or 20)
    button("Back",WIDTH-58,0,58,26,show_details)
    local help=label("Up to 12 characters",0,32,WIDTH,14,MUTED)
    local field=track(root:add_input{x=X,y=53,width=WIDTH,height=30,text=state.name})
    button("Save name",0,90,WIDTH,34,function()
        local name=Pet.name(field:get_text())
        if not name then help:set_text("Letters, digits, spaces or -");return end
        state.name=name;save();show_home();say("Nice to meet you!")
    end)
end
show_details=function()
    clear();page="details"
    detail_age=label(Pet.stage(state).." / "..active_age(state),0,0,WIDTH-64,WIDTH<260 and 14 or 20)
    button("Back",WIDTH-58,0,58,26,show_home)
    local items={{"Food","food",0xffc28d},{"Joy","joy",0xff9bb4},
        {"Clean","clean",0x81baff},{"Rest","energy",0xc0a5ff}}
    detail_rows={}
    local half=math.floor((WIDTH-12)/2)
    for i,item in ipairs(items) do
        local y=32+math.floor((i-1)/2)*34
        local x=((i-1)%2)*(half+12)
        local caption=label(item[1].."  "..state[item[2]].." / 100",x,y,half,14,item[3])
        local meter=track(root:add_bar{x=X+x,y=y+20,width=half,height=8,value=state[item[2]],color=item[3]})
        detail_rows[i]={key=item[2],name=item[1],caption=caption,meter=meter}
    end
    detail_care=label("Health "..state.health.." / Care "..state.care,0,103,WIDTH-88,14,MUTED)
    button("Name",WIDTH-82,96,82,28,show_name)
end
local stars,target,game_label
show_game=function()
    local ok,text=Pet.act(state,"play")
    if not ok then say(text);refresh_home();return end
    clear();page="game";stars=0
    label("Star catch",0,0,WIDTH-64,20)
    button("Back",WIDTH-58,0,58,26,show_home)
    game_label=label("Tap * three times!",0,35,WIDTH,14,MINT)
    local buttons={}
    local bw=math.floor((WIDTH-12)/3)
    local function move()
        target=math.random(1,3)
        for i,b in ipairs(buttons) do b:set_text(i==target and "*" or "o") end
    end
    for i=1,3 do
        local choice=i
        buttons[i]=button("o",(i-1)*(bw+6),67,bw,42,function()
            if choice~=target then game_label:set_text("Try the * !  "..stars.." / 3");return end
            local accepted,feedback=Pet.act(state,"star")
            if not accepted then show_home();say(feedback);return end
            stars=stars+1
            if stars==3 then
                state.care=math.min(1000000000,state.care+1);save();show_home();say("Three stars! That was fun")
            else save();game_label:set_text("Nice!  "..stars.." / 3");move() end
        end,0x324a55)
    end
    move()
end
-- Randomness is only cosmetic/game placement; no security use.
math.randomseed(cyd.time.uptime_ms()+state.age+state.sequence)
if origin=="recovered" then notice="New start: save unreadable"
elseif origin=="new" then notice="Hello! I'm "..state.name
else notice="Welcome back!" end
flash_until=cyd.time.uptime_ms()+3500
save();show_home()
local last_tick=cyd.time.uptime_ms()
cyd.timer.every(1000,function()
    local now=cyd.time.uptime_ms()
    local seconds=math.floor(math.max(0,now-last_tick)/1000)
    last_tick=last_tick+seconds*1000
    Pet.advance(state,seconds)
    frame=frame+1
    if now-last_save>=30000 then save() end
    if page=="game" and state.sleeping then show_home();say("Time for a little nap") end
    if page=="home" then refresh_home()
    elseif page=="details" then
        for _,row in ipairs(detail_rows) do
            row.caption:set_text(row.name.."  "..state[row.key].." / 100")
            row.meter:set_value(state[row.key])
        end
        detail_age:set_text(Pet.stage(state).." / "..active_age(state))
        detail_care:set_text("Health "..state.health.." / Care "..state.care)
    end
end)
