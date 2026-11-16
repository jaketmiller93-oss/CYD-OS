#include "pager.h"
#include "cyd.h"
#include "cyd_projects.h"
#include "projects.h"
#include "freertos/queue.h"
#include <stdio.h>
#include <string.h>
typedef enum { BROWSE, PROJECT, MOVE, EDIT, DELETE_FOLDER, FOLDER_ACTIONS } project_page_t;
typedef enum { LOAD, CREATE, RENAME, DELETE, MOVE_PROJECT, RUN } project_job_kind_t;
typedef struct {
    project_job_kind_t kind;
    uint32_t generation;
    char folder[9], project[65], name[33], revision[17];
} project_job_t;
static lv_obj_t *project_view, *project_body, *project_note, *project_editor, *project_title;
static QueueHandle_t project_jobs;
static cJSON *project_data;
static project_page_t project_page;
static char current_folder[9], current_project[65];
static unsigned project_width, project_height;
static uint32_t project_generation;
static bool renaming_folder;
static unsigned grid_cell_width, grid_cell_height, grid_icon_size;
static void draw_projects(project_page_t page);
static const char *project_str(cJSON *o, const char *key) {
    cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    return cJSON_IsString(v) ? v->valuestring : "";
}
static cJSON *find_project(void) {
    cJSON *p;
    cJSON_ArrayForEach(
        p, cJSON_GetObjectItem(project_data, "projects")) if (!strcmp(project_str(p, "package_id"),
                                                                      current_project)) return p;
    return NULL;
}
static const char *folder_name(const char *id) {
    if (!*id)
        return "Apps";
    cJSON *f;
    cJSON_ArrayForEach(f, cJSON_GetObjectItem(
                              project_data, "folders")) if (!strcmp(project_str(f, "id"),
                                                                    id)) return project_str(f,
                                                                                            "name");
    return "Folder";
}
static lv_obj_t *project_text(const char *text) {
    lv_obj_t *l = lv_label_create(project_body);
    lv_label_set_text(l, text);
    lv_obj_set_width(l, LV_PCT(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    return l;
}
static lv_obj_t *project_button(const char *text, lv_event_cb_t cb, const char *value) {
    lv_obj_t *b = lv_button_create(project_body);
    lv_obj_set_width(b, LV_PCT(100));
    lv_obj_set_height(b, 44);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_set_width(l, LV_PCT(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_center(l);
    /* Copy the action value into LVGL-owned label text; no transient JSON pointers in callbacks. */
    lv_obj_t *tag = lv_label_create(b);
    lv_label_set_text(tag, value ? value : "");
    lv_obj_add_flag(tag, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, tag);
    return b;
}
/* Icons and captions share one hit target; values stay owned by LVGL, not JSON. */
static unsigned folder_apps(const char *id) {
    unsigned count = 0;
    cJSON *p;
    cJSON_ArrayForEach(p, cJSON_GetObjectItem(project_data, "projects"))
        if (!strcmp(project_str(p, "folder"), id))
            count++;
    return count;
}
static uint32_t app_color(const char *identity) {
    static const uint32_t colors[] = {0x276f69, 0x385995, 0x744b8f, 0x9a503d, 0x3f6e43, 0x8a622c};
    uint32_t hash = 2166136261u;
    for (const unsigned char *p = (const unsigned char *)identity; *p; p++)
        hash = (hash ^ *p) * 16777619u;
    return colors[hash % (sizeof(colors) / sizeof(colors[0]))];
}
static lv_obj_t *project_tile(const char *name, lv_event_cb_t cb, const char *value,
                              bool folder, unsigned count) {
    lv_obj_t *button = lv_button_create(project_body);
    lv_obj_set_size(button, grid_cell_width, grid_cell_height);
    lv_obj_set_style_pad_all(button, 0, 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x303743), LV_STATE_PRESSED);
    lv_obj_set_style_radius(button, 12, 0);
    lv_obj_t *icon = lv_obj_create(button);
    unsigned x = (grid_cell_width - grid_icon_size) / 2;
    lv_obj_set_pos(icon, x, 2);
    lv_obj_set_size(icon, grid_icon_size, grid_icon_size);
    lv_obj_set_style_pad_all(icon, 0, 0);
    lv_obj_set_style_border_width(icon, 0, 0);
    lv_obj_set_style_radius(icon, folder ? 12 : 16, 0);
    lv_obj_set_style_bg_color(icon, lv_color_hex(folder ? 0x403019 : app_color(value)), 0);
    lv_obj_remove_flag(icon, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *glyph = lv_label_create(icon);
    char initial[2] = {0};
    unsigned char first = (unsigned char)name[0];
    if (first >= 'a' && first <= 'z')
        initial[0] = first - 'a' + 'A';
    else if ((first >= 'A' && first <= 'Z') || (first >= '0' && first <= '9'))
        initial[0] = first;
    lv_label_set_text(glyph, folder ? LV_SYMBOL_DIRECTORY : *initial ? initial : LV_SYMBOL_FILE);
#if LV_FONT_MONTSERRAT_28
    lv_obj_set_style_text_font(glyph, &lv_font_montserrat_28, 0);
#endif
    lv_obj_set_style_text_color(glyph, lv_color_hex(folder ? 0xffc15a : 0xffffff), 0);
    lv_obj_center(glyph);
    lv_obj_t *caption = lv_label_create(button);
    lv_label_set_text(caption, name);
    lv_obj_set_pos(caption, 0, grid_icon_size + 6);
    lv_obj_set_size(caption, grid_cell_width, 36);
    lv_label_set_long_mode(caption, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, 0);
#if LV_FONT_MONTSERRAT_12
    if (grid_cell_width < 90)
        lv_obj_set_style_text_font(caption, &lv_font_montserrat_12, 0);
#endif
    lv_obj_set_style_text_color(caption, lv_color_hex(0xf1f3f7), 0);
    if (count && (folder || count > 1)) {
        char text[4];
        snprintf(text, sizeof(text), "%u", count > 99 ? 99 : count);
        lv_obj_t *badge = lv_label_create(button);
        lv_label_set_text(badge, text);
        lv_obj_set_pos(badge, x + grid_icon_size - 16, 0);
        lv_obj_set_size(badge, 24, 18);
        lv_obj_set_style_text_align(badge, LV_TEXT_ALIGN_CENTER, 0);
#if LV_FONT_MONTSERRAT_12
        lv_obj_set_style_text_font(badge, &lv_font_montserrat_12, 0);
#endif
        lv_obj_set_style_text_color(badge, lv_color_white(), 0);
        lv_obj_set_style_bg_color(badge, lv_color_hex(0x424c5c), 0);
        lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(badge, 9, 0);
    }
    lv_obj_t *tag = lv_label_create(button);
    lv_label_set_text(tag, value ? value : "");
    lv_obj_add_flag(tag, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, tag);
    return button;
}
static void tile_grid(void) {
    lv_obj_set_style_bg_opa(project_body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(project_body, 0, 0);
    lv_obj_set_style_pad_column(project_body, 8, 0);
    lv_obj_set_flex_flow(project_body, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(project_body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scroll_dir(project_body, LV_DIR_VER);
    /* The page may still be hidden while drawing, so use its explicit geometry
     * rather than stale LVGL content coordinates before it becomes visible. */
    unsigned inner = project_width > 16 ? project_width - 16 : 1;
    unsigned minimum = project_width >= 480 ? 100 : 68;
    unsigned columns = (inner + 8) / (minimum + 8);
    if (!columns)
        columns = 1;
    if (columns > 6)
        columns = 6;
    grid_cell_width = (inner - (columns - 1) * 8) / columns;
    grid_icon_size = project_width >= 480 ? 60 : 48;
    if (grid_icon_size + 8 > grid_cell_width)
        grid_icon_size = grid_cell_width > 8 ? grid_cell_width - 8 : 1;
    grid_cell_height = grid_icon_size + 46;
}
static void toolbar_button(const char *text, unsigned x, lv_event_cb_t cb) {
    lv_obj_t *button = lv_button_create(project_view);
    lv_obj_set_pos(button, x, 2);
    lv_obj_set_size(button, 32, 30);
    lv_obj_set_style_pad_all(button, 0, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x252d3a), 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, NULL);
}
static const char *button_value(lv_event_t *e) {
    return lv_label_get_text((lv_obj_t *)lv_event_get_user_data(e));
}
static bool queue_project(project_job_kind_t kind, const char *value) {
    project_job_t job = {.kind = kind, .generation = ++project_generation};
    snprintf(job.folder, 9, "%s", current_folder);
    snprintf(job.project, 65, "%s", current_project);
    if (kind == CREATE || kind == RENAME)
        snprintf(job.name, 33, "%s", value ? value : "");
    if (kind == RUN)
        snprintf(job.revision, 17, "%s", value ? value : "");
    if (kind == MOVE_PROJECT)
        snprintf(job.folder, 9, "%s", value ? value : "");
    if (!project_jobs || xQueueSend(project_jobs, &job, 0) != pdTRUE) {
        if (project_note)
            lv_label_set_text(project_note, "Apps service busy; try again");
        return false;
    }
    if (project_note) {
        lv_obj_remove_flag(project_note, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(project_note, "Working...");
    }
    return true;
}
static void project_back(lv_event_t *e) {
    (void)e;
    project_generation++;
    if (project_page == MOVE)
        draw_projects(PROJECT);
    else if (project_page != BROWSE)
        draw_projects(BROWSE);
    else if (current_folder[0]) {
        current_folder[0] = 0;
        draw_projects(BROWSE);
    } else
        cyd_projects_ui_close();
}
static void project_refresh(lv_event_t *e) {
    (void)e;
    queue_project(LOAD, NULL);
}
static void open_folder(lv_event_t *e) {
    snprintf(current_folder, 9, "%s", button_value(e));
    project_generation++;
    draw_projects(BROWSE);
}
static void open_project(lv_event_t *e) {
    snprintf(current_project, 65, "%s", button_value(e));
    project_generation++;
    draw_projects(PROJECT);
}
static void run_project(lv_event_t *e) {
    queue_project(RUN, button_value(e));
}
static void move_page(lv_event_t *e) {
    (void)e;
    project_generation++;
    draw_projects(MOVE);
}
static void move_selected(lv_event_t *e) {
    queue_project(MOVE_PROJECT, button_value(e));
}
static void new_folder(lv_event_t *e) {
    (void)e;
    project_generation++;
    renaming_folder = false;
    draw_projects(EDIT);
}
static void rename_folder(lv_event_t *e) {
    (void)e;
    project_generation++;
    renaming_folder = true;
    draw_projects(EDIT);
}
static void folder_tools(lv_event_t *e) {
    (void)e;
    project_generation++;
    draw_projects(FOLDER_ACTIONS);
}
static void delete_folder_page(lv_event_t *e) {
    (void)e;
    project_generation++;
    draw_projects(DELETE_FOLDER);
}
static void delete_folder_confirm(lv_event_t *e) {
    (void)e;
    queue_project(DELETE, NULL);
}
static void project_clear(lv_event_t *e) {
    (void)e;
    lv_textarea_set_text(project_editor, "");
}
static void project_keyboard(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CANCEL) {
        draw_projects(BROWSE);
        return;
    }
    if (code != LV_EVENT_READY)
        return;
    const char *name = lv_textarea_get_text(project_editor);
    size_t n = strlen(name);
    if (!n || n > 32 || name[0] == ' ' || name[n - 1] == ' ') {
        lv_label_set_text(project_title, "Folder name: 1-32 bytes, no edge spaces");
        return;
    }
    if (queue_project(renaming_folder ? RENAME : CREATE, name)) {
        draw_projects(BROWSE);
        if (project_note)
            lv_label_set_text(project_note, "Saving folder...");
    }
}
static void draw_projects(project_page_t page) {
    project_page = page;
    lv_obj_clean(project_view);
    project_body = project_note = project_editor = project_title = NULL;
    if (page == EDIT) {
        project_title = lv_label_create(project_view);
        lv_label_set_text(project_title, renaming_folder ? "Rename folder" : "New folder");
        lv_obj_set_pos(project_title, 4, 1);
        lv_obj_set_width(project_title, project_width - 8);
        lv_label_set_long_mode(project_title, LV_LABEL_LONG_DOT);
        project_editor = lv_textarea_create(project_view);
        lv_obj_set_pos(project_editor, 4, 20);
        lv_obj_set_size(project_editor, project_width - 72, 44);
        lv_textarea_set_one_line(project_editor, true);
        lv_textarea_set_max_length(project_editor, 32);
        lv_textarea_set_text(project_editor, renaming_folder ? folder_name(current_folder) : "");
        lv_obj_t *clear = lv_button_create(project_view);
        lv_obj_set_pos(clear, project_width - 64, 24);
        lv_obj_set_size(clear, 60, 36);
        lv_obj_set_style_pad_all(clear, 2, 0);
        lv_obj_t *l = lv_label_create(clear);
        lv_label_set_text(l, "Clear");
        lv_obj_center(l);
        lv_obj_add_event_cb(clear, project_clear, LV_EVENT_CLICKED, NULL);
        lv_obj_t *keys = lv_keyboard_create(project_view);
        lv_obj_set_align(keys, LV_ALIGN_TOP_LEFT);
        lv_obj_set_pos(keys, 0, 72);
        lv_obj_set_size(keys, project_width, project_height - 72);
        lv_keyboard_set_textarea(keys, project_editor);
        lv_obj_add_event_cb(keys, project_keyboard, LV_EVENT_ALL, NULL);
        return;
    }
    lv_obj_t *back = lv_button_create(project_view);
    lv_obj_set_pos(back, 4, 2);
    lv_obj_set_size(back, 44, 30);
    lv_obj_set_style_pad_all(back, 0, 0);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x252d3a), 0);
    lv_obj_set_style_shadow_width(back, 0, 0);
    lv_obj_t *label = lv_label_create(back);
    lv_label_set_text(label, "Up");
    lv_obj_center(label);
    lv_obj_add_event_cb(back, project_back, LV_EVENT_CLICKED, NULL);
    label = lv_label_create(project_view);
    const char *title = page == PROJECT         ? project_str(find_project(), "name")
                        : page == MOVE          ? "Move app"
                        : page == DELETE_FOLDER ? "Delete folder"
                                                : folder_name(current_folder);
    lv_label_set_text(label, *title ? title : "App");
    lv_obj_set_pos(label, 56, 8);
    lv_obj_set_width(label, project_width - (page == BROWSE ? 138 : 64));
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    project_body = lv_obj_create(project_view);
    lv_obj_set_pos(project_body, 0, 36);
    lv_obj_set_size(project_body, project_width, project_height - 36);
    lv_obj_set_style_pad_all(project_body, 8, 0);
    lv_obj_set_style_pad_row(project_body, 8, 0);
    lv_obj_set_flex_flow(project_body, LV_FLEX_FLOW_COLUMN);
    if (page == BROWSE || page == MOVE)
        tile_grid();
    project_note =
        project_text(project_data ? project_str(project_data, "catalog_error") : "Loading apps...");
    if (project_data && !*project_str(project_data, "catalog_error"))
        lv_obj_add_flag(project_note, LV_OBJ_FLAG_HIDDEN);
    if (page == BROWSE) {
        if (!current_folder[0]) {
            cJSON *f;
            cJSON_ArrayForEach(f, cJSON_GetObjectItem(project_data, "folders")) {
                project_tile(project_str(f, "name"), open_folder, project_str(f, "id"), true,
                             folder_apps(project_str(f, "id")));
            }
        }
        unsigned count = 0;
        cJSON *p;
        cJSON_ArrayForEach(
            p, cJSON_GetObjectItem(project_data, "projects")) if (!strcmp(project_str(p, "folder"),
                                                                          current_folder)) {
            if (count++ >= 64)
                break;
            project_tile(project_str(p, "name"), open_project, project_str(p, "package_id"), false,
                         cJSON_GetArraySize(cJSON_GetObjectItem(p, "versions")));
        }
        if (!count)
            project_text("No apps here yet. Upload an app, or move an app into this folder.");
        if (count > 64)
            project_text("More apps are available in the browser dashboard.");
        toolbar_button(current_folder[0] ? LV_SYMBOL_SETTINGS : LV_SYMBOL_PLUS,
                       project_width - 74, current_folder[0] ? folder_tools : new_folder);
        toolbar_button(LV_SYMBOL_REFRESH, project_width - 38, project_refresh);
    } else if (page == PROJECT) {
        cJSON *p = find_project();
        project_text(current_project);
        project_text("Saved versions - tap one to run");
        unsigned count = 0;
        cJSON *v;
        cJSON_ArrayForEach(v, cJSON_GetObjectItem(p, "versions")) {
            if (count++ >= 64)
                break;
            char b[96];
            snprintf(b, sizeof(b), "%s / %.8s", project_str(v, "version"),
                     project_str(v, "revision"));
            project_button(b, run_project, project_str(v, "revision"));
        }
        if (count > 64)
            project_text("Additional saved versions are available in the browser dashboard.");
        project_button("Move app to folder", move_page, "");
    } else if (page == MOVE) {
        project_tile("Apps", move_selected, "", true, folder_apps(""));
        cJSON *f;
        cJSON_ArrayForEach(f, cJSON_GetObjectItem(project_data, "folders"))
            project_tile(project_str(f, "name"), move_selected, project_str(f, "id"), true,
                         folder_apps(project_str(f, "id")));
    } else if (page == FOLDER_ACTIONS) {
        project_text("Folder options");
        project_button("Rename folder", rename_folder, "");
        project_button("Delete folder (keep apps)", delete_folder_page, "");
        project_button("Back to folder", project_back, "");
    } else if (page == DELETE_FOLDER) {
        project_text("Delete this folder? Its apps move back to Apps. Apps, saved versions "
                     "and private app data are kept.");
        project_button("Delete folder", delete_folder_confirm, "");
        project_button("Cancel", project_back, "");
    }
    cyd_pager_attach(project_body);
}
static void process_project_job(const project_job_t *job) {
    if (job->kind == RUN) {
        cyd_ui_lock();
        bool valid = job->generation == project_generation &&
                     !lv_obj_has_flag(project_view, LV_OBJ_FLAG_HIDDEN);
        if (valid)
            cyd_projects_ui_close();
        cyd_ui_unlock();
        if (valid) {
            esp_err_t e = cyd_run_revision(job->revision);
            if (e)
                cyd_log("projects", esp_err_to_name(e));
        }
        return;
    }
    esp_err_t e = ESP_OK;
    char new_id[9] = {0};
    if (job->kind == CREATE)
        e = cyd_project_folder_create(job->name, new_id);
    if (job->kind == RENAME)
        e = cyd_project_folder_rename(job->folder, job->name);
    if (job->kind == DELETE)
        e = cyd_project_folder_delete(job->folder);
    if (job->kind == MOVE_PROJECT)
        e = cyd_project_move(job->project, job->folder);
    cJSON *next = cyd_projects_catalog();
    cyd_ui_lock();
    if (job->generation == project_generation &&
        !lv_obj_has_flag(project_view, LV_OBJ_FLAG_HIDDEN)) {
        cJSON_Delete(project_data);
        project_data = next;
        next = NULL;
        if (!e && job->kind == CREATE)
            snprintf(current_folder, 9, "%s", new_id);
        if (!e && job->kind == DELETE)
            current_folder[0] = 0;
        if (!e && job->kind == MOVE_PROJECT)
            snprintf(current_folder, 9, "%s", job->folder);
        draw_projects(job->kind == MOVE_PROJECT ? PROJECT : BROWSE);
        if (e && project_note)
            lv_obj_remove_flag(project_note, LV_OBJ_FLAG_HIDDEN);
        if (e && project_note)
            lv_label_set_text(project_note,
                              e == ESP_ERR_INVALID_ARG ? "Invalid or duplicate folder name"
                              : e == ESP_ERR_NO_MEM
                                  ? "Folder/app limit reached, or memory unavailable"
                                  : "Could not save folders; check SD and update status");
    }
    cyd_ui_unlock();
    cJSON_Delete(next);
}
static void project_worker(void *arg) {
    (void)arg;
    project_job_t job;
    while (xQueueReceive(project_jobs, &job, portMAX_DELAY))
        process_project_job(&job);
}
esp_err_t cyd_projects_ui_init(lv_obj_t *parent, unsigned width, unsigned height) {
    project_width = width;
    project_height = height - 56;
    project_view = lv_obj_create(parent);
    lv_obj_set_pos(project_view, 0, 0);
    lv_obj_set_size(project_view, project_width, project_height);
    lv_obj_set_style_pad_all(project_view, 0, 0);
    lv_obj_set_style_border_width(project_view, 0, 0);
    lv_obj_set_style_radius(project_view, 0, 0);
    lv_obj_set_style_bg_opa(project_view, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(project_view, lv_color_hex(0x101216), 0);
    lv_obj_remove_flag(project_view, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(project_view, LV_OBJ_FLAG_HIDDEN);
    project_jobs = xQueueCreate(8, sizeof(project_job_t));
    if (!project_jobs || xTaskCreate(project_worker, "app_folders", 8192, NULL, 3, NULL) != pdPASS)
        return ESP_ERR_NO_MEM;
    return ESP_OK;
}
void cyd_projects_ui_open(void) {
    if (!project_view)
        return;
    current_folder[0] = current_project[0] = 0;
    draw_projects(BROWSE);
    lv_obj_remove_flag(project_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(project_view);
    if (project_jobs)
        queue_project(LOAD, NULL);
}
void cyd_projects_ui_close(void) {
    if (!project_view)
        return;
    project_generation++;
    lv_obj_add_flag(project_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clean(project_view);
    project_body = project_note = project_editor = project_title = NULL;
    cJSON_Delete(project_data);
    project_data = NULL;
}

void cyd_projects_ui_resize(unsigned w, unsigned h) {
    project_width = w;
    project_height = h - 56;
    if (!project_view)
        return;
    lv_obj_set_size(project_view, w, project_height);
    if (!lv_obj_has_flag(project_view, LV_OBJ_FLAG_HIDDEN))
        draw_projects(project_page);
}

bool cyd_projects_ui_nav(int direction,cyd_nav_state_t *state) {
    if(!project_view || lv_obj_has_flag(project_view,LV_OBJ_FLAG_HIDDEN))return false;
    cyd_pager_refresh(project_body);
    if(direction && !cyd_pager_move(project_body,direction) && direction<0)project_back(NULL);
    cyd_pager_state(project_body,state);state->active=true;state->back=true;return true;
}
