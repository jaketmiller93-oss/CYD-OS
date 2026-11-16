#pragma once
#include "cyd_pair.h"
#include "pair_ops.h"
esp_err_t cyd_pair_dispatch(unsigned, const void *, size_t, void *, size_t, size_t *);
esp_err_t cyd_pair_files(unsigned, const void *, size_t, void *, size_t, size_t *);
esp_err_t cyd_pair_update(unsigned, const void *, size_t, void *, size_t, size_t *);
esp_err_t cyd_pair_network(unsigned, const void *, size_t, void *, size_t, size_t *);
const char *cyd_pair_string(const cJSON *, const char *);
bool cyd_pair_number(const cJSON *, const char *, int64_t, int64_t, int64_t *);
esp_err_t cyd_pair_encode(cJSON *, void *, size_t, size_t *);
void cyd_pair_clock_adopt(int64_t, int, bool);
