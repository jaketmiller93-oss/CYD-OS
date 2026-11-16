#include "mock.h"
#include "../../components/app_runtime/runtime.c"
#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>
static int64_t host_clock=1000000;
static unsigned host_ui_delay_us,host_cpu_tick_us;
static bool station,ap;static unsigned screen_width=320,screen_height=240;
static app_t application;static lua_State *vm;
static bool lifecycle;
static const char *reject_task;
static void *created_arg;
static unsigned lua_tasks, service_tasks;
static int sem_values[32];static unsigned sem_count;

uint32_t esp_random(void){static uint32_t n=33;return ++n;}
int64_t esp_timer_get_time(void){host_clock+=host_cpu_tick_us;return host_clock;}
SemaphoreHandle_t xSemaphoreCreateMutex(void){return (void *)1;}SemaphoreHandle_t xSemaphoreCreateBinary(void){if(!lifecycle)return (void *)1;assert(sem_count<32);sem_values[sem_count]=0;return &sem_values[sem_count++];}int xSemaphoreTake(SemaphoreHandle_t s,unsigned t){(void)t;for(unsigned i=0;i<sem_count;i++)if(s==&sem_values[i]){if(!sem_values[i])return 0;sem_values[i]=0;return 1;}return 1;}int xSemaphoreGive(SemaphoreHandle_t s){for(unsigned i=0;i<sem_count;i++)if(s==&sem_values[i]){assert(!sem_values[i]);sem_values[i]=1;}return 1;}void vSemaphoreDelete(SemaphoreHandle_t s){(void)s;}
int xTaskCreate(void (*f)(void *),const char *n,unsigned z,void *a,unsigned p,void *h){(void)f;(void)p;(void)h;
 if(lifecycle){assert(z==12288);if(reject_task&&!strcmp(reject_task,n))return 0;
 if(!strcmp(n,"lua_app")){lua_tasks++;created_arg=a;}else if(!strcmp(n,"app_services"))service_tasks++;else abort();}return 1;}void vTaskDelete(void *p){(void)p;}void vTaskDelay(unsigned ms){host_clock+=ms*1000;}
