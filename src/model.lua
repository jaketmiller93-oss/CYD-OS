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
