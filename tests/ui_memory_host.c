/* Real shell startup/drawing with constrained internal allocation and real LVGL. */
#include "cyd.h"
#include "esp_heap_caps.h"
#include "cyd_device.h"
#include "settings.h"
#include "connections.h"
#include "projects.h"
#include <sys/stat.h>
void host_run_task(const char *);
static uint16_t frame[800*800];
static unsigned calls, fail_at, flushes;
static void *buffers[2];
#ifdef CONFIG_IDF_TARGET_ESP32S3
#define TEST_WIDTH 800
#define TEST_HEIGHT 480
#else
#define TEST_WIDTH 320
#define TEST_HEIGHT 240
#endif
void *host_heap_caps_malloc(size_t bytes, int caps) {
#ifdef CYD_PAIRED_UI_TEST
    if(calls>=2 || bytes!=TEST_WIDTH*24*2)return malloc(bytes);
#endif
    assert(calls<2);calls++;
    assert(bytes==TEST_WIDTH*24*2);
#ifdef CONFIG_IDF_TARGET_ESP32S3
    assert((caps&MALLOC_CAP_SPIRAM) && !(caps&(MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA)));
#elif defined(CYD_PAIRED_UI_TEST)
    if (calls == 1)
        assert((caps&MALLOC_CAP_SPIRAM) && !(caps&(MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA)));
    else
        assert((caps&(MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA))==(MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA));
#else
    assert((caps&(MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA))==(MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA));
#endif
    /* Represents the fragmented internal heap after RGB + Wi-Fi startup. */
    if ((caps&MALLOC_CAP_INTERNAL) && bytes>20000) return NULL;
    if (calls==fail_at) return NULL;
    return buffers[calls-1]=malloc(bytes);
}
unsigned cyd_board_width(void) {return TEST_WIDTH;}
unsigned cyd_board_height(void) {return TEST_HEIGHT;}
const char *cyd_board_model(void) {return "Memory regression fixture";}
void cyd_board_flush(lv_display_t *d,const lv_area_t *a,uint8_t *data) {
    assert(data && a->x1>=0 && a->y1>=0 && a->x2<TEST_WIDTH && a->y2<TEST_HEIGHT);
#if defined(CYD_PAIRED_UI_TEST) && !defined(CONFIG_IDF_TARGET_ESP32S3)
    assert(data == buffers[1]); /* Only internal DMA staging reaches C5 SPI. */
#endif
    /* Read all pixels just as RGB's synchronous copy does; ASan checks bounds. */
    volatile uint8_t check=0;
    size_t count=(a->x2-a->x1+1)*(a->y2-a->y1+1)*2;
    for(size_t i=0;i<count;i++)check^=data[i];
    const uint16_t *pixels=(const uint16_t *)data;
    for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)frame[y*TEST_WIDTH+x]=*pixels++;
    (void)check;flushes++;lv_display_flush_ready(d);
}
static void render(void){for(unsigned i=0;i<20;i++){host_tick(20);lv_timer_handler();}}
static lv_obj_t *find_button(lv_obj_t *o,const char *text,bool footer) {
    if(lv_obj_check_type(o,&lv_label_class)&&!strcmp(lv_label_get_text(o),text)&&lv_obj_check_type(lv_obj_get_parent(o),&lv_button_class)) {
        lv_obj_t *b=lv_obj_get_parent(o);lv_area_t a;lv_obj_get_coords(b,&a);
        if(!footer || a.y1>=lv_display_get_vertical_resolution(NULL)-56)return b;
    }
    for(unsigned n=0;n<lv_obj_get_child_count(o);n++){lv_obj_t *r=find_button(lv_obj_get_child(o,n),text,footer);if(r)return r;}return NULL;
}
static bool visible(lv_obj_t *o){for(;o;o=lv_obj_get_parent(o))if(lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN))return false;return true;}
static lv_obj_t *pager_parent(lv_obj_t *o){for(;o;o=lv_obj_get_parent(o))if(lv_obj_get_user_data(o))return o;return NULL;}
static void click(const char *text,bool footer){lv_obj_t *b=find_button(lv_screen_active(),text,footer);assert(b);if(!footer){lv_obj_t *root=pager_parent(b);if(root)cyd_pager_reveal(root,b);render();}assert(visible(b));lv_obj_send_event(b,LV_EVENT_CLICKED,NULL);host_run_task("shell_actions");render();}
static void footer_bounds(void){int w=lv_display_get_horizontal_resolution(NULL),h=lv_display_get_vertical_resolution(NULL);const char *names[]={"Back","Home","Next"};int previous=-1;for(unsigned i=0;i<3;i++){lv_obj_t *b=find_button(lv_screen_active(),names[i],true);assert(b);lv_area_t a;lv_obj_get_coords(b,&a);assert(a.y1>=h-56&&a.y2<h&&a.x1>previous&&a.x2<w);previous=a.x2;}}
static void save(const char *name){const char *dir=getenv("CYD_PAGED_PREVIEW_DIR");if(!dir)return;mkdir(dir,0700);char path[512];snprintf(path,sizeof(path),"%s/%s-%dx%d.ppm",dir,name,TEST_WIDTH,TEST_HEIGHT);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%d %d\n255\n",TEST_WIDTH,TEST_HEIGHT);for(unsigned i=0;i<TEST_WIDTH*TEST_HEIGHT;i++){uint16_t p=frame[i];unsigned char rgb[]={(p>>11)*255/31,((p>>5)&63)*255/63,(p&31)*255/31};fwrite(rgb,1,3,f);}fclose(f);}
static void navigation_test(void){
 cyd_nav_state_t state;assert(!cyd_ui_apply_rotation(0));render();footer_bounds();assert(!find_button(lv_screen_active(),LV_SYMBOL_EYE_OPEN,false));save("home");
 click(LV_SYMBOL_SETTINGS,false);assert(cyd_settings_nav(0,&state));assert(state.page==1);save("settings-1");
 unsigned pages=state.pages;
 for(unsigned n=1;n<pages;n++){click("Next",true);assert(cyd_settings_nav(0,&state)&&state.page==n+1);footer_bounds();}
 assert(lv_obj_has_state(find_button(lv_screen_active(),"Next",true),LV_STATE_DISABLED));save("settings-last");
 for(unsigned n=pages;n>1;n--){click("Back",true);assert(cyd_settings_nav(0,&state)&&state.page==n-1);}
 click("Diagnostics",false);assert(cyd_settings_nav(0,&state));save("diagnostics");
 while(state.page>1){click("Back",true);assert(cyd_settings_nav(0,&state));}
 click("Back",true);assert(cyd_settings_nav(0,&state));assert(find_button(lv_screen_active(),"Display & touch",false));
 click("Display & touch",false);assert(cyd_settings_nav(0,&state));save("display");
 if(state.next){click("Next",true);assert(cyd_settings_nav(0,&state)&&state.page==2);save("display-2");}
 click("Home",true);assert(!cyd_settings_nav(0,&state));
 click(LV_SYMBOL_SETTINGS,false);click("Developer & modules",false);assert(cyd_settings_nav(0,&state));unsigned count=state.pages;assert(count>1);save("developer-1");
 for(unsigned n=1;n<count;n++){click("Next",true);assert(cyd_settings_nav(0,&state)&&state.page==n+1);}save("developer-last");
 assert(!cyd_ui_apply_rotation(1));render();footer_bounds();assert(cyd_settings_nav(0,&state)&&state.page==1);click("Home",true);
 assert(!cyd_ui_apply_rotation(0));render();click(LV_SYMBOL_SETTINGS,false);click("Clock",false);click("Set date & time",false);assert(cyd_settings_nav(0,&state)&&!state.next);footer_bounds();click("Back",true);assert(cyd_settings_nav(0,&state));click("Home",true);
 click(LV_SYMBOL_SETTINGS,false);click("Connections",false);assert(cyd_connections_nav(0,&state));save("connections");click("Home",true);assert(!cyd_connections_nav(0,&state));
 click(LV_SYMBOL_DIRECTORY,false);host_run_task("app_folders");render();assert(cyd_projects_ui_nav(0,&state));save("apps");click("Home",true);
 click(LV_SYMBOL_LIST,false);save("logs");click("Home",true);footer_bounds();assert(lv_obj_has_state(find_button(lv_screen_active(),"Back",true),LV_STATE_DISABLED));
}
int main(int argc,char **argv) {
    assert(argc==2);fail_at=atoi(argv[1]);bool navigation=fail_at==3;if(navigation)fail_at=0;host_reset();
    int result=cyd_ui_init();
    if(fail_at) {
        assert(result==ESP_ERR_NO_MEM && calls==fail_at);
        /* UI startup frees the first buffer when its second allocation fails. */
        lv_deinit();puts("PASS buffer allocation failure cleanup");return 0;
    }
    assert(result==ESP_OK && calls==2);
    for(unsigned r=0;r<4;r++) {
        assert(cyd_ui_apply_rotation(r)==ESP_OK);
        unsigned before=flushes;
        for(unsigned n=0;n<20;n++){host_tick(20);lv_timer_handler();}
        assert(flushes>before);
    }
    if(navigation)navigation_test();
    lv_deinit();free(buffers[0]);free(buffers[1]);
    puts("PASS real UI startup and all rotations with constrained internal heap");return 0;
}
