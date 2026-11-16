/* Included by runtime.c: all LVGL access is serialized with the existing OS
 * lock. */
typedef struct {
    app_t *owner;
    unsigned slot;
    uint32_t generation;
} widget_handle_t;
static widget_t *widget(lua_State *L, int index) {
    app_t *a = context(L);
    widget_handle_t *h = luaL_checkudata(L, index, "cyd.object");
    luaL_argcheck(L, h->owner == a && h->slot <= UI_LIMIT, index, "Foreign widget");
    widget_t *w = &a->widgets[h->slot];
    luaL_argcheck(L, w->object && w->generation == h->generation, index, "Deleted/stale widget");
    return w;
}
static lv_obj_t *object(lua_State *L, int i) { return widget(L, i)->object; }
static int push_widget(lua_State *L, widget_t *w) {
    widget_handle_t *h = lua_newuserdatauv(L, sizeof(*h), 0);
    *h = (widget_handle_t){w->owner, w->slot, w->generation};
    luaL_setmetatable(L, "cyd.object");
    return 1;
}
static int create_screen(lua_State *L) { return push_widget(L, &context(L)->widgets[UI_LIMIT]); }
static int viewport(lua_State *L) {
    app_t *a = context(L);
    cyd_ui_lock();
    lv_obj_update_layout(a->root);
    int w = lv_obj_get_content_width(a->root), h = lv_obj_get_content_height(a->root);
    cyd_ui_unlock();
    lua_pushinteger(L, w);
    lua_pushinteger(L, h);
    return 2;
}
static int show(lua_State *L) {
    app_t *a = context(L);
    widget_t *w = widget(L, 1);
    if (w->slot != UI_LIMIT && !w->view)
        return luaL_error(L, "Expected an application view");
    cyd_ui_lock();
    for (unsigned i = 0; i < UI_LIMIT; i++)
        if (a->widgets[i].object && a->widgets[i].view) {
            if (&a->widgets[i] == w)
                lv_obj_remove_flag(a->widgets[i].object, LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(a->widgets[i].object, LV_OBJ_FLAG_HIDDEN);
        }
    cyd_ui_unlock();
    return 0;
}
static void option_field(lua_State *L, int index, const char *name) {
    index = lua_absindex(L, index);
    lua_pushstring(L, name);
    lua_rawget(L, index);
}
static int field_int(lua_State *L, int table, const char *key, int fallback, int low, int high) {
    option_field(L, table, key);
    lua_Integer n = luaL_optinteger(L, -1, fallback);
    lua_pop(L, 1);
    if (n < low || n > high)
        luaL_error(L, "Invalid %s range", key);
    return (int)n;
}
static bool field_bool(lua_State *L, int table, const char *key) {
    option_field(L, table, key);
    bool v = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return v;
}
static const char *field_text(lua_State *L, int table, const char *key, const char *fallback,
                              size_t limit) {
    option_field(L, table, key);
    const char *s = lua_isnil(L, -1) ? fallback : bounded_string(L, -1, limit);
    lua_pop(L, 1);
    return s;
}
static void widget_event(lv_event_t *e) {
    widget_t *w = lv_event_get_user_data(e);
    app_t *a = w->owner;
    if (a->closing)
        return;
    unsigned kind = lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED ? 1 : 0;
    if (w->refs[kind] == LUA_NOREF)
        return;
    xSemaphoreTake(a->service_guard, portMAX_DELAY);
    if (!a->closing && a->services)
        cyd_services_touch(a->services, w->slot * 3 + kind, w->generation);
    xSemaphoreGive(a->service_guard);
}
static void widget_deleted(lv_event_t *e) {
    widget_t *w = lv_event_get_user_data(e);
    if (w->pixels) {
        lv_image_cache_drop(&w->image);
        free(w->pixels);
        w->pixels = NULL;
        w->owner->images--;
    }
    w->object = w->label = NULL;
    if (w->generation != UINT32_MAX)
        w->generation++;
}
/* LVGL 9.1's masked set_text(max_length) duplicates its password buffer.
   Toggle under the UI lock around updates; no unmasked frame is rendered. */
static void input_text(lv_obj_t *object, const char *text) {
    bool masked = lv_textarea_get_password_mode(object);
    if (masked)
        lv_textarea_set_password_mode(object, false);
    lv_textarea_set_text(object, text);
    if (masked)
        lv_textarea_set_password_mode(object, true);
}
static void input_keyboard(lv_event_t *e) {
    app_t *a = lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_READY && code != LV_EVENT_CANCEL)
        return;
    widget_t *w = a->input_target;
    if (code == LV_EVENT_READY && !a->closing && w && w->object &&
        w->generation == a->input_generation) {
        const char *text = lv_textarea_get_text(a->input_edit);
        if (strlen(text) > 256)
            return;
        input_text(w->object, text);
        if (w->refs[2] != LUA_NOREF) {
            xSemaphoreTake(a->service_guard, portMAX_DELAY);
            if (!a->closing && a->services)
                cyd_services_touch(a->services, w->slot * 3 + 2, w->generation);
            xSemaphoreGive(a->service_guard);
        }
    }
    lv_obj_add_flag(a->input_overlay, LV_OBJ_FLAG_HIDDEN);
    a->input_target = NULL;
}
static void input_focus(lv_event_t *e) {
    widget_t *w = lv_event_get_user_data(e);
    app_t *a = w->owner;
    if (a->closing)
        return;
    if (!a->input_overlay) {
        lv_display_t *d = lv_obj_get_display(a->root);
        int width = lv_display_get_horizontal_resolution(d),
            height = lv_display_get_vertical_resolution(d) - 56;
        a->input_overlay = lv_obj_create(lv_obj_get_parent(a->root));
        lv_obj_set_pos(a->input_overlay, 0, 0);
        lv_obj_set_size(a->input_overlay, width, height);
        lv_obj_set_style_pad_all(a->input_overlay, 0, 0);
        lv_obj_remove_flag(a->input_overlay, LV_OBJ_FLAG_SCROLLABLE);
        a->input_edit = lv_textarea_create(a->input_overlay);
        lv_obj_set_pos(a->input_edit, 4, 4);
        lv_obj_set_width(a->input_edit, width - 8);
        lv_textarea_set_one_line(a->input_edit, true);
        lv_textarea_set_max_length(a->input_edit, 256);
        lv_obj_t *keys = lv_keyboard_create(a->input_overlay);
        lv_obj_set_align(keys, LV_ALIGN_TOP_LEFT);
        lv_obj_set_pos(keys, 0, 52);
        lv_obj_set_size(keys, width, height - 52);
        lv_keyboard_set_textarea(keys, a->input_edit);
        lv_obj_add_event_cb(keys, input_keyboard, LV_EVENT_ALL, a);
    }
    a->input_target = w;
    a->input_generation = w->generation;
    lv_textarea_set_password_mode(a->input_edit, lv_textarea_get_password_mode(w->object));
    input_text(a->input_edit, lv_textarea_get_text(w->object));
    lv_obj_remove_flag(a->input_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(a->input_overlay);
}
static int add_widget_kind(lua_State *L, unsigned kind) {
    app_t *a = context(L);
    lv_obj_t *parent = object(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    if (kind == 5 && parent != a->root)
        return luaL_error(L, "Views must be direct children of the app container");
    unsigned slot;
    for (slot = 0; slot < UI_LIMIT; slot++)
        if (!a->widgets[slot].object && a->widgets[slot].generation != UINT32_MAX)
            break;
    if (slot == UI_LIMIT)
        return luaL_error(L, "UI object quota exceeded");
    const char *text = field_text(L, 2, "text", "", kind == 4 ? 256 : 1024);
    if (!utf8_valid((const unsigned char *)text, strlen(text)))
        return luaL_error(L, "Invalid UTF-8 text");
    int default_width = 120;
    cyd_ui_lock();
    if (kind == 0)
        default_width = lv_obj_get_content_width(parent) - 24;
    cyd_ui_unlock();
    if (default_width < 1)
        default_width = 1;
    int x = field_int(L, 2, "x", 20, 0, 800), y = field_int(L, 2, "y", 20, 0, 800),
        width = field_int(L, 2, "width", kind == 1 ? 180 : default_width, 1, 800),
        height = field_int(L, 2, "height", kind == 1 ? 52 : 40, 1, 800),
        color = field_int(L, 2, "color", 0xffffff, 0, 0xffffff),
        bg = field_int(L, 2, "background", 0x222222, 0, 0xffffff),
        font = field_int(L, 2, "font", 14, 14, 28), value = field_int(L, 2, "value", 0, 0, 100);
    if (font != 14 && font != 20 && font != 28)
        return luaL_error(L, "Fonts: 14,20,28 only");
    const char *layout = field_text(L, 2, "layout", "", 6);
    if (*layout && strcmp(layout, "column") && strcmp(layout, "row"))
        return luaL_error(L, "Unknown layout");
    bool scroll = field_bool(L, 2, "scroll"), masked = field_bool(L, 2, "masked");
    int padding = field_int(L,2,"padding",-1,-1,24),
        border = field_int(L,2,"border",-1,-1,4),
        radius = field_int(L,2,"radius",-1,-1,32);
    option_field(L,2,"height");bool explicit_height=!lua_isnil(L,-1);lua_pop(L,1);
    widget_t *w = &a->widgets[slot];
    for (unsigned i = 0; i < 3; i++) {
        if (w->refs[i] != 0 && w->refs[i] != LUA_NOREF)
            luaL_unref(L, LUA_REGISTRYINDEX, w->refs[i]);
        w->refs[i] = LUA_NOREF;
    }
    const char *names[] = {"on_click", "on_change", "on_submit"};
    for (unsigned i = 0; i < 3; i++) {
        option_field(L, 2, names[i]);
        if (!lua_isnil(L, -1)) {
            luaL_checktype(L, -1, LUA_TFUNCTION);
            w->refs[i] = luaL_ref(L, LUA_REGISTRYINDEX);
        } else
            lua_pop(L, 1);
    }
    w->owner = a;
    w->slot = slot;
    w->generation++;
    w->view = kind == 5;
    cyd_ui_lock();
    lv_obj_t *o = kind == 0   ? lv_label_create(parent)
                  : kind == 1 ? lv_button_create(parent)
                  : kind == 2 ? lv_bar_create(parent)
                  : kind == 4 ? lv_textarea_create(parent)
                              : lv_obj_create(parent);
    w->object = o;
    if (!o) {
        cyd_ui_unlock();
        return luaL_error(L, "Native UI allocation failed");
    }
    lv_obj_set_pos(o, x, y);
    if (kind != 0)
        lv_obj_set_size(o, width, height);
    else {
        lv_obj_set_width(o, width);
        if (explicit_height) lv_obj_set_height(o,height);
        lv_label_set_long_mode(o, LV_LABEL_LONG_WRAP);
        lv_label_set_text(o, text);
    }
    lv_obj_set_style_text_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(bg), 0);
    if(padding>=0)lv_obj_set_style_pad_all(o,padding,0);
    if(border>=0)lv_obj_set_style_border_width(o,border,0);
    if(radius>=0)lv_obj_set_style_radius(o,radius,0);
    const lv_font_t *f = font == 20   ? &lv_font_montserrat_20
                         : font == 28 ? &lv_font_montserrat_28
                                      : &lv_font_montserrat_14;
    lv_obj_set_style_text_font(o, f, 0);
    if (kind == 1) {
        w->label = lv_label_create(o);
        lv_label_set_text(w->label, text);
        lv_obj_center(w->label);
    }
    if (kind == 2) {
        lv_obj_set_style_bg_color(o, lv_color_hex(color), LV_PART_INDICATOR);
        lv_bar_set_range(o, 0, 100);
        lv_bar_set_value(o, value, LV_ANIM_OFF);
    }
    if (kind == 3 || kind == 5) {
        if (scroll)
            lv_obj_add_flag(o, LV_OBJ_FLAG_SCROLLABLE);
        else
            lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
        if (*layout)
            lv_obj_set_flex_flow(o,
                                 !strcmp(layout, "row") ? LV_FLEX_FLOW_ROW : LV_FLEX_FLOW_COLUMN);
    }
    if (kind == 4) {
        lv_textarea_set_one_line(o, true);
        lv_textarea_set_max_length(o, 256);
        lv_textarea_set_password_mode(o, masked);
        input_text(o, text);
        lv_obj_add_event_cb(o, input_focus, LV_EVENT_CLICKED, w);
    }
    if (kind == 5)
        lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(o, widget_deleted, LV_EVENT_DELETE, w);
    lv_obj_add_event_cb(o, widget_event, LV_EVENT_CLICKED, w);
    lv_obj_add_event_cb(o, widget_event, LV_EVENT_VALUE_CHANGED, w);
    cyd_ui_unlock();
    return push_widget(L, w);
}
static int label(lua_State *L) { return add_widget_kind(L, 0); }
static int button(lua_State *L) { return add_widget_kind(L, 1); }
static int bar(lua_State *L) { return add_widget_kind(L, 2); }
static int container(lua_State *L) { return add_widget_kind(L, 3); }
static int input(lua_State *L) { return add_widget_kind(L, 4); }
static int view(lua_State *L) { return add_widget_kind(L, 5); }
static int set_text(lua_State *L) {
    widget_t *w = widget(L, 1);
    const char *s = bounded_string(L, 2, 1024);
    if (!utf8_valid((const unsigned char *)s, strlen(s)))
        return luaL_error(L, "Invalid UTF-8 text");
    if (lv_obj_check_type(w->object, &lv_textarea_class) && strlen(s) > 256)
        return luaL_error(L, "Text input limit 256 bytes");
    cyd_ui_lock();
    if (lv_obj_check_type(w->object, &lv_label_class))
        lv_label_set_text(w->object, s);
    else if (w->label)
        lv_label_set_text(w->label, s);
    else if (lv_obj_check_type(w->object, &lv_textarea_class) && strlen(s) <= 256)
        input_text(w->object, s);
    cyd_ui_unlock();
    return 0;
}
static int get_text(lua_State *L) {
    widget_t *w = widget(L, 1);
    char text[1025] = "";
    cyd_ui_lock();
    const char *s = lv_obj_check_type(w->object, &lv_label_class) ? lv_label_get_text(w->object)
                    : w->label                                    ? lv_label_get_text(w->label)
                    : lv_obj_check_type(w->object, &lv_textarea_class)
                        ? lv_textarea_get_text(w->object)
                        : "";
    snprintf(text, sizeof(text), "%s", s);
    cyd_ui_unlock();
    lua_pushstring(L, text);
    return 1;
}
static int set_value(lua_State *L) {
    lv_obj_t *o = object(L, 1);
    lua_Integer n = luaL_checkinteger(L, 2);
    if (n < 0 || n > 100)
        return luaL_error(L, "Value range 0..100");
    cyd_ui_lock();
    if (lv_obj_check_type(o, &lv_bar_class))
        lv_bar_set_value(o, n, LV_ANIM_OFF);
    cyd_ui_unlock();
    return 0;
}
static int set_color(lua_State *L) {
    lv_obj_t *o = object(L, 1);
    lua_Integer n = luaL_checkinteger(L, 2);
    if (n < 0 || n > 0xffffff)
        return luaL_error(L, "Invalid color");
    cyd_ui_lock();
    lv_obj_set_style_text_color(o, lv_color_hex(n), 0);
    cyd_ui_unlock();
    return 0;
}
static int set_background(lua_State *L) {
    lv_obj_t *o = object(L, 1);
    lua_Integer n = luaL_checkinteger(L, 2);
    if (n < 0 || n > 0xffffff)
        return luaL_error(L, "Invalid color");
    cyd_ui_lock();
    lv_obj_set_style_bg_color(o, lv_color_hex(n), 0);
    cyd_ui_unlock();
    return 0;
}
static int delete_widget(lua_State *L) {
    widget_t *w = widget(L, 1);
    if (w->slot == UI_LIMIT)
        return luaL_error(L, "OS app container cannot be deleted");
    cyd_ui_lock();
    lv_obj_delete(w->object);
    cyd_ui_unlock();
    return 0;
}
static int add_image(lua_State *L) {
    app_t *a = context(L);
    if (a->images >= 2)
        return luaL_error(L, "Image quota 2");
    (void)object(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    const char *name = field_text(L, 2, "resource", "", CYD_MAX_PATH - 1);
    if (!cyd_safe_path(name))
        return luaL_error(L, "Invalid image resource");
    native_holder_t *h = native_holder(L);
    uint8_t *bytes;
    size_t n;
    if (read_resource(a, name, &bytes, &n, 32776))
        return luaL_error(L, "Image unavailable");
    h->data = bytes;
    if (n < 8 || memcmp(bytes, "CIMG", 4))
        return luaL_error(L, "Expected CIMG RGB565 resource");
    unsigned width = bytes[4] | (bytes[5] << 8), height = bytes[6] | (bytes[7] << 8);
    if (!width || !height || width > 128 || height > 128 || n != 8 + width * height * 2)
        return luaL_error(L, "Image size invalid");
    /* Reuse a tracked generic widget, then attach an owned LVGL image source. */
    option_field(L, 2, "x");
    lua_Integer x = luaL_optinteger(L, -1, 0);
    lua_pop(L, 1);
    option_field(L, 2, "y");
    lua_Integer y = luaL_optinteger(L, -1, 0);
    lua_pop(L, 1);
    if (x < 0 || y < 0 || x > 800 || y > 800)
        return luaL_error(L, "Image position invalid");
    unsigned slot;
    for (slot = 0; slot < UI_LIMIT; slot++)
        if (!a->widgets[slot].object && a->widgets[slot].generation != UINT32_MAX)
            break;
    if (slot == UI_LIMIT)
        return luaL_error(L, "Widget quota exceeded");
    widget_t *w = &a->widgets[slot];
    for (unsigned i = 0; i < 3; i++) {
        if (w->refs[i] != 0 && w->refs[i] != LUA_NOREF)
            luaL_unref(L, LUA_REGISTRYINDEX, w->refs[i]);
        w->refs[i] = LUA_NOREF;
    }
    w->owner = a;
    w->slot = slot;
    w->generation++;
    w->view = false;
    cyd_ui_lock();
    w->object = lv_image_create(object(L, 1));
    w->pixels = bytes;
    h->data = NULL;
    a->images++;
    w->image = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC,
                                           .cf = LV_COLOR_FORMAT_RGB565,
                                           .w = width,
                                           .h = height,
                                           .stride = width * 2},
                                .data_size = width * height * 2,
                                .data = bytes + 8};
    lv_image_set_src(w->object, &w->image);
    lv_obj_set_pos(w->object, x, y);
    lv_obj_add_event_cb(w->object, widget_deleted, LV_EVENT_DELETE, w);
    cyd_ui_unlock();
    return push_widget(L, w);
}
static void ui_metatable(lua_State *L) {
    luaL_newmetatable(L, "cyd.object");
    lua_pushliteral(L, "protected CYD widget");
    lua_setfield(L, -2, "__metatable");
    lua_newtable(L);
    const luaL_Reg funcs[] = {{"add_label", label},
                              {"add_button", button},
                              {"add_bar", bar},
                              {"add_container", container},
                              {"add_rectangle", container},
                              {"add_input", input},
                              {"add_view", view},
                              {"add_image", add_image},
                              {"set_text", set_text},
                              {"get_text", get_text},
                              {"set_value", set_value},
                              {"set_color", set_color},
                              {"set_background", set_background},
                              {"delete", delete_widget},
                              {NULL, NULL}};
    for (const luaL_Reg *fn = funcs; fn->name; fn++)
        ui_table_fn(L, fn->name, fn->func);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);
}
static void bind_ui_api(lua_State *L) { (void)L; }
static int text_lines(lua_State *L) {
    const char *text=bounded_string(L,1,16384);
    size_t size=strlen(text),at=0;
    if(!utf8_valid((const unsigned char *)text,size))return luaL_error(L,"Invalid UTF-8 text");
    int width=(int)luaL_checkinteger(L,2);
    if(width<16||width>800)return luaL_error(L,"Text width must be 16-800");
    int pixels=(int)luaL_optinteger(L,3,14);
    if(pixels!=14&&pixels!=20&&pixels!=28)return luaL_error(L,"Fonts: 14,20,28 only");
    const lv_font_t *font=pixels==20?&lv_font_montserrat_20:pixels==28?&lv_font_montserrat_28:&lv_font_montserrat_14;
    lua_newtable(L);unsigned count=0;
    while(at<size && count<512){
        cyd_ui_lock();
        uint32_t n=_lv_text_get_next_line(text+at,font,0,width,NULL,LV_TEXT_FLAG_NONE);
        cyd_ui_unlock();
        if(!n) {unsigned char c=text[at];n=c<128?1:c<224?2:c<240?3:4;}
        if(n>size-at)n=(uint32_t)(size-at);
        size_t end=at+n;while(end>at&&(text[end-1]=='\n'||text[end-1]=='\r'))--end;
        lua_pushlstring(L,text+at,end-at);lua_rawseti(L,-2,++count);at+=n;
    }
    if(!count){lua_pushliteral(L,"");lua_rawseti(L,-2,1);}
    lua_pushboolean(L,at<size);return 2;
}
