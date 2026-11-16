-- Shared source prelude, concatenated into each package at build time.
-- No runtime loader, background task, network access, or executable data files.
local kit = {}
local root = cyd.ui.create_screen()
local screenW,screenH = cyd.ui.size()
local scale=screenW>=400 and 1.5 or 1
local W,H=math.floor(screenW/scale),math.floor(screenH/scale)
local function px(n)return math.floor(n*scale+0.5)end
if not cyd.time.now or not cyd.documents or not cyd.ui.text_lines then
    root:add_label{text="Update CYD-OS first: C5 0.4.4c or S3 0.4.3s.",x=4,y=8,width=W-8,font=20}
    cyd.ui.show(root)
    return
end
local view, status, status_text
kit.w,kit.h,kit.y,kit.body = W,H,28,H-72
kit.scale=scale
kit.page,kit.mode = 1,""
local colors={ink=0xf0f4fa,muted=0x9eacbf,accent=0x70d8bd,bg=0x101722,button=0x253449}
root:set_background(colors.bg)
function kit.cut(s,n)
    s=tostring(s or "")
    if #s<=n then return s end
    local out=""
    for c in s:gmatch("[%z\1-\127\194-\244][\128-\191]*") do
        if #out+#c>n then break end
        out=out..c
    end
    return out
end
function kit.label(text,x,y,w,h,font,color)
    font=font or 14
    if scale>1 then font=font==14 and 20 or 28 end
    return view:add_label{text=kit.cut(text,1024),x=px(x or 4),y=px(y or kit.y),width=px(w or W-8),
        height=px(h or kit.body),font=font,color=color or colors.ink}
end
function kit.button(text,x,y,w,h,fn)
    return view:add_button{text=text,x=px(x),y=px(y),width=math.max(1,px(w)),height=math.max(1,px(h)),font=scale>1 and 20 or 14,
        background=colors.button,padding=0,border=0,radius=5,on_click=fn}
end
function kit.notice(s)
    s=kit.cut(s or "",100)
    if status and status_text~=s then status:set_text(s);status_text=s end
end
function kit.update_text(handle,text)
    if handle:get_text()~=text then handle:set_text(text) end
end
function kit.begin(title,action,fn)
    if view then view:delete() end
    view=root:add_view{x=0,y=0,width=screenW,height=screenH,scroll=false,padding=0,border=0,radius=0}
    view:add_rectangle{x=0,y=0,width=screenW,height=screenH,background=colors.bg,padding=0,border=0,radius=0}
    kit.label(title,4,1,W-(action and 84 or 8),24,20)
    if action then kit.button(action,W-78,0,74,24,fn) end
    status=kit.label("",4,H-43,W-8,17,14,colors.muted)
    status_text=""
    cyd.ui.show(view)
end
function kit.footer(left,lf,middle,mf,right,rf)
    local width=math.floor((W-16)/3)
    for i,v in ipairs({{left,lf},{middle,mf},{right,rf}}) do
        if v[1] and v[2] then kit.button(v[1],4+(i-1)*(width+4),H-24,width,23,v[2]) end
    end
end
function kit.input(value,label)
    kit.label(label,4,kit.y,W-8,17)
    return view:add_input{x=px(4),y=px(kit.y+18),width=px(W-8),height=px(28),padding=2,font=scale>1 and 20 or 14,text=kit.cut(value,240)}
