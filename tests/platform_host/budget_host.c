/* Real sandbox/UI methods with controlled UI latency and script clock costs. */
#define main inherited_runtime_main
#include "runtime_host.c"
#undef main
static void expect_error(const char *code, const char *message) {
    execution_begin(&application);
    int rc = luaL_loadstring(vm, code);
    if (!rc) rc = lua_pcall(vm, 0, 0, 0);
    assert(rc && strstr(lua_tostring(vm, -1), message));
    lua_pop(vm, 1);
}
int main(void) {
    assert(!cyd_mkdirs(CYD_ROOT "/projects/0000000000000001"));
    lv_init();
    lv_display_t *d = lv_display_create(800, 480);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d, draw_buffer, NULL, sizeof(draw_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, flush);
    screen_width = 800; screen_height = 480; reset();
    host_ui_delay_us = 40000;
    int64_t before = host_clock;
    assert(eval("s=cyd.ui.create_screen();for i=1,30 do line=s:add_label{text='test'} end;cyd.ui.show(s)") == 0);
    assert(host_clock - before > 500000 && host_clock - before < 5000000);
    host_ui_delay_us = 0;
    before = host_clock;
    assert(eval("local n=0;for i=1,400000 do n=n+1 end;assert(n==400000)") == 0);
    assert(host_clock - before > 500000); /* Required scheduler yields are excluded. */
    host_cpu_tick_us = 20000;
    expect_error("while true do end", "500 ms of script time");
    host_cpu_tick_us = 0; host_ui_delay_us = 40000;
    expect_error("while true do line:set_text('busy') end", "wall time limit exceeded (5 s)");
    atomic_store(&cancel, true);
    before = host_clock;
    expect_error("line:set_text('cancelled')", "Application stopped");
    assert(host_clock == before); /* Cancel stops before another native UI operation. */
    atomic_store(&cancel, false); host_ui_delay_us = 0;
    finish();
    puts("PASS slow UI, yield accounting, script budget, absolute wall cap and Stop");
    return 0;
}
