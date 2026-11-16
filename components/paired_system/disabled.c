#include "cyd_pair.h"
esp_err_t cyd_pair_start(void) {
    return ESP_OK;
}
esp_err_t cyd_pair_rpc(unsigned o, const void *p, size_t n, void *r, size_t c, size_t *s) {
    (void)o;
    (void)p;
    (void)n;
    (void)r;
    (void)c;
    (void)s;
    return ESP_ERR_NOT_SUPPORTED;
}
cJSON *cyd_pair_json(unsigned o, const cJSON *p) {
    (void)o;
    (void)p;
    return NULL;
}
cJSON *cyd_pair_status(void) {
    return NULL;
}

bool cyd_pair_installing(void) {
    return false;
}

esp_err_t cyd_pair_memory_init(void) {
    return ESP_OK;
}

void cyd_pair_elf_sha256(char out[65]) {
    out[0] = 0;
}