end
function kit.show_text(title,text,back,action,fn,page)
    local rows=math.max(1,math.floor(kit.body/18))
    local lines,truncated=cyd.ui.text_lines(tostring(text),px(W-8),scale>1 and 20 or 14)
    local chunks,part,bytes={},{},0
    local function add(line)
        if #part>0 and (#part>=rows or bytes+#line+1>1000) then chunks[#chunks+1]=table.concat(part,"\n");part={};bytes=0 end
        if #part>0 then bytes=bytes+1 end
        part[#part+1]=line;bytes=bytes+#line
    end
    for _,line in ipairs(lines) do
        while #line>1000 do local prefix=kit.cut(line,1000);add(prefix);line=line:sub(#prefix+1) end
        add(line)
    end
    if #part>0 then chunks[#chunks+1]=table.concat(part,"\n") end
    local function draw(index)
        index=math.max(1,math.min(index,#chunks));kit.begin(title,action,fn)
        kit.label(chunks[index]);kit.notice(index.." / "..#chunks..(truncated and " | first 512 lines" or ""))
        kit.footer("Back",back,index>1 and "Previous" or nil,function()draw(index-1)end,
            index<#chunks and "Next" or nil,function()draw(index+1)end)
    end
    draw(page or 1)
end
function kit.list(title,items,open,newlabel,newfn,back,page)
    local rows=math.max(1,math.floor(kit.body/27));local pages=math.max(1,math.ceil(#items/rows))
    page=math.max(1,math.min(page or 1,pages));kit.begin(title,newlabel,newfn)
    if #items==0 then kit.label("Nothing here yet.") end
    for n=(page-1)*rows+1,math.min(page*rows,#items) do
        local item=items[n]
        kit.button(kit.cut(item.label,50),4,kit.y+(n-1-(page-1)*rows)*27,W-8,25,function()open(item)end)
    end
    kit.notice(#items.." items | "..page.." / "..pages)
    kit.footer(back and "Back",back,page>1 and "Previous" or nil,function()kit.list(title,items,open,newlabel,newfn,back,page-1)end,
        page<pages and "Next" or nil,function()kit.list(title,items,open,newlabel,newfn,back,page+1)end)
end
function kit.confirm(title,text,yes,no)
    kit.begin(title);kit.label(text)
    kit.footer("Cancel",no,nil,nil,"Delete",yes)
end
function kit.quote(s)
    return '"'..s:gsub('[%z\1-\31\\"]',function(c)
        if c=='"' then return '\\"' elseif c=='\\' then return '\\\\' end
        return string.format('\\u%04x',c:byte())
    end)..'"'
end
function kit.json(value)
    local t=type(value)
    if t=="string" then return kit.quote(value)
    elseif t=="number" then assert(value==value and math.abs(value)~=math.huge);return tostring(value)
    elseif t=="boolean" then return value and "true" or "false"
    elseif t=="table" then
        local values={}
        if value[1]~=nil or next(value)==nil then
            for _,v in ipairs(value) do values[#values+1]=kit.json(v) end
            return '['..table.concat(values,',')..']'
        end
        local keys={};for k in pairs(value) do keys[#keys+1]=k end;table.sort(keys)
        for _,k in ipairs(keys) do values[#values+1]=kit.quote(k)..':'..kit.json(value[k]) end
        return '{'..table.concat(values,',')..'}'
    end
    error("Unsupported saved value")
end
function kit.store(kind,default,validate)
    local best={seq=0,data=default}
    for _,name in ipairs({"state-a.json","state-b.json"}) do
        local raw=cyd.storage.read(name)
        local v=raw and #raw<=7800 and cyd.json.decode(raw)
        if type(v)=="table" and v.kind==kind and v.format==1 and type(v.seq)=="number"
            and v.seq%1==0 and v.seq>best.seq and v.seq<1000000000 and validate(v.data) then best=v end
    end
    local store={data=best.data,seq=best.seq}
    function store:save(value)
        local seq=self.seq+1
        local raw=kit.json{kind=kind,format=1,seq=seq,data=value}
        if #raw>7800 then return nil,"Saved-data capacity reached" end
        cyd.storage.write(seq%2==1 and "state-a.json" or "state-b.json",raw)
        self.data,self.seq=value,seq
        return true
    end
    return store
end
function kit.filename(name)
    return type(name)=="string" and #name>0 and #name<=48 and name:match('^[a-z0-9_%-][a-z0-9_.%-]*$') and name:sub(-1)~='.'
end
function kit.is_text(s)
    local i=1
    while i<=#s do
        local a=s:byte(i);local n=1
        if a<32 and a~=9 and a~=10 and a~=13 then return false end
        if a>=128 then
            if a>=194 and a<=223 then n=2 elseif a>=224 and a<=239 then n=3
            elseif a>=240 and a<=244 then n=4 else return false end
            if i+n-1>#s then return false end
            local b=s:byte(i+1)
            if (a==224 and b<160) or (a==237 and b>159) or (a==240 and b<144) or (a==244 and b>143) then return false end
            for j=1,n-1 do local c=s:byte(i+j);if c<128 or c>191 then return false end end
        end
        i=i+n
    end
    return true
end
function kit.export(name,bytes)
    local ok,err=cyd.documents.write(name,bytes,false)
    kit.notice(ok and ("Exported: "..name) or err)
end
function kit.leap(y)return y%4==0 and (y%100~=0 or y%400==0)end
function kit.days(y,m)return ({31,kit.leap(y) and 29 or 28,31,30,31,30,31,31,30,31,30,31})[m]end
function kit.date_valid(s)
    if type(s)~="string" then return false end
    local y,m,d=s:match('^(%d%d%d%d)%-(%d%d)%-(%d%d)$');y,m,d=tonumber(y),tonumber(m),tonumber(d)
    return y and y>=2024 and y<=2099 and m>=1 and m<=12 and d>=1 and d<=kit.days(y,m)
end
function kit.records(o)
    local function valid(data)
        if type(data)~="table" or #data>o.limit then return false end
        local count,ids=0,{}
        for k in pairs(data) do
            if type(k)~="number" or k%1~=0 or k<1 or k>#data then return false end
            count=count+1
        end
        if count~=#data then return false end
        for _,r in ipairs(data) do
            if type(r)~="table" or type(r.id)~="number" or r.id%1~=0 or r.id<1 or r.id>1000000000 or ids[r.id] then return false end
            ids[r.id]=true
            for _,f in ipairs(o.fields) do if type(r[f.key])~="string" or #r[f.key]>f.max then return false end end
            if o.validate and not o.validate(r) then return false end
        end
        return true
    end
    local store=kit.store(o.kind,{},valid)
    local list,detail,edit
    local function save_record(draft)
        if draft[o.fields[1].key]=="" then kit.notice("First field cannot be empty");return end
        if o.validate and not o.validate(draft) then kit.notice(o.invalid or "Check the entered values");return end
        local data={};local found=false
        for _,r in ipairs(store.data) do
            if r.id==draft.id then data[#data+1]=draft;found=true else data[#data+1]=r end
        end
        if not found then
            if #data>=o.limit then kit.notice("Limit: "..o.limit.." entries");return end
            data[#data+1]=draft
        end
        local ok,err=store:save(data);if ok then list() else kit.notice(err) end
    end
    function edit(original,index,draft)
        index=index or 1
        if not draft then
            draft={id=original and original.id or store.seq+1}
            for _,r in ipairs(store.data) do draft.id=math.max(draft.id,r.id+1) end
            if original then draft.id=original.id end
            for _,f in ipairs(o.fields) do draft[f.key]=original and original[f.key] or (o.defaults and o.defaults[f.key] or "") end
        end
        local f=o.fields[index];local field
        local function keep()draft[f.key]=kit.cut(field:get_text(),f.max)end
        kit.begin(o.title,index==#o.fields and "Save" or nil,function()keep();save_record(draft)end)
        field=kit.input(draft[f.key],f.label.." ("..f.max.." bytes)")
        kit.notice(index.." / "..#o.fields.." | Back discards")
        kit.footer("Back",list,index>1 and "Previous" or nil,function()keep();edit(original,index-1,draft)end,
            index<#o.fields and "Next" or nil,function()keep();edit(original,index+1,draft)end)
    end
    function detail(r)
        local text={}
        for _,f in ipairs(o.fields) do text[#text+1]=f.label..": "..r[f.key] end
        local function menu()
            kit.list("Entry actions",{{label="Edit",key=1},{label="Export to Documents",key=2},{label="Delete",key=3}},function(item)
                if item.key==1 then edit(r)
                elseif item.key==2 then
                    detail(r);kit.export(o.kind..'-'..r.id..'-'..store.seq..'.json',kit.json(r))
                else kit.confirm("Delete entry?","This removes the saved entry.",function()
                    local data={};for _,v in ipairs(store.data) do if v.id~=r.id then data[#data+1]=v end end
                    local ok,err=store:save(data);if ok then list() else kit.notice(err) end
                end,function()detail(r)end) end
            end,nil,nil,function()detail(r)end)
        end
        kit.show_text(o.title,table.concat(text,"\n"),list,"Actions",menu)
    end
    function list()
        local items={}
        for _,r in ipairs(store.data) do if not o.filter or o.filter(r) then
            items[#items+1]={label=r[o.label or o.fields[1].key],record=r}
        end end
        table.sort(items,function(a,b)return a.label:lower()<b.label:lower()end)
        kit.list(o.title,items,function(item)detail(item.record)end,"New",function()edit()end,o.back)
    end
    list()
    return {store=store,list=list,edit=edit}
end
