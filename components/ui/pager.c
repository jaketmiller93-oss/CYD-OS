#include "pager.h"
#include <string.h>
typedef struct {
    lv_obj_t *source,*box;
    bool enabled;
    unsigned first,count;
    int x,y,h,chunk;
} item_t;
typedef struct {
    unsigned current,pages,count;
    bool grid;
    item_t items[];
} pager_t;
static void hide(lv_obj_t *o,bool hidden) {
    if(hidden==lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN))return;
    if(hidden)lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);else lv_obj_remove_flag(o,LV_OBJ_FLAG_HIDDEN);
}
static void destroyed(lv_event_t *e) {
    lv_obj_t *root=lv_event_get_target(e);
    lv_free(lv_obj_get_user_data(root));lv_obj_set_user_data(root,NULL);
}
void cyd_pager_reset(lv_obj_t *root) {
    if(!root || !lv_obj_get_user_data(root))return;
    pager_t *p=lv_obj_get_user_data(root);
    for(unsigned n=0;n<p->count;n++) {
        item_t *i=&p->items[n];
        if(i->box!=i->source){lv_obj_set_parent(i->source,root);lv_obj_delete(i->box);}
        lv_obj_move_to_index(i->source,n);
        hide(i->source,!i->enabled);
        if(lv_obj_check_type(i->source,&lv_label_class))lv_obj_set_width(i->source,LV_PCT(100));
    }
    lv_obj_remove_event_cb(root,destroyed);
    lv_free(lv_obj_get_user_data(root));lv_obj_set_user_data(root,NULL);
}
static int text_height(lv_obj_t *label,int width) {
    lv_point_t size;
    lv_text_get_size(&size,lv_label_get_text(label),lv_obj_get_style_text_font(label,0),
        lv_obj_get_style_text_letter_space(label,0),lv_obj_get_style_text_line_space(label,0),
        width,LV_TEXT_FLAG_NONE);
    return size.y;
}
void cyd_pager_refresh(lv_obj_t *root) {
    if(!root)return;
    pager_t *p=lv_obj_get_user_data(root);if(!p)return;
    lv_obj_update_layout(root);
    int left=0,top=0;
    int w=lv_obj_get_content_width(root),h=lv_obj_get_content_height(root);
    if(w<1||h<1)return;
    int gx=lv_obj_get_style_pad_column(root,0),gy=lv_obj_get_style_pad_row(root,0);
    unsigned page=0,last=0;int x=0,y=0,row=0;
    for(unsigned n=0;n<p->count;n++) {
        item_t *i=&p->items[n];
        if(!i->enabled && !lv_obj_has_flag(i->box,LV_OBJ_FLAG_HIDDEN))i->enabled=true;
        if(!i->enabled)continue;
        bool label=lv_obj_check_type(i->source,&lv_label_class);
        int iw=lv_obj_get_width(i->source);
        if(label){iw=w;lv_obj_set_width(i->source,w);}
        int ih=label?text_height(i->source,iw):lv_obj_get_height(i->source);
        if(ih<1)ih=1;
        if(p->grid && x && x+iw>w){y+=row+gy;x=row=0;}
        if(!p->grid && label && !lv_obj_has_flag(i->source,LV_OBJ_FLAG_USER_1) && n+1<p->count && p->items[n+1].enabled) {
            lv_obj_t *next=p->items[n+1].source;
            if(lv_obj_check_type(next,&lv_dropdown_class)||lv_obj_check_type(next,&lv_slider_class)||lv_obj_check_type(next,&lv_button_class)) {
                int together=ih+gy+lv_obj_get_height(next);
                if(together<=h && y && y+together>h){page++;y=0;}
            }
        }
        if(ih>h && label) {
            if(x){y+=row+gy;x=row=0;}
            if(y){page++;y=0;}
            int line=lv_font_get_line_height(lv_obj_get_style_text_font(i->source,0))+
                lv_obj_get_style_text_line_space(i->source,0);
            if(line<1)line=1;
            i->chunk=h/line*line;if(i->chunk<1)i->chunk=h;
            i->first=page;i->count=(ih+i->chunk-1)/i->chunk;i->h=ih;i->x=left;i->y=top;
            if(i->box==i->source) {
                i->box=lv_obj_create(root);lv_obj_remove_style_all(i->box);
                lv_obj_remove_flag(i->box,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
                lv_obj_set_scrollbar_mode(i->box,LV_SCROLLBAR_MODE_OFF);
                lv_obj_set_parent(i->source,i->box);
                lv_obj_set_width(i->source,w);
            }
            lv_obj_set_size(i->box,w,i->chunk);
            last=page+i->count-1;page=last+1;x=y=row=0;
        } else {
            if(y+ih>h){page++;x=y=row=0;}
            i->first=page;i->count=1;i->h=ih;i->chunk=ih;i->x=left+x;i->y=top+y;
            if(i->box!=i->source){lv_obj_set_size(i->box,w,ih);lv_obj_set_pos(i->source,0,0);}
            last=page;
            if(p->grid){x+=iw+gx;if(ih>row)row=ih;}
            else y+=ih+gy;
        }
    }
    p->pages=last+1;if(p->current>=p->pages)p->current=p->pages-1;
    for(unsigned n=0;n<p->count;n++) {
        item_t *i=&p->items[n];if(!i->enabled)continue;
        bool visible=p->current>=i->first && p->current<i->first+i->count;
        hide(i->box,!visible);
        if(visible) {
            lv_obj_set_pos(i->box,i->x,i->y);
            if(i->box!=i->source)lv_obj_set_pos(i->source,0,-(int)(p->current-i->first)*i->chunk);
        }
    }
}
void cyd_pager_attach(lv_obj_t *root) {
    if(!root)return;
    if(lv_obj_get_user_data(root)){cyd_pager_refresh(root);return;}
    lv_obj_update_layout(root);
    unsigned count=lv_obj_get_child_count(root);
    pager_t *p=lv_malloc(sizeof(*p)+sizeof(item_t)*count);if(!p)return;
    memset(p,0,sizeof(*p)+sizeof(item_t)*count);p->count=count;p->pages=1;
    p->grid=lv_obj_get_style_flex_flow(root,0)==LV_FLEX_FLOW_ROW_WRAP;
    for(unsigned n=0;n<count;n++) {
        item_t *i=&p->items[n];i->source=i->box=lv_obj_get_child(root,n);
        i->enabled=!lv_obj_has_flag(i->source,LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_scroll_to_y(root,0,LV_ANIM_OFF);lv_anim_delete(root,NULL);
    lv_obj_set_layout(root,LV_LAYOUT_NONE);
    lv_obj_remove_flag(root,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_MOMENTUM|
        LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_scroll_dir(root,LV_DIR_NONE);lv_obj_set_scrollbar_mode(root,LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_user_data(root,p);lv_obj_add_event_cb(root,destroyed,LV_EVENT_DELETE,NULL);
    cyd_pager_refresh(root);
}
bool cyd_pager_move(lv_obj_t *root,int direction) {
    if(!root)return false;
    cyd_pager_refresh(root);pager_t *p=lv_obj_get_user_data(root);if(!p)return false;
    if(direction<0 && p->current){p->current--;cyd_pager_refresh(root);return true;}
    if(direction>0 && p->current+1<p->pages){p->current++;cyd_pager_refresh(root);return true;}
    return false;
}
void cyd_pager_state(lv_obj_t *root,cyd_nav_state_t *out) {
    memset(out,0,sizeof(*out));out->page=out->pages=1;
    if(!root)return;
    pager_t *p=lv_obj_get_user_data(root);if(!p)return;
    out->active=true;out->page=p->current+1;out->pages=p->pages;
    out->back=p->current>0;out->next=p->current+1<p->pages;
}
bool cyd_pager_reveal(lv_obj_t *root,lv_obj_t *target) {
    if(!root)return false;
    cyd_pager_refresh(root);
    pager_t *p=lv_obj_get_user_data(root);if(!p)return false;
    for(unsigned n=0;n<p->count;n++) {
        item_t *i=&p->items[n];if(!i->enabled)continue;
        if(target==i->source || target==i->box || (lv_obj_is_valid(target) && lv_obj_get_parent(target)==i->source)) {
            p->current=i->first;cyd_pager_refresh(root);return true;
        }
    }
    return false;
}
