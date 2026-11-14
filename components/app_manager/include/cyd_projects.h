#pragma once
#include "esp_err.h"
#include "cJSON.h"
#include <stddef.h>
#define CYD_PROJECT_MAX_FOLDERS 16
#define CYD_PROJECT_MAX_PLACEMENTS 64
esp_err_t cyd_projects_init(void);
cJSON *cyd_projects_catalog(void);
esp_err_t cyd_project_folder_create(const char *name, char out_id[9]);
esp_err_t cyd_project_folder_rename(const char *id, const char *name);
esp_err_t cyd_project_folder_delete(const char *id);
esp_err_t cyd_project_move(const char *package_id, const char *folder_id);
