/* Actual Apps UI with simulated catalog/RTOS/point input. No device or SD access. */
#include "cyd.h"
#include "cyd_projects.h"
const char *esp_err_to_name(esp_err_t);
#include "../components/ui/projects.c"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
static cJSON *catalog;
static char last_run[17], last_operation[24];
static unsigned width, height, clicks;
static uint16_t framebuffer[800*800], draw_buffer[800*24];
static lv_point_t point;
static bool pressed;
struct host_queue { unsigned head,count; project_job_t items[8]; };
QueueHandle_t xQueueCreate(unsigned n,size_t size){assert(n==8&&size==sizeof(project_job_t));return calloc(1,sizeof(struct host_queue));}
int xQueueSend(QueueHandle_t q,const void *p,unsigned ticks){(void)ticks;if(q->count==8)return 0;q->items[(q->head+q->count++)%8]=*(const project_job_t *)p;return pdTRUE;}
int xQueueReceive(QueueHandle_t q,void *p,unsigned ticks){(void)ticks;if(!q->count)return 0;*(project_job_t *)p=q->items[q->head];q->head=(q->head+1)%8;q->count--;return pdTRUE;}
int xTaskCreate(void (*f)(void *),const char *name,unsigned size,void *arg,unsigned priority,void *handle){(void)f;(void)arg;(void)priority;(void)handle;assert(!strcmp(name,"app_folders")&&size==8192);return pdPASS;}
void cyd_ui_lock(void){}void cyd_ui_unlock(void){}
void cyd_log(const char *tag,const char *message){fprintf(stderr,"%s: %s\n",tag,message);}
const char *esp_err_to_name(esp_err_t e){return e?"error":"ESP_OK";}
cJSON *cyd_projects_catalog(void){return cJSON_Duplicate(catalog,true);}
esp_err_t cyd_run_revision(const char *revision){snprintf(last_run,sizeof(last_run),"%s",revision);return ESP_OK;}
static cJSON *folder(const char *id){cJSON *v;cJSON_ArrayForEach(v,cJSON_GetObjectItem(catalog,"folders"))if(!strcmp(project_str(v,"id"),id))return v;return NULL;}
esp_err_t cyd_project_folder_create(const char *name,char id[9]){strcpy(last_operation,"create");strcpy(id,"feed1234");cJSON *o=cJSON_CreateObject();cJSON_AddStringToObject(o,"id",id);cJSON_AddStringToObject(o,"name",name);cJSON_AddItemToArray(cJSON_GetObjectItem(catalog,"folders"),o);return 0;}
esp_err_t cyd_project_folder_rename(const char *id,const char *name){strcpy(last_operation,"rename");cJSON *f=folder(id);assert(f);cJSON_ReplaceItemInObject(f,"name",cJSON_CreateString(name));return 0;}
esp_err_t cyd_project_folder_delete(const char *id){strcpy(last_operation,"delete");cJSON *folders=cJSON_GetObjectItem(catalog,"folders"),*v;int index=0;cJSON_ArrayForEach(v,folders){if(!strcmp(project_str(v,"id"),id)){cJSON_DeleteItemFromArray(folders,index);break;}index++;}cJSON_ArrayForEach(v,cJSON_GetObjectItem(catalog,"projects"))if(!strcmp(project_str(v,"folder"),id))cJSON_ReplaceItemInObject(v,"folder",cJSON_CreateString(""));return 0;}
esp_err_t cyd_project_move(const char *id,const char *destination){strcpy(last_operation,"move");cJSON *v;cJSON_ArrayForEach(v,cJSON_GetObjectItem(catalog,"projects"))if(!strcmp(project_str(v,"package_id"),id)){cJSON_ReplaceItemInObject(v,"folder",cJSON_CreateString(destination));return 0;}return ESP_FAIL;}
static void read_point(lv_indev_t *input,lv_indev_data_t *data){(void)input;data->point=point;data->state=pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;}
static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *pixels){assert(area->x1>=0&&area->y1>=0&&area->x2<(int)width&&area->y2<(int)height);uint16_t *p=(uint16_t *)pixels;for(int y=area->y1;y<=area->y2;y++)for(int x=area->x1;x<=area->x2;x++)framebuffer[y*width+x]=*p++;lv_display_flush_ready(display);}
static void render(void){for(unsigned i=0;i<4;i++){lv_tick_inc(20);lv_timer_handler();}}
static void jobs(void){project_job_t job;while(xQueueReceive(project_jobs,&job,0))process_project_job(&job);render();}
static lv_obj_t *find(lv_obj_t *root,const char *text){if(lv_obj_check_type(root,&lv_label_class)&&!lv_obj_has_flag(root,LV_OBJ_FLAG_HIDDEN)&&!strcmp(lv_label_get_text(root),text)&&lv_obj_check_type(lv_obj_get_parent(root),&lv_button_class))return lv_obj_get_parent(root);for(unsigned i=0;i<lv_obj_get_child_count(root);i++){lv_obj_t *found=find(lv_obj_get_child(root,i),text);if(found)return found;}return NULL;}
static bool text_present(lv_obj_t *root,const char *text){if(lv_obj_check_type(root,&lv_label_class)&&!strcmp(lv_label_get_text(root),text))return true;for(unsigned i=0;i<lv_obj_get_child_count(root);i++)if(text_present(lv_obj_get_child(root,i),text))return true;return false;}
static void click(const char *text){lv_obj_t *button=find(project_view,!strcmp(text,"Back")?"Up":text);if(!button){fprintf(stderr,"Missing: %s\n",text);abort();}cyd_pager_reveal(project_body,button);render();lv_obj_send_event(button,LV_EVENT_CLICKED,NULL);clicks++;render();}
static void dump_labels(lv_obj_t *root){if(lv_obj_check_type(root,&lv_label_class))fprintf(stderr,"label: %s (hidden=%d)\n",lv_label_get_text(root),lv_obj_has_flag(root,LV_OBJ_FLAG_HIDDEN));for(unsigned i=0;i<lv_obj_get_child_count(root);i++)dump_labels(lv_obj_get_child(root,i));}
static void pointer_icon(const char *name){lv_obj_t *button=find(project_view,name);if(!button){fprintf(stderr,"Missing icon: %s; page=%d folder=%s\n",name,(int)project_page,current_folder);dump_labels(project_view);}assert(button);cyd_pager_reveal(project_body,button);render();lv_obj_t *icon=lv_obj_get_child(button,0);assert(!lv_obj_has_flag(icon,LV_OBJ_FLAG_CLICKABLE));lv_obj_scroll_to_view(button,LV_ANIM_OFF);render();lv_area_t box;lv_obj_get_coords(icon,&box);point=(lv_point_t){(box.x1+box.x2)/2,(box.y1+box.y2)/2};pressed=true;render();pressed=false;render();clicks++;}
static void check_grid(void){lv_obj_update_layout(project_view);assert(lv_obj_get_style_flex_flow(project_body,0)==LV_FLEX_FLOW_ROW_WRAP);lv_area_t content;lv_obj_get_content_coords(project_body,&content);unsigned count=0;for(unsigned i=0;i<lv_obj_get_child_count(project_body);i++){lv_obj_t *child=lv_obj_get_child(project_body,i);if(!lv_obj_check_type(child,&lv_button_class)||lv_obj_has_flag(child,LV_OBJ_FLAG_HIDDEN))continue;count++;lv_area_t a;lv_obj_get_coords(child,&a);assert(a.x1>=content.x1&&a.x2<=content.x2);assert(lv_obj_get_width(child)<(int)project_width);assert(lv_obj_get_height(child)>=80);assert(lv_obj_get_child_count(child)>=3);}assert(count>0);}
static void save(const char *folder,const char *name){render();char path[512];snprintf(path,sizeof(path),"%s/apps-%s-%ux%u.ppm",folder,name,width,height);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",width,height);for(unsigned i=0;i<width*height;i++){uint16_t p=framebuffer[i];unsigned char rgb[]={(p>>11)*255/31,((p>>5)&63)*255/63,(p&31)*255/31};fwrite(rgb,1,3,f);}fclose(f);}
static void edit_name(const char *name){assert(project_editor);lv_textarea_set_text(project_editor,name);lv_obj_t *keyboard=lv_obj_get_child(project_view,-1);lv_obj_send_event(keyboard,LV_EVENT_READY,NULL);render();jobs();}
int main(int argc,char **argv){assert(argc==4);width=atoi(argv[1]);height=atoi(argv[2]);lv_init();lv_display_t *display=lv_display_create(width,height);lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(display,draw_buffer,NULL,sizeof(draw_buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(display,flush);lv_theme_default_init(display,lv_palette_main(LV_PALETTE_BLUE),lv_palette_main(LV_PALETTE_CYAN),true,LV_FONT_DEFAULT);lv_indev_t *input=lv_indev_create();lv_indev_set_type(input,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(input,read_point);
 catalog=cJSON_Parse("{\"folders\":[{\"id\":\"1234abcd\",\"name\":\"Tools\"},{\"id\":\"8765abcd\",\"name\":\"Games\"},{\"id\":\"aaaabbbb\",\"name\":\"Daily\"},{\"id\":\"deadbeef\",\"name\":\"Empty folder\"}],\"projects\":[{\"package_id\":\"com.cydos.calculator\",\"name\":\"Calculator\",\"folder\":\"\",\"versions\":[{\"version\":\"1.0.0\",\"revision\":\"cccccccccccccccc\"}]},{\"package_id\":\"com.example.calendar\",\"name\":\"Calendar\",\"folder\":\"\",\"versions\":[{\"version\":\"1.0.0\",\"revision\":\"bbbbbbbbbbbbbbbb\"}]},{\"package_id\":\"com.example.notes\",\"name\":\"Notes\",\"folder\":\"\",\"versions\":[{\"version\":\"1.0.0\",\"revision\":\"aaaaaaaaaaaaaaaa\"},{\"version\":\"1.1.0\",\"revision\":\"dddddddddddddddd\"}]},{\"package_id\":\"com.example.long\",\"name\":\"A very long application name for wrapping\",\"folder\":\"\",\"versions\":[{\"version\":\"1.0.0\",\"revision\":\"eeeeeeeeeeeeeeee\"}]},{\"package_id\":\"org.example.hello\",\"name\":\"Hello\",\"folder\":\"1234abcd\",\"versions\":[{\"version\":\"1.0.0\",\"revision\":\"1111111111111111\"},{\"version\":\"1.1.0\",\"revision\":\"2222222222222222\"}]}]}");assert(catalog);
 lv_obj_set_style_bg_color(lv_screen_active(),lv_color_hex(0x101216),0);assert(cyd_projects_ui_init(lv_screen_active(),width,height)==0);lv_obj_t *home=lv_button_create(lv_screen_active());lv_obj_set_pos(home,(width-200)/2,height-50);lv_obj_set_size(home,200,44);lv_obj_set_style_bg_color(home,lv_color_hex(0x29292c),0);lv_obj_t *label=lv_label_create(home);lv_label_set_text(label,"Home / Stop app");lv_obj_center(label);
 cyd_projects_ui_open();jobs();check_grid();save(argv[3],"root");
 pointer_icon("Tools");assert(!strcmp(current_folder,"1234abcd"));check_grid();save(argv[3],"folder");
 click(LV_SYMBOL_SETTINGS);assert(project_page==FOLDER_ACTIONS);click("Rename folder");edit_name("Renamed Tools");assert(!strcmp(last_operation,"rename"));assert(!strcmp(folder_name("1234abcd"),"Renamed Tools"));
 click(LV_SYMBOL_SETTINGS);click("Delete folder (keep apps)");click("Cancel");assert(folder("1234abcd"));
 click(LV_SYMBOL_SETTINGS);click("Delete folder (keep apps)");click("Delete folder");jobs();assert(!folder("1234abcd")&&!current_folder[0]);assert(find(project_body,"Hello"));
 click(LV_SYMBOL_PLUS);edit_name("Fresh");assert(!strcmp(last_operation,"create"));assert(folder("feed1234"));click("Back");
 pointer_icon("Calculator");assert(project_page==PROJECT);save(argv[3],"versions");click("Move app to folder");assert(project_page==MOVE);check_grid();click("Fresh");jobs();assert(!strcmp(last_operation,"move"));assert(!strcmp(project_str(find_project(),"folder"),"feed1234"));
 click("Back");click("Back");pointer_icon("Fresh");pointer_icon("Calculator");click("1.0.0 / cccccccc");jobs();assert(!strcmp(last_run,"cccccccccccccccc"));assert(lv_obj_has_flag(project_view,LV_OBJ_FLAG_HIDDEN));
 cyd_projects_ui_open();jobs();click("Empty folder");assert(!folder_apps("deadbeef"));assert(text_present(project_body,"No apps here yet. Upload an app, or move an app into this folder."));click("Back");
 click(LV_SYMBOL_REFRESH);cyd_projects_ui_close();jobs();assert(lv_obj_has_flag(project_view,LV_OBJ_FLAG_HIDDEN));
 cyd_projects_ui_open();jobs();
 unsigned old_width=width,old_height=height;width=old_height;height=old_width;
 lv_display_set_resolution(display,width,height);cyd_projects_ui_resize(width,height);render();check_grid();
 width=old_width;height=old_height;lv_display_set_resolution(display,width,height);
 cyd_projects_ui_resize(width,height);render();check_grid();
 click(LV_SYMBOL_REFRESH);click(LV_SYMBOL_PLUS);jobs();assert(project_page==EDIT&&project_editor);
 lv_obj_send_event(lv_obj_get_child(project_view,-1),LV_EVENT_CANCEL,NULL);render();assert(project_page==BROWSE);
 cJSON_Delete(catalog);cyd_projects_ui_close();free(project_jobs);lv_deinit();printf("PASS %ux%u: %u actions; grid bounds, icon hit-testing, rename/delete/create/move/run, empty folder and stale refresh\n",width,height,clicks);return 0;}
