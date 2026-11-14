/* Executes the actual CYDOS runtime, pinned Lua and LVGL. RTOS/SD transport are
 * explicit host fixtures; no display hardware or real input controller is used. */
#include "mock.h"
#include "runtime.c"
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
static app_t application;
static lua_State *vm;
static int64_t clock_us=1000000;
static unsigned width=320,height=240;
static uint16_t *framebuffer;
static const char *app_folder;
uint32_t esp_random(void){return 42;}
int64_t esp_timer_get_time(void){return clock_us;}
SemaphoreHandle_t xSemaphoreCreateMutex(void){return (void *)1;}
SemaphoreHandle_t xSemaphoreCreateBinary(void){return (void *)1;}
int xSemaphoreTake(SemaphoreHandle_t s,unsigned t){(void)s;(void)t;return 1;}
int xSemaphoreGive(SemaphoreHandle_t s){(void)s;return 1;}
void vSemaphoreDelete(SemaphoreHandle_t s){(void)s;}
int xTaskCreate(void (*f)(void *),const char *n,unsigned z,void *a,unsigned p,void *h){(void)f;(void)n;(void)z;(void)a;(void)p;(void)h;return 1;}
void vTaskDelete(void *p){(void)p;}
void vTaskDelay(unsigned ms){clock_us+=ms*1000;}
void cyd_ui_lock(void){}
void cyd_ui_unlock(void){}
lv_obj_t *cyd_ui_app_root(void){return application.root;}
void cyd_ui_show_app(bool show){(void)show;}
bool cyd_maintenance_active(void){return false;}
void cyd_network_snapshot(cyd_net_status_t *s){memset(s,0,sizeof(*s));}
esp_err_t cyd_mkdirs(const char *s){return mkdir(s,0700)==0||errno==EEXIST?0:ESP_FAIL;}
esp_err_t cyd_write_file(const char *p,const void *v,size_t n){FILE *f=fopen(p,"wb");if(!f)return ESP_FAIL;size_t written=fwrite(v,1,n,f);int e=fclose(f);return written==n&&!e?0:ESP_FAIL;}
esp_err_t cyd_read_file(const char *p,uint8_t **v,size_t *n,size_t max){FILE *f=fopen(p,"rb");if(!f)return ESP_ERR_NOT_FOUND;fseek(f,0,SEEK_END);long size=ftell(f);rewind(f);if(size<0||(size_t)size>max){fclose(f);return ESP_ERR_INVALID_SIZE;}*v=calloc(1,size+1);if(!*v){fclose(f);return ESP_ERR_NO_MEM;}*n=fread(*v,1,size,f);fclose(f);return *n==(size_t)size?0:ESP_FAIL;}
esp_err_t cyd_credentials_get(const char *a,const char *k,void *v,size_t *n){(void)a;(void)k;(void)v;(void)n;return ESP_ERR_NOT_FOUND;}
esp_err_t cyd_credentials_set(const char *a,const char *k,const void *v,size_t n){(void)a;(void)k;(void)v;(void)n;abort();}
esp_err_t cyd_credentials_delete(const char *a,const char *k){(void)a;(void)k;abort();}
static void *no_network(const cyd_endpoint_t *e,char *error,size_t n){(void)e;(void)error;(void)n;abort();}
cyd_io_t cyd_services_esp_io(void){return (cyd_io_t){.open=no_network};}
static int eval(const char *code){application.deadline=clock_us+500000;int result=luaL_loadstring(vm,code);if(!result)result=lua_pcall(vm,0,0,0);if(result){fprintf(stderr,"Lua: %s\n",lua_tostring(vm,-1));lua_pop(vm,1);}return result;}
static void render(void){for(unsigned i=0;i<4;i++){lv_tick_inc(20);lv_timer_handler();}}
static void dispatch(void){while(cyd_services_pop(application.services,&application.delivery)){lua_pushcfunction(vm,dispatch_event);application.deadline=clock_us+500000;assert(lua_pcall(vm,0,0,0)==0);}render();}
static void tick(unsigned seconds){clock_us+=(int64_t)seconds*1000000;cyd_services_tick(application.services,clock_us/1000);dispatch();}
static void close_app(void){application.closing=true;cyd_services_close(application.services);free(application.services);application.services=NULL;lua_close(vm);vm=NULL;assert(application.memory==0);if(application.input_overlay)lv_obj_delete(application.input_overlay);lv_obj_delete(application.root);}
static void boot(void){
    memset(&application,0,sizeof(application));
    application.root=lv_obj_create(lv_screen_active());
    lv_obj_set_pos(application.root,0,34);lv_obj_set_size(application.root,width,height-88);
    lv_obj_set_style_pad_all(application.root,12,0);
    application.manifest.storage=true;strcpy(application.manifest.id,"org.cyd.pocketpal");strcpy(application.rev,"0000000000000001");
    application.widgets[UI_LIMIT]=(widget_t){.owner=&application,.slot=UI_LIMIT,.generation=1,.object=application.root,.refs={LUA_NOREF,LUA_NOREF,LUA_NOREF}};
    application.services=calloc(1,sizeof(cyd_services_t));cyd_io_t io=cyd_services_esp_io();cyd_services_init(application.services,&io);application.services->now=clock_us/1000;
    application.service_guard=(void *)1;for(unsigned i=0;i<16;i++)application.timer_refs[i]=LUA_NOREF;application.http_ref=LUA_NOREF;for(unsigned i=0;i<2;i++)application.mqtt_refs[i][0]=application.mqtt_refs[i][1]=LUA_NOREF;
    atomic_store(&cancel,false);vm=lua_newstate(allocator,&application);assert(vm);*(app_t **)lua_getextraspace(vm)=&application;
    lua_pushcfunction(vm,bind_api);assert(lua_pcall(vm,0,0,0)==0);lua_sethook(vm,hook,LUA_MASKCOUNT,1000);
    char path[512];snprintf(path,sizeof(path),"%s/main.lua",app_folder);uint8_t *code;size_t n;assert(cyd_read_file(path,&code,&n,65536)==0);assert(eval((char *)code)==0);free(code);render();
}
static widget_t *find_button(const char *text){for(unsigned i=0;i<UI_LIMIT;i++){widget_t *w=&application.widgets[i];if(w->object&&w->label&&!strcmp(lv_label_get_text(w->label),text))return w;}return NULL;}
static void click(const char *text){widget_t *w=find_button(text);if(!w)fprintf(stderr,"Missing button: %s\n",text);assert(w);lv_obj_send_event(w->object,LV_EVENT_CLICKED,NULL);dispatch();}
static void visible_action(const char *text){widget_t *w=find_button(text);assert(w);lv_obj_update_layout(application.root);lv_area_t bounds,child;lv_obj_get_coords(application.root,&bounds);lv_obj_get_coords(w->object,&child);assert(child.x1>=bounds.x1&&child.x2<=bounds.x2&&child.y1>=bounds.y1&&child.y2<=bounds.y2);assert(child.y2<(int)height-50);}
static cJSON *saved(void){cJSON *latest=NULL;for(unsigned i=0;i<2;i++){const char *path=i?CYD_ROOT "/data/org.cyd.pocketpal/pal-b.json":CYD_ROOT "/data/org.cyd.pocketpal/pal-a.json";uint8_t *bytes;size_t n;if(cyd_read_file(path,&bytes,&n,16384))continue;cJSON *parsed=cJSON_Parse((char *)bytes);free(bytes);if(!parsed)continue;if(!latest||cJSON_GetObjectItem(parsed,"sequence")->valueint>cJSON_GetObjectItem(latest,"sequence")->valueint){cJSON_Delete(latest);latest=parsed;}else cJSON_Delete(parsed);}assert(latest);return latest;}
static int number(const char *key){cJSON *s=saved();int value=cJSON_GetObjectItem(s,key)->valueint;cJSON_Delete(s);return value;}
static void screenshot(const char *path){render();FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",width,height);for(unsigned i=0;i<width*height;i++){uint16_t c=framebuffer[i];unsigned char p[3]={(c>>11)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};fwrite(p,1,3,f);}fclose(f);}
static uint16_t draw[800*24];
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *pixels){assert(a->x1>=0&&a->y1>=0&&a->x2<(int)width&&a->y2<(int)height);unsigned stride=a->x2-a->x1+1;for(int y=a->y1;y<=a->y2;y++)memcpy(framebuffer+y*width+a->x1,pixels+(y-a->y1)*stride*2,stride*2);lv_display_flush_ready(d);}
static void setup_resources(void){
    mkdir(CYD_ROOT,0700);mkdir(CYD_ROOT "/data",0700);mkdir(CYD_ROOT "/data/org.cyd.pocketpal",0700);mkdir(CYD_ROOT "/projects",0700);mkdir(CYD_ROOT "/projects/0000000000000001",0700);mkdir(CYD_ROOT "/projects/0000000000000001/sprites",0700);
    const char *moods[]={"happy","happy2","blink","hungry","dirty","bored","tired","sleep","sick"};
    for(unsigned i=0;i<9;i++){char from[512],to[256];snprintf(from,sizeof(from),"%s/sprites/%s.rgb565",app_folder,moods[i]);snprintf(to,sizeof(to),CYD_ROOT "/projects/0000000000000001/sprites/%s.rgb565",moods[i]);uint8_t *bytes;size_t n;assert(cyd_read_file(from,&bytes,&n,32776)==0);assert(cyd_write_file(to,bytes,n)==0);free(bytes);}
}
int main(int argc,char **argv){
    assert(argc==5);app_folder=argv[1];width=atoi(argv[2]);height=atoi(argv[3]);setup_resources();
    lv_init();framebuffer=calloc(width*height,2);lv_display_t *d=lv_display_create(width,height);lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,draw,NULL,sizeof(draw),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
    lv_obj_set_style_bg_color(lv_screen_active(),lv_color_hex(0x08090d),0);
    lv_obj_t *title=lv_label_create(lv_screen_active());lv_label_set_text(title,"CYD OS  /  Pocket Pal");lv_obj_set_pos(title,10,9);lv_obj_set_style_text_color(title,lv_color_hex(0xffffff),0);
    lv_obj_t *home=lv_button_create(lv_screen_active());lv_obj_set_pos(home,(width-200)/2,height-50);lv_obj_set_size(home,200,44);lv_obj_t *home_label=lv_label_create(home);lv_label_set_text(home_label,"Home / Stop app");lv_obj_center(home_label);
    boot();for(unsigned i=0;i<4;i++)visible_action((const char *[4]){"Feed","Play","Wash","Sleep"}[i]);screenshot(argv[4]);
    int food=number("food");click("Feed");assert(number("food")>food);int sequence=number("sequence");click("Feed");assert(number("sequence")==sequence);
    click("Sleep");assert(find_button("Wake"));sequence=number("sequence");click("Feed");assert(number("sequence")==sequence);int energy=number("energy");tick(60);assert(number("energy")>energy);
    click("Wake");click("Play");for(unsigned i=0;i<3;i++)click("*");assert(find_button("Feed"));assert(number("joy")>=90);
    click("More");visible_action("Name");
    lv_obj_t *food_meter=NULL;for(unsigned i=0;i<UI_LIMIT;i++)if(application.widgets[i].object&&lv_obj_check_type(application.widgets[i].object,&lv_bar_class)){food_meter=application.widgets[i].object;break;}assert(food_meter);int old_value=lv_bar_get_value(food_meter);tick(60);assert(lv_bar_get_value(food_meter)<old_value);
    click("Name");visible_action("Save name");widget_t *input=NULL;for(unsigned i=0;i<UI_LIMIT;i++){widget_t *w=&application.widgets[i];if(w->object&&lv_obj_check_type(w->object,&lv_textarea_class))input=w;}assert(input);input_text(input->object,"Pip");click("Save name");for(unsigned i=0;i<4;i++)visible_action((const char *[4]){"Feed","Play","Wash","Sleep"}[i]);
    cJSON *s=saved();assert(!strcmp(cJSON_GetObjectItem(s,"name")->valuestring,"Pip"));cJSON_Delete(s);
    for(unsigned i=0;i<600;i++){tick(1);assert(application.images==1);}assert(application.memory<200000);
    int age=number("age");close_app();clock_us+=3600000000LL;boot();assert(number("age")==age);
    sequence=number("sequence");const char *newest=sequence%2?CYD_ROOT "/data/org.cyd.pocketpal/pal-b.json":CYD_ROOT "/data/org.cyd.pocketpal/pal-a.json";
    close_app();assert(cyd_write_file(newest,"{truncated",10)==0);boot();assert(number("age")<=age);s=saved();assert(!strcmp(cJSON_GetObjectItem(s,"name")->valuestring,"Pip"));cJSON_Delete(s);
    assert(eval("assert(cyd.network~=nil and coroutine==nil)")==0);for(unsigned i=0;i<3;i++)assert(!application.services->conn[i].used);
    close_app();lv_deinit();free(framebuffer);printf("PASS actual Lua/LVGL %ux%u: care/game, visible controls, 600 animation ticks, saved rename, pause/restart, corrupt-save recovery, no network, clean teardown\n",width,height);return 0;
}