void cyd_ui_lock(void){host_clock+=host_ui_delay_us;}void cyd_ui_unlock(void){}lv_obj_t *cyd_ui_app_root(void){return application.root;}void cyd_ui_show_app(bool show){(void)show;}bool cyd_maintenance_active(void){return false;}
void cyd_network_snapshot(cyd_net_status_t *s){memset(s,0,sizeof(*s));s->station_enabled=true;s->ap_enabled=ap;s->state=station?CYD_NET_CONNECTED:CYD_NET_OFFLINE;if(station)strcpy(s->station_ip,"192.0.2.4");if(ap)strcpy(s->ap_ip,"192.168.4.1");}
esp_err_t cyd_mkdirs(const char *s){char p[320];snprintf(p,sizeof(p),"%s",s);for(char *c=p+1;*c;c++)if(*c=='/'){*c=0;mkdir(p,0700);*c='/';}return !mkdir(p,0700)||errno==EEXIST?0:ESP_FAIL;}
esp_err_t cyd_write_file(const char *p,const void *v,size_t n){FILE *f=fopen(p,"wb");if(!f)return ESP_FAIL;bool ok=fwrite(v,1,n,f)==n;if(fclose(f))ok=false;return ok?0:ESP_FAIL;}
bool cyd_storage_ready(void){return true;}
void cyd_device_snapshot(cyd_device_status_t *s){memset(s,0,sizeof(*s));s->clock_valid=true;}
esp_err_t cyd_read_file(const char *p,uint8_t **v,size_t *n,size_t max){FILE *f=fopen(p,"rb");if(!f)return ESP_ERR_NOT_FOUND;fseek(f,0,SEEK_END);long size=ftell(f);rewind(f);if(size<0||(size_t)size>max){fclose(f);return ESP_ERR_INVALID_SIZE;}*v=calloc(1,size+1);*n=fread(*v,1,size,f);fclose(f);return *n==(size_t)size?0:ESP_FAIL;}
esp_err_t cyd_credentials_get(const char *a,const char *k,void *v,size_t *n){(void)a;(void)k;(void)v;(void)n;return ESP_ERR_NOT_FOUND;}esp_err_t cyd_credentials_set(const char *a,const char *k,const void *v,size_t n){(void)a;(void)k;(void)v;(void)n;return 0;}esp_err_t cyd_credentials_delete(const char *a,const char *k){(void)a;(void)k;return 0;}
static unsigned char io_input[2048];static size_t io_size,io_offset;static unsigned io_closes;
static void *fake_open(const cyd_endpoint_t *e,char *error,size_t n){(void)e;(void)error;(void)n;return (void *)1;}static int fake_connect(void *p,char *e,size_t n){(void)p;(void)e;(void)n;return 1;}static int fake_read(void *p,void *b,size_t n){(void)p;if(io_offset==io_size)return 0;if(n>io_size-io_offset)n=io_size-io_offset;memcpy(b,io_input+io_offset,n);io_offset+=n;return n;}static int fake_write(void *p,const void *b,size_t n){(void)p;(void)b;return n;}static void fake_close(void *p){(void)p;io_closes++;}
static void feed(const void *p,size_t n){assert(n<=sizeof(io_input));memcpy(io_input,p,n);io_size=n;io_offset=0;}
cyd_io_t cyd_services_esp_io(void){return (cyd_io_t){fake_open,fake_connect,fake_read,fake_write,fake_close};}
static int eval(const char *code){execution_begin(&application);int rc=luaL_loadstring(vm,code);if(!rc)rc=lua_pcall(vm,0,0,0);if(rc){fprintf(stderr,"Lua: %s\n",lua_tostring(vm,-1));lua_pop(vm,1);}return rc;}
static void reset(void){if(vm){application.closing=true;lua_close(vm);cyd_services_close(application.services);free(application.services);if(application.input_overlay)lv_obj_delete(application.input_overlay);lv_obj_delete(application.root);}memset(&application,0,sizeof(application));application.root=lv_obj_create(lv_screen_active());lv_obj_set_size(application.root,screen_width,screen_height-56);application.manifest.storage=true;strcpy(application.manifest.id,"org.cyd.test");strcpy(application.rev,"0000000000000001");io_size=io_offset=0;application.widgets[UI_LIMIT]=(widget_t){.owner=&application,.slot=UI_LIMIT,.generation=1,.object=application.root,.refs={LUA_NOREF,LUA_NOREF,LUA_NOREF}};application.services=calloc(1,sizeof(cyd_services_t));cyd_io_t io=cyd_services_esp_io();cyd_services_init(application.services,&io);application.service_guard=(void *)1;for(unsigned i=0;i<16;i++)application.timer_refs[i]=LUA_NOREF;application.http_ref=LUA_NOREF;for(unsigned i=0;i<2;i++)application.mqtt_refs[i][0]=application.mqtt_refs[i][1]=LUA_NOREF;atomic_store(&cancel,false);vm=lua_newstate(allocator,&application);*(app_t **)lua_getextraspace(vm)=&application;lua_pushcfunction(vm,bind_api);assert(lua_pcall(vm,0,0,0)==0);lua_sethook(vm,hook,LUA_MASKCOUNT,1000);}
static void dispatch(void){while(cyd_services_pop(application.services,&application.delivery)){lua_pushcfunction(vm,dispatch_event);execution_begin(&application);assert(lua_pcall(vm,0,0,0)==0);}}
static uint16_t draw_buffer[800*24];
static void flush(lv_display_t *d,const lv_area_t *area,uint8_t *pixels){(void)pixels;assert(area->x1>=0&&area->y1>=0&&area->x2<(int)screen_width&&area->y2<(int)screen_height);lv_display_flush_ready(d);}
static void render(void){for(unsigned i=0;i<4;i++){lv_tick_inc(20);lv_timer_handler();}}
static void finish(void){application.closing=true;cyd_services_close(application.services);free(application.services);application.services=NULL;lua_close(vm);vm=NULL;if(application.input_overlay)lv_obj_delete(application.input_overlay);lv_obj_delete(application.root);lv_deinit();assert(application.memory==0);const char *files[]={"sample.txt","sample.json","icon.rgb565"};for(unsigned i=0;i<3;i++){char path[256];snprintf(path,sizeof(path),CYD_ROOT "/projects/0000000000000001/%s",files[i]);unlink(path);}rmdir(CYD_ROOT "/projects/0000000000000001");rmdir(CYD_ROOT "/projects");rmdir(CYD_ROOT);}
static void demo(const char *source){const char *files[]={"sample.json","icon.rgb565"};for(unsigned i=0;i<2;i++){char from[512],to[256];snprintf(from,sizeof(from),"%s/%s",source,files[i]);snprintf(to,sizeof(to),CYD_ROOT "/projects/0000000000000001/%s",files[i]);FILE *in=fopen(from,"rb"),*out=fopen(to,"wb");assert(in&&out);int c;while((c=fgetc(in))!=EOF)fputc(c,out);fclose(in);fclose(out);}char path[512];snprintf(path,sizeof(path),"%s/main.lua",source);FILE *f=fopen(path,"rb");assert(f);char code[65537];size_t n=fread(code,1,65536,f);fclose(f);code[n]=0;application.manifest.network=application.manifest.mqtt=application.manifest.credentials=true;assert(eval(code)==0);for(unsigned i=0;i<3;i++)assert(!application.services->conn[i].used);render();cyd_services_tick(application.services,1000);dispatch();render();finish();printf("Generic demo loaded and rendered at %ux%u without automatic network traffic\n",screen_width,screen_height);}
static void release_launch(app_t *a){
 a->closing=true;
 /* The network worker is explicitly simulated as having stopped before join. */
 if(a->worker_started){cyd_services_close(a->services);xSemaphoreGive(a->service_done);}
 services_stop(a);
 if(a->service_guard)vSemaphoreDelete(a->service_guard);
 if(a->service_done)vSemaphoreDelete(a->service_done);
 free(a);xSemaphoreGive(done);
}
static void launch_checks(void){
 lifecycle=true;assert(cyd_lua_integer_bytes()==sizeof(lua_Integer));assert(cyd_lua_number_bytes()==sizeof(lua_Number));assert(cyd_runtime_init()==0);
 cyd_manifest_t manifest={0};strcpy(manifest.id,"org.cyd.offline");
 unsigned calls=host_psram_calls;
 assert(cyd_runtime_start("test.lua","1111111111111111",&manifest)==0);
 assert(host_psram_calls>calls&&lua_tasks==1);app_t *a=created_arg;
 assert(services_start(a));assert(!a->worker_started&&service_tasks==0);
 uint32_t gen;assert(cyd_services_timer(a->services,20,false,&gen)>=0);
 host_clock+=20000;assert(services_next(a));assert(a->delivery.kind==CYD_EV_TIMER);
 assert(cyd_services_touch(a->services,0,1));assert(services_next(a));assert(a->delivery.kind==CYD_EV_TOUCH);
 release_launch(a);
 manifest.network=true;
 assert(cyd_runtime_start("test.lua","2222222222222222",&manifest)==0);a=created_arg;
 assert(services_start(a));assert(a->worker_started&&service_tasks==1);release_launch(a);
 manifest.network=false;manifest.mqtt=true;
 assert(cyd_runtime_start("test.lua","3333333333333333",&manifest)==0);a=created_arg;
 assert(services_start(a));assert(a->worker_started&&service_tasks==2);release_launch(a);
 manifest.mqtt=false;reject_task="lua_app";
 assert(cyd_runtime_start("test.lua","4444444444444444",&manifest)==ESP_ERR_NO_MEM);
 assert(strstr(last_error,"Lua task allocation failed"));assert(strstr(last_error,"internal free="));
 reject_task=NULL;host_psram_fail=1;
 assert(cyd_runtime_start("test.lua","5555555555555555",&manifest)==ESP_ERR_NO_MEM);
 assert(strstr(last_error,"App context allocation failed"));host_psram_fail=0;
 assert(cyd_runtime_start("test.lua","6666666666666666",&manifest)==0);a=created_arg;
 host_psram_fail=1;assert(!services_start(a));host_psram_fail=0;release_launch(a);
 manifest.network=true;reject_task="app_services";
 assert(cyd_runtime_start("test.lua","7777777777777777",&manifest)==0);a=created_arg;
 assert(!services_start(a));assert(!a->worker_started);reject_task=NULL;release_launch(a);
 manifest.network=false;
 assert(cyd_runtime_start("test.lua","8888888888888888",&manifest)==0);a=created_arg;
 assert(services_start(a));assert(!a->worker_started);release_launch(a);
 lifecycle=false;puts("Launch checks passed: PSRAM allocation, offline timers/touch without service task, network workers, failure cleanup and relaunch");
}
int main(int argc,char **argv){if(argc>2){screen_width=atoi(argv[2]);screen_height=screen_width==800?480:240;}mkdir(CYD_ROOT,0700);mkdir(CYD_ROOT "/projects",0700);mkdir(CYD_ROOT "/projects/0000000000000001",0700);FILE *resource=fopen(CYD_ROOT "/projects/0000000000000001/sample.txt","wb");assert(resource);fputs("packaged resource",resource);fclose(resource);unsigned char pixels[]={'C','I','M','G',2,0,2,0,0,0,255,255,0,0,255,255};resource=fopen(CYD_ROOT "/projects/0000000000000001/icon.rgb565","wb");assert(resource);fwrite(pixels,1,sizeof(pixels),resource);fclose(resource);lv_init();lv_display_t *display=lv_display_create(screen_width,screen_height);lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(display,draw_buffer,NULL,sizeof(draw_buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(display,flush);reset();if(argc>1){demo(argv[1]);return 0;}
 assert(eval("assert(cyd.ui.create_screen().__index==nil)")==0);
 assert(eval("local s=cyd.ui.create_screen();label=s:add_label{text='old API'};s:add_button{text='OK',on_click=function()label:set_text('clicked')end};cyd.ui.show(s)")==0);
 widget_t *button_record=&application.widgets[1];lv_obj_send_event(button_record->object,LV_EVENT_CLICKED,NULL);dispatch();assert(!strcmp(lv_label_get_text(application.widgets[0].object),"clicked"));
 assert(eval("label:set_color(0x00ff00);progress=cyd.ui.create_screen():add_bar{value=2};progress:set_value(73);box=cyd.ui.create_screen():add_container{scroll=true,layout='column'};entry=box:add_input{masked=true,text='secret'};assert(entry:get_text()=='secret');entry:set_text('new');assert(entry:get_text()=='new');view=cyd.ui.create_screen():add_view{};cyd.ui.show(view)")==0);assert(lv_bar_get_value(application.widgets[2].object)==73);
 assert(eval("submitted='';entry2=cyd.ui.create_screen():add_input{masked=true,text='first',on_submit=function(s)submitted=s end}")==0);
 widget_t *editing=NULL;for(unsigned i=0;i<UI_LIMIT;i++)if(application.widgets[i].object&&lv_obj_check_type(application.widgets[i].object,&lv_textarea_class)&&!strcmp(lv_textarea_get_text(application.widgets[i].object),"first"))editing=&application.widgets[i];assert(editing);lv_obj_send_event(editing->object,LV_EVENT_CLICKED,NULL);assert(application.input_overlay);lv_obj_update_layout(application.input_overlay);lv_area_t overlay;lv_obj_get_coords(application.input_overlay,&overlay);assert(overlay.y2<184);input_text(application.input_edit,"second");lv_obj_send_event(lv_obj_get_child(application.input_overlay,-1),LV_EVENT_READY,NULL);dispatch();assert(eval("assert(submitted=='second' and entry2:get_text()=='second')")==0);
 assert(eval("box:delete();entry:get_text()")!=0);
 assert(eval("cyd.network.info()")!=0);application.manifest.network=true;station=false;ap=true;assert(eval("local n=cyd.network.info();assert(n.ap_enabled and not n.station_connected and n.station_ip=='')")==0);station=true;assert(eval("assert(cyd.network.info().station_connected)")==0);
 assert(eval("cyd.network.request({url='https://localhost',insecure=true},function()end)")!=0);
 assert(eval("cyd.mqtt.connect{url='mqtt://127.0.0.1'}")!=0);assert(eval("cyd.credentials.get('token')")!=0);
 assert(eval("local value,error=cyd.json.decode('['..string.rep('0,',300)..'0]');assert(value==nil and error)")==0);
 assert(eval("local t=cyd.json.decode('{\"x\":[1,true,null]}');assert(t.x[1]==1 and t.x[2] and t.x[3]==cyd.json.null)")==0);
 assert(eval("assert(cyd.json.decode('{\"x\":1,\"x\":2}')==nil)")==0);assert(eval("assert(cyd.json.decode('1e999')==nil)")==0);assert(eval("assert(cyd.json.decode(string.rep('[',20)..'0'..string.rep(']',20))==nil)")==0);
 assert(eval("count=0;timer=cyd.timer.every(20,function()count=count+1 end)")==0);cyd_services_tick(application.services,20);cyd_services_tick(application.services,2000);dispatch();assert(eval("assert(count==1);assert(timer:cancel())")==0);cyd_services_tick(application.services,4000);dispatch();assert(eval("assert(count==1)")==0);
 assert(eval("assert(cyd.resources.read('sample.txt')=='packaged resource');assert(cyd.resources.read('missing.txt')==nil);image=cyd.ui.create_screen():add_image{resource='icon.rgb565'}")==0);assert(application.images==1);render();assert(eval("image:delete()")==0);assert(application.images==0);render();assert(eval("cyd.resources.read('../another/secret')")!=0);
 assert(eval("request_done=0;request=cyd.network.request({url='http://localhost/test'},function(r)assert(r.ok and r.status==200 and r.body=='hello');request_done=request_done+1 end)")==0);assert(eval("local busy,error=cyd.network.request({url='http://localhost/other'},function()end);assert(busy==nil and error)")==0);const char *response="HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello";feed(response,strlen(response));cyd_services_tick(application.services,4100);cyd_services_tick(application.services,4101);dispatch();assert(eval("assert(request_done==1 and request.__index==nil)")==0);
 application.manifest.mqtt=true;assert(eval("messages=0;states=0;client=cyd.mqtt.connect{url='mqtt://localhost',on_message=function(topic,body)assert(topic=='x' and body=='abc');messages=messages+1 end,on_state=function(s)assert(s=='connected');states=states+1 end};assert(client:subscribe('x'))")==0);const unsigned char ack[]={0x20,2,0,0};feed(ack,sizeof(ack));cyd_services_tick(application.services,4200);dispatch();assert(eval("assert(states==1 and client:state()=='connected')")==0);const unsigned char message[]={0x30,6,0,1,'x','a','b','c'};feed(message,sizeof(message));cyd_services_tick(application.services,4201);dispatch();assert(eval("assert(messages==1);old=client;assert(client:disconnect());client=cyd.mqtt.connect{url='mqtt://localhost'};assert(not old:publish('x','stale'));assert(not old:disconnect())")==0);
 assert(eval("setmetatable({}, {__gc=function()while true do end end})")!=0);assert(eval("local meta={__index={answer=42}};local obj=setmetatable({},meta);meta.__gc=function()while true do end end;assert(obj.answer==42);assert(type(getmetatable(obj))=='string' and type(getmetatable(''))=='string')")==0);
 assert(eval("function recurse()return string.gsub('x','x',recurse)end;recurse()")!=0);
 assert(eval("assert(pcall==nil and coroutine==nil and os==nil and io==nil and debug==nil);while true do end")!=0);
 host_clock=5000000;
 assert(eval("offline_timer_fired=0;cyd.timer.after(100,function()offline_timer_fired=offline_timer_fired+1 end)")==0);
 assert(!services_next(&application));host_clock+=99000;assert(!services_next(&application));
 host_clock+=1000;assert(services_next(&application));
 lua_pushcfunction(vm,dispatch_event);execution_begin(&application);
 assert(lua_pcall(vm,0,0,0)==0);assert(eval("assert(offline_timer_fired==1)")==0);
 reset();mkdir(CYD_ROOT "/data",0700);mkdir(CYD_ROOT "/data/org.cyd.test",0700);resource=fopen(CYD_ROOT "/data/org.cyd.test/large.txt","wb");assert(resource);for(unsigned i=0;i<16384;i++)fputc('x',resource);fclose(resource);
 assert(eval("assert(#cyd.storage.read('large.txt')==16384)")==0);lua_gc(vm,LUA_GCCOLLECT,0);
 size_t reserved=LUA_LIMIT-application.memory-2048;application.memory+=reserved;
 assert(eval("cyd.storage.read('large.txt')")!=0);application.memory-=reserved;lua_gc(vm,LUA_GCCOLLECT,0);
 unlink(CYD_ROOT "/data/org.cyd.test/large.txt");rmdir(CYD_ROOT "/data/org.cyd.test");rmdir(CYD_ROOT "/data");
 reset();assert(eval("local x={};for i=1,200000 do x[i]=string.rep('x',100) end")!=0);
 finish();launch_checks();puts("Real Lua/LVGL API compatibility, permissions, Wi-Fi, UI lifetime, JSON, timers and sandbox checks passed");return 0;}
