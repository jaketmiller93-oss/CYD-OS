/* Actual Lua runtime + LVGL. Physical drivers/RTOS are explicitly doubled. */
#define main platform_legacy_main
#include "runtime_host.c"
#undef main
#include <dirent.h>
static uint16_t frame[800*800];
static void preview_flush(lv_display_t *d,const lv_area_t *a,uint8_t *p) {
    uint16_t *v=(uint16_t *)p;
    assert(a->x1>=0&&a->y1>=0&&a->x2<(int)screen_width&&a->y2<(int)screen_height);
    for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)frame[y*screen_width+x]=*v++;
    lv_display_flush_ready(d);
}
static void clear_tree(const char *p) {
    struct stat st;if(lstat(p,&st))return;
    if(!S_ISDIR(st.st_mode)){unlink(p);return;}
    DIR *d=opendir(p);assert(d);struct dirent *e;
    while((e=readdir(d))){if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;
        char q[512];snprintf(q,sizeof(q),"%s/%s",p,e->d_name);clear_tree(q);}
    closedir(d);rmdir(p);
}
static widget_t *find_button(const char *text) {
    for(unsigned i=0;i<UI_LIMIT;i++){
        widget_t *w=&application.widgets[i];
        if(w->object&&lv_obj_check_type(w->object,&lv_button_class)&&w->label&&
            !strcmp(lv_label_get_text(w->label),text))return w;
    }
    return NULL;
}
static void click(const char *text) {
    widget_t *b=find_button(text);
    for(int i=0;!b&&i<64;i++){
        widget_t *next=find_button("Next");if(!next)break;
        lv_obj_send_event(next->object,LV_EVENT_CLICKED,NULL);dispatch();render();b=find_button(text);
    }
    if(!b)fprintf(stderr,"Missing button: %s\n",text);
    assert(b);lv_obj_send_event(b->object,LV_EVENT_CLICKED,NULL);dispatch();render();
}
static void fill(const char *text) {
    unsigned found=0;widget_t *target=NULL;
    for(unsigned i=0;i<UI_LIMIT;i++)if(application.widgets[i].object&&
        lv_obj_check_type(application.widgets[i].object,&lv_textarea_class)){
        target=&application.widgets[i];found++;
    }
    assert(found==1);
    lv_obj_send_event(target->object,LV_EVENT_CLICKED,NULL);
    assert(application.input_overlay&&application.input_edit);
    input_text(application.input_edit,text);
    lv_obj_send_event(lv_obj_get_child(application.input_overlay,-1),LV_EVENT_READY,NULL);
    dispatch();render();
}
static bool has_text(const char *s) {
    for(unsigned i=0;i<UI_LIMIT;i++){
        widget_t *w=&application.widgets[i];
        if(w->object&&lv_obj_check_type(w->object,&lv_label_class)&&strstr(lv_label_get_text(w->object),s))return true;
    }
    return false;
}
static void save_preview(const char *folder,const char *name) {
    render();char p[512];snprintf(p,sizeof(p),"%s/%s-%ux%u.ppm",folder,name,screen_width,screen_height);
    FILE *f=fopen(p,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",screen_width,screen_height);
    for(unsigned i=0;i<screen_width*screen_height;i++){
        unsigned c=frame[i];unsigned char rgb[]={((c>>11)&31)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};
        fwrite(rgb,1,3,f);
    }
    fclose(f);
}
static char *source(const char *folder) {
    char path[512];snprintf(path,sizeof(path),"%s/main.lua",folder);FILE *f=fopen(path,"rb");assert(f);
    char *code=calloc(1,65537);assert(code);size_t n=fread(code,1,65536,f);assert(!ferror(f)&&n<65536);fclose(f);return code;
}
static void write_test_file(const char *name,const char *s) {
    assert(!cyd_write_file(name,s,strlen(s)));
}
static void api_checks(void) {
    assert(eval("cyd.documents.list()")!=0);
    application.manifest.documents=true;
    assert(eval("assert(cyd.time.now().valid and cyd.time.now().year>=2024)")==0);
    assert(eval("local t,cut=cyd.ui.text_lines(string.rep('é',100),100);assert(not cut and table.concat(t)==string.rep('é',100));local many,truncated=cyd.ui.text_lines(string.rep('a\\n',600),100);assert(#many==512 and truncated)")==0);
    host_calloc_fail=1;
    assert(eval("local t,e=cyd.documents.list();assert(t==nil and e)")==0);
    host_calloc_fail=0;
    assert(eval("assert(cyd.documents.write('../data/leak','x')==nil);assert(cyd.documents.read('/etc/passwd')==nil);assert(cyd.documents.write('UPPER.TXT','x')==nil)")==0);
    assert(eval("assert(cyd.documents.write('a.txt','old'));assert(cyd.documents.write('a.txt','bad')==nil);assert(cyd.documents.read('a.txt')=='old');assert(cyd.documents.write('a.txt','new',true));assert(cyd.documents.read('a.txt')=='new');assert(cyd.documents.rename('a.txt','b.txt'));local a=cyd.documents.list();assert(#a==1 and a[1].name=='b.txt' and a[1].size==3)")==0);
    write_test_file(CYD_ROOT "/secret.txt","PRIVATE");assert(!symlink(CYD_ROOT "/secret.txt",DOC_ROOT "/link.txt"));
    assert(eval("assert(cyd.documents.read('link.txt')==nil);assert(cyd.documents.write('link.txt','bad',true)==nil);assert(cyd.documents.delete('link.txt')==nil)")==0);
    write_test_file(DOC_ROOT "/.old-recover.txt","old data");write_test_file(DOC_ROOT "/.new-recover.txt","uncommitted");
    assert(eval("assert(cyd.documents.read('recover.txt')=='old data')")==0);
    write_test_file(DOC_ROOT "/.old-recover.txt","older");
    assert(eval("assert(cyd.documents.read('recover.txt')=='old data')")==0);
    clear_tree(DOC_ROOT);
    assert(eval("for i=1,64 do assert(cyd.documents.write(string.format('f%02d.txt',i),'x')) end;assert(cyd.documents.write('overflow.txt','x')==nil);assert(#cyd.documents.list()==64);assert(cyd.documents.write('f01.txt',string.rep('x',16385),true)==nil)")==0);
    assert(eval("assert(cyd.documents.write('f01.txt',string.rep('x',16384),true))")==0);
    lua_gc(vm,LUA_GCCOLLECT,0);size_t reserved=LUA_LIMIT-application.memory-2048;application.memory+=reserved;
    assert(eval("cyd.documents.read('f01.txt')")!=0);application.memory-=reserved;lua_gc(vm,LUA_GCCOLLECT,0);
    puts("PASS real Lua Documents permissions, paths, symlinks, overwrite, rename, quota and interrupted-save recovery");
}
int main(int argc,char **argv) {
    assert(argc==6);screen_width=atoi(argv[2]);screen_height=atoi(argv[3]);
    const char *delay=getenv("CYD_UI_DELAY_US");host_ui_delay_us=delay?strtoul(delay,NULL,10):0;
    clear_tree(CYD_ROOT);assert(!cyd_mkdirs(CYD_ROOT "/projects/0000000000000001"));
    lv_init();lv_display_t *d=lv_display_create(screen_width,screen_height);
    lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d,draw_buffer,NULL,sizeof(draw_buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d,preview_flush);lv_obj_set_style_bg_color(lv_screen_active(),lv_color_hex(0x090d14),0);
    reset();lv_obj_set_pos(application.root,0,34);lv_obj_set_size(application.root,screen_width,screen_height-88);
    lv_obj_set_style_pad_all(application.root,12,0);lv_obj_remove_flag(application.root,LV_OBJ_FLAG_SCROLLABLE);
    if(!strcmp(argv[4],"api")){api_checks();finish();clear_tree(CYD_ROOT);return 0;}
    application.manifest.documents=strcmp(argv[4],"clock")&&strcmp(argv[4],"calculator");
    snprintf(application.manifest.id,sizeof(application.manifest.id),"com.cydos.%s",argv[4]);
    char *code=source(argv[1]);assert(eval(code)==0);render();save_preview(argv[5],argv[4]);
    if (getenv("CYD_ESSENTIALS_LAUNCH_ONLY")) {
        free(code); finish(); clear_tree(CYD_ROOT);
        puts("PASS essentials launch with slow native UI"); return 0;
    }
    if(!strcmp(argv[4],"notes")||!strcmp(argv[4],"contacts")){
        bool notes=!strcmp(argv[4],"notes");click("New");fill(notes?"Bench note":"Sensor supplier");click("Next");fill(notes?"Wire IO8 to C5 RX. A saved note.":"123-456");
        if(!notes){click("Next");fill("parts@example.invalid");click("Next");fill("Offline contact");}
        click("Save");click(notes?"Bench note":"Sensor supplier");save_preview(argv[5],notes?"notes-detail":"contacts-detail");
        click("Actions");click("Export to Documents");assert(has_text("Exported:"));
        click("Back");assert(find_button(notes?"Bench note":"Sensor supplier"));
        // Relaunch reads the saved two-slot state; no hidden table retained.
        reset();lv_obj_set_pos(application.root,0,34);lv_obj_set_size(application.root,screen_width,screen_height-88);lv_obj_set_style_pad_all(application.root,12,0);
        application.manifest.documents=true;snprintf(application.manifest.id,sizeof(application.manifest.id),"com.cydos.%s",argv[4]);
        assert(eval(code)==0);assert(find_button(notes?"Bench note":"Sensor supplier"));
        click(notes?"Bench note":"Sensor supplier");click("Actions");click("Delete");click("Delete");assert(!find_button(notes?"Bench note":"Sensor supplier"));
    }else if(!strcmp(argv[4],"calendar")){
        click("Events");click("New");fill("2028-02-30");click("Next");fill("Leap day test");click("Next");fill("09:30");click("Next");fill("Check leap-year validation");click("Save");assert(has_text("valid date"));
        click("Previous");click("Previous");click("Previous");fill("2028-02-29");click("Next");click("Next");click("Next");click("Save");click("Leap day test");save_preview(argv[5],"calendar-event");
    }else if(!strcmp(argv[4],"files")){
        click("New");fill("bench.txt");click("Save");fill("A shared document.");click("Save");
        click("bench.txt (18 B)");click("Actions");click("Edit text");fill("Updated");click("Save");click("bench.txt (7 B)");click("Actions");click("Rename");fill("renamed.txt");click("Save");click("Back");save_preview(argv[5],"files-saved");
        click("renamed.txt (7 B)");click("Actions");click("Delete");click("Delete");assert(has_text("Deleted"));
        assert(eval("assert(cyd.documents.write('long.txt',string.rep('sensor value ',1000)))")==0);
        click("New");click("Back");click("long.txt (13000 B)");
        unsigned bytes_seen=0,pages=0;
        for(;;){
            bool found=false;
            for(unsigned i=0;i<UI_LIMIT;i++){
                widget_t *w=&application.widgets[i];
                if(w->object&&lv_obj_check_type(w->object,&lv_label_class)&&lv_obj_get_y(w->object)==(screen_width>=480?42:28)){
                    const char *s=lv_label_get_text(w->object);assert(strlen(s)<=1000);
                    for(;*s;s++)if(*s!='\n')bytes_seen++;found=true;
                }
            }
            assert(found&&++pages<512);if(!find_button("Next"))break;click("Next");
        }
        assert(bytes_seen==13000);
    }else if(!strcmp(argv[4],"clock")){
        click("Clock");save_preview(argv[5],"clock-time");click("Back");click("Countdown timer");click("Set seconds");fill("2");click("Set");click("Start");
        host_clock+=3000000;cyd_services_tick(application.services,host_clock/1000);dispatch();render();assert(has_text("TIMER FINISHED")&&find_button("Again"));save_preview(argv[5],"timer-alert");
        click("Back");click("Stopwatch");click("Start");host_clock+=4000000;cyd_services_tick(application.services,host_clock/1000);dispatch();click("Pause");assert(has_text("00:00:04"));
        const char *fixed_time="cyd.time.now=function()return {valid=true,year=2026,month=11,day=15,hour=12,minute=34,second=0,clock_12h=false}end";
        assert(eval(fixed_time)==0);click("Back");click("Daily alarm (while open)");fill("25:99");click("Save");assert(has_text("Enter HH:MM"));fill("12:34");click("Save");assert(has_text("ALARM: 12:34"));
        reset();lv_obj_set_pos(application.root,0,34);lv_obj_set_size(application.root,screen_width,screen_height-88);lv_obj_set_style_pad_all(application.root,12,0);strcpy(application.manifest.id,"com.cydos.clock");
        assert(eval(fixed_time)==0);assert(eval(code)==0);assert(!has_text("ALARM: 12:34"));
    }else if(!strcmp(argv[4],"calculator")){
        click("7");click("+");click("8");click("=");assert(has_text("15"));save_preview(argv[5],"calculator-result");
        click("/");click("0");click("=");assert(has_text("Divide by 0"));
    }
    free(code);assert(application.objects<=UI_LIMIT);finish();clear_tree(CYD_ROOT);
    puts("PASS essentials app interaction, persistence, pagination and cleanup");return 0;
}
