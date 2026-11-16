local browse,open,edit
local function named(title,initial,done,back)
    kit.begin(title)
    local field=kit.input(initial,"Lowercase filename (48 bytes)")
    kit.footer("Back",back,nil,nil,"Save",function()
        local name=field:get_text():lower()
        if not kit.filename(name) then kit.notice("Use a-z, 0-9, -, _ and .; no folders");return end
        done(name)
    end)
end
function edit(name,bytes,isnew)
    kit.begin(isnew and "New file" or "Edit file")
    local field=kit.input(bytes,"Text (up to 240 bytes)")
    kit.footer("Back",browse,nil,nil,"Save",function()
        local text=kit.cut(field:get_text(),240)
        local ok,err=cyd.documents.write(name,text,not isnew)
        if ok then browse();kit.notice("Saved "..name) else kit.notice(err) end
    end)
end
function open(name)
    local bytes,err=cyd.documents.read(name)
    if not bytes then browse();kit.notice(err);return end
    local textual=kit.is_text(bytes)
    local text=bytes
    if not textual then
        local hex={};for i=1,math.min(256,#bytes) do hex[#hex+1]=string.format('%02x',bytes:byte(i)) end
        text="Binary file: "..#bytes.." bytes\nFirst 256 bytes (hex):\n"..table.concat(hex,' ')
    end
    local function actions()
        local items={{label="Rename",key="rename"},{label="Delete",key="delete"}}
        if textual and #bytes<=240 then table.insert(items,1,{label="Edit text",key="edit"}) end
        kit.list("File actions",items,function(item)
            if item.key=="edit" then edit(name,bytes,false)
            elseif item.key=="rename" then named("Rename file",name,function(newname)
                local ok,e=cyd.documents.rename(name,newname)
                if ok then open(newname) else kit.notice(e) end
            end,function()open(name)end)
            else kit.confirm("Delete file?",name,function()
                local ok,e=cyd.documents.delete(name);browse();kit.notice(ok and "Deleted" or e)
            end,function()open(name)end) end
        end,nil,nil,function()open(name)end)
    end
    kit.show_text(name,text,browse,"Actions",actions)
end
function browse()
    local files,err=cyd.documents.list();local items={}
    for _,f in ipairs(files or {}) do items[#items+1]={label=f.name.." ("..f.size.." B)",name=f.name} end
    kit.list("Documents",items,function(item)open(item.name)end,"New",function()
        named("New file","untitled.txt",function(name)edit(name,"",true)end,browse)
    end)
    if err then kit.notice(err) end
end
browse()
