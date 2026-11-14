#include "cyd.h"
#include "cyd_projects.h"
#include "projects.h"
#include "freertos/queue.h"
#include <stdio.h>
#include <string.h>
typedef enum { BROWSE, PROJECT, MOVE, EDIT, DELETE_FOLDER } project_page_t;
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
    draw_projects(MOVE);
}
static void move_selected(lv_event_t *e) {
    queue_project(MOVE_PROJECT, button_value(e));
}
static void new_folder(lv_event_t *e) {
    (void)e;
    renaming_folder = false;
    draw_projects(EDIT);
}
static void rename_folder(lv_event_t *e) {
    (void)e;
    renaming_folder = true;
    draw_projects(EDIT);
}
static void delete_folder_page(lv_event_t *e) {
    (void)e;
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
    lv_obj_set_size(back, 62, 30);
    lv_obj_t *label = lv_label_create(back);
    lv_label_set_text(label, "Back");
    lv_obj_center(label);
    lv_obj_add_event_cb(back, project_back, LV_EVENT_CLICKED, NULL);
    label = lv_label_create(project_view);
    const char *title = page == PROJECT         ? project_str(find_project(), "name")
                        : page == MOVE          ? "Move app"
                        : page == DELETE_FOLDER ? "Delete folder"
                                                : folder_name(current_folder);
    lv_label_set_text(label, *title ? title : "App");
    lv_obj_set_pos(label, 76, 8);
    lv_obj_set_width(label, project_width - 84);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    project_body = lv_obj_create(project_view);
    lv_obj_set_pos(project_body, 0, 36);
    lv_obj_set_size(project_body, project_width, project_height - 36);
    lv_obj_set_style_pad_all(project_body, 8, 0);
    lv_obj_set_style_pad_row(project_body, 8, 0);
    lv_obj_set_flex_flow(project_body, LV_FLEX_FLOW_COLUMN);
    project_note =
        project_text(project_data ? project_str(project_data, "catalog_error") : "Loading apps...");
    if (project_data && !*project_str(project_data, "catalog_error"))
        lv_obj_add_flag(project_note, LV_OBJ_FLAG_HIDDEN);
    if (page == BROWSE) {
        if (!current_folder[0]) {
            cJSON *f;
            cJSON_ArrayForEach(f, cJSON_GetObjectItem(project_data, "folders")) {
                char b[96];
                snprintf(b, sizeof(b), "Folder: %s", project_str(f, "name"));
                project_button(b, open_folder, project_str(f, "id"));
            }
            project_text("Apps outside folders");
        }
        unsigned count = 0;
        cJSON *p;
        cJSON_ArrayForEach(
            p, cJSON_GetObjectItem(project_data, "projects")) if (!strcmp(project_str(p, "folder"),
                                                                          current_folder)) {
            if (count++ >= 64)
                break;
            char b[112];
            snprintf(b, sizeof(b), "%s (%d versions)", project_str(p, "name"),
                     cJSON_GetArraySize(cJSON_GetObjectItem(p, "versions")));
            project_button(b, open_project, project_str(p, "package_id"));
        }
        if (!count)
            project_text("No apps here yet. Upload an app, or move an app into this folder.");
        if (count > 64)
            project_text("More apps are available in the browser dashboard.");
        if (!current_folder[0])
            project_button("Create folder", new_folder, "");
        else {
            project_button("Rename folder", rename_folder, "");
            project_button("Delete folder (keep apps)", delete_folder_page, "");
        }
        project_button("Refresh", project_refresh, "");
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
        project_button("Apps (outside folders)", move_selected, "");
        cJSON *f;
        cJSON_ArrayForEach(f, cJSON_GetObjectItem(project_data, "folders"))
            project_button(project_str(f, "name"), move_selected, project_str(f, "id"));
    } else if (page == DELETE_FOLDER) {
        project_text("Delete this folder? Its apps move back to Apps. Apps, saved versions "
                     "and private app data are kept.");
        project_button("Delete folder", delete_folder_confirm, "");
        project_button("Cancel", project_back, "");
    }
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
    lv_obj_remove_flag(project_view, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(project_view, LV_OBJ_FLAG_HIDDEN);
    project_jobs = xQueueCreate(8, sizeof(project_job_t));
    if (!project_jobs || xTaskCreate(project_worker, "app_folders", 8192, NULL, 3, NULL) != pdPASS)
        return ESP_ERR_NO_MEM;
    return ESP_OK;
}
void cyd_projects_ui_open(void) {
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
    lv_obj_set_size(project_view, w, project_height);
    if (!lv_obj_has_flag(project_view, LV_OBJ_FLAG_HIDDEN))
        draw_projects(project_page);
}
