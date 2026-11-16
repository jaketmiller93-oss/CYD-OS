local today=cyd.time.now()
local year,month=today.year or 2024,today.month or 1
local first_week=0
local months={"January","February","March","April","May","June","July","August","September","October","November","December"}
local draw
local function weekday(y,m,d)
    local t={0,3,2,5,0,3,5,1,4,6,2,4};if m<3 then y=y-1 end
    return (y+math.floor(y/4)-math.floor(y/100)+math.floor(y/400)+t[m]+d)%7
end
local function events(date)
    kit.records{kind="events",title="Events",limit=24,label="title",back=draw,
        filter=date and function(r)return r.date==date end or nil,
        defaults={date=date or string.format('%04d-%02d-01',year,month)},
        fields={{key="date",label="Date YYYY-MM-DD",max=10},{key="title",label="Title",max=48},
            {key="time",label="Time HH:MM (optional)",max=5},{key="note",label="Note",max=160}},
        validate=function(r)
            if not kit.date_valid(r.date) or r.title=="" then return false end
            if r.time=="" then return true end
            local h,m=r.time:match('^(%d%d):(%d%d)$')
            return h and tonumber(h)<24 and tonumber(m)<60
        end,invalid="Use a valid date, title and HH:MM time"}
end
local function shift(n)
    month=month+n;if month<1 then month=12;year=year-1 elseif month>12 then month=1;year=year+1 end
    if year<2024 then year=2024;month=1 elseif year>2099 then year=2099;month=12 end
    first_week=0;draw()
end
function draw()
    today=cyd.time.now()
    kit.begin((kit.scale==1 and months[month]:sub(1,3) or months[month]).." "..year,"Events",function()events()end)
    local cw,rowh=math.floor((kit.w-8)/7),25
    local visible=math.min(6,math.max(1,math.floor((kit.body-16)/rowh)))
    local offset=weekday(year,month,1)
    local weeks=math.ceil((offset+kit.days(year,month))/7)
    first_week=math.min(first_week,math.max(0,weeks-visible))
    for c,name in ipairs({"Su","Mo","Tu","We","Th","Fr","Sa"}) do kit.label(name,4+(c-1)*cw,kit.y,cw,16) end
    for row=0,math.min(visible,weeks-first_week)-1 do for col=0,6 do
        local day=(first_week+row)*7+col-offset+1
        if day>0 and day<=kit.days(year,month) then
            local date=string.format('%04d-%02d-%02d',year,month,day)
            local text=today.valid and today.year==year and today.month==month and today.day==day and ('['..day..']') or tostring(day)
            kit.button(text,4+col*cw,kit.y+17+row*rowh,cw-2,rowh-2,function()events(date)end)
        end
    end end
    kit.notice((today.valid and "" or "OS clock unset | ").."weeks "..(first_week+1).."-"..math.min(first_week+visible,weeks).." / "..weeks)
    kit.footer("< Month",function()shift(-1)end,"Weeks",function()first_week=first_week+visible;if first_week>=weeks then first_week=0 end;draw()end,
        "Month >",function()shift(1)end)
end
draw()
