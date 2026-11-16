/* Real settings service + LVGL UI, controlled clock/NVS/board doubles. */
#include "cyd.h"
#include "../components/device_settings/device.c"
#include "../components/ui/settings.c"
#include <assert.h>
static unsigned screen_w, screen_h, flush_count;
static bool peer_fixture;
static uint16_t pixels[800*24], frame[800*800];
static esp_err_t hardware(unsigned n) {(void)n; return 0;}
esp_err_t cyd_runtime_stop(void) {return 0;}
void cyd_connections_open(void) {} void cyd_connections_open_device(void) {}
void cyd_network_address(char *b,size_t n) {snprintf(b,n,"192.0.2.10");}
bool cyd_storage_ready(void) {return true;}
unsigned cyd_board_width(void) {return 320;} unsigned cyd_board_height(void) {return 240;}
const char *cyd_board_model(void) {return "C5 host fixture";}
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p) {
 assert(a->x1>=0 && a->y1>=0 && a->x2<(int)screen_w && a->y2<(int)screen_h);
 uint16_t *s=(uint16_t *)p;
 for(int y=a->y1;y<=a->y2;y++) for(int x=a->x1;x<=a->x2;x++) frame[y*screen_w+x]=*s++;
 flush_count++; lv_display_flush_ready(d);
}
static void render(void) {for(int i=0;i<4;i++){host_tick(50);lv_tick_inc(50);lv_timer_handler();}}
static void pump(void) {device_job_t j;while(xQueueReceive(jobs,&j,0))apply_job(&j);device_tick();render();}
static lv_obj_t *find(lv_obj_t *o,const char *name) {
 if(lv_obj_check_type(o,&lv_label_class) && !strcmp(lv_label_get_text(o),name) && lv_obj_check_type(lv_obj_get_parent(o),&lv_button_class))return lv_obj_get_parent(o);
 for(unsigned i=0;i<lv_obj_get_child_count(o);i++){lv_obj_t *r=find(lv_obj_get_child(o,i),name);if(r)return r;}
 return NULL;
}
static void click(const char *s) {lv_obj_t *b=find(view,!strcmp(s,"Back")?"Up":s);assert(b);cyd_pager_reveal(body,b);render();lv_obj_send_event(b,LV_EVENT_CLICKED,NULL);render();}
static lv_obj_t *dropdown(unsigned n) {
 for(unsigned i=0;i<3;i++){if(choice_buttons[i] && !n--)return choice_buttons[i];}
 abort();
}
static void choose(unsigned n,unsigned value) {
 lv_obj_t *o=dropdown(n);unsigned field=0;while(field<3&&choice_buttons[field]!=o)field++;assert(field<3);
 for(unsigned tries=0;choice_values[field]!=value&&tries<5;tries++){cyd_pager_reveal(body,o);lv_obj_send_event(o,LV_EVENT_CLICKED,NULL);pump();}
 assert(choice_values[field]==value);
}
static void save(const char *dir,const char *name){char path[512];snprintf(path,sizeof(path),"%s/settings-%s-%ux%u.ppm",dir,name,screen_w,screen_h);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",screen_w,screen_h);for(unsigned i=0;i<screen_w*screen_h;i++){uint16_t p=frame[i];unsigned char c[]={(p>>11)*255/31,((p>>5)&63)*255/63,(p&31)*255/31};fwrite(c,1,3,f);}fclose(f);}
int main(int argc,char **argv) {
 assert(argc==4);screen_w=atoi(argv[1]);screen_h=atoi(argv[2]);host_reset();lv_init();
 lv_display_t *d=lv_display_create(screen_w,screen_h);lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
 lv_theme_default_init(d,lv_palette_main(LV_PALETTE_BLUE),lv_palette_main(LV_PALETTE_CYAN),true,LV_FONT_DEFAULT);
 assert(!cyd_device_init(320,240,hardware,hardware));cyd_settings_init(lv_screen_active(),screen_w,screen_h);cyd_settings_open();render();save(argv[3],"home");
 click("Display & touch");
 for(unsigned settle=0;settle<10;settle++)render();
 unsigned idle_before=flush_count;
 for(unsigned idle=0;idle<10;idle++)render();
 assert(flush_count==idle_before); /* unchanged settings must not redraw at 5Hz */
 choose(0,1);choose(1,1);assert(preferences.dim_seconds==30&&preferences.dim_percent==25);save(argv[3],"display");
 click("Back");click("Clock");choose(0,1);assert(preferences.clock_12h);click("UTC offset +15 minutes");pump();assert(config.offset==15);
 click("Back");click("Diagnostics");assert(strstr(lv_label_get_text(live),"Internal largest"));save(argv[3],"monitor");
 click("Pause / resume readings");char before[1024];strcpy(before,lv_label_get_text(live));host_tick(5000);render();assert(!strcmp(before,lv_label_get_text(live)));
 click("Pause / resume readings");host_tick(1500);render();assert(strcmp(before,lv_label_get_text(live)));
 click("Cycle refresh: 1s / 250ms / 2s");assert(monitor_interval==250);
 click("Back");click("Touch input test");int x,y;cyd_device_touch(700,1100,true,&x,&y);host_tick(1100);render();assert(strstr(lv_label_get_text(live),"700, 1100"));assert(touch_dot);save(argv[3],"touch");
 click("Back");click("Screen color test");click("Next test color");assert(test_color==1);save(argv[3],"screen");
 click("Back");click("Developer & modules");render();save(argv[3],"developer");click("Back");click("About this tablet");render();save(argv[3],"about");
 click("Back");click("Connected boards");render();assert(strstr(lv_label_get_text(live),"disconnected"));save(argv[3],"link");
 peer_fixture=true;host_tick(1100);render();assert(strstr(lv_label_get_text(live),"remote-fixture"));
 peer_fixture=false;host_tick(1100);render();assert(!strstr(lv_label_get_text(live),"remote-fixture"));
 click("Back");click("About this tablet");
 lv_display_set_resolution(d,screen_h,screen_w);cyd_settings_resize(screen_h,screen_w);unsigned old=screen_w;screen_w=screen_h;screen_h=old;render();assert(page==8);
 cyd_settings_close();assert(!live&&!touch_pad);cyd_settings_open_diagnostics();render();assert(page==4&&live);cyd_settings_close();lv_deinit();puts("PASS settings preferences, live/pause/refresh, touch, colors, navigation and resize");return 0;
}

#include "cyd_peer.h"
void cyd_peer_snapshot(cyd_peer_info_t *out) {
    *out = (cyd_peer_info_t){.role = CYD_LINK_COMPANION, .echo_result = -1,
        .message = "Local operation; board link disabled pending qualified wiring"};
    if (peer_fixture) {
        out->configured=out->running=out->connected=out->remote_valid=true;
        out->message="Host fixture connected";out->age_ms=100;out->remote.free_heap=100000;
        strcpy(out->remote.version,"remote-fixture");
    }
}
