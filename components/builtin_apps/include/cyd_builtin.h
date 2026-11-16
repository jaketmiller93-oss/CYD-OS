#pragma once
#include "cyd.h"
unsigned cyd_builtin_count(void);
const char *cyd_builtin_name(unsigned);
const char *cyd_builtin_key(unsigned);
esp_err_t cyd_builtin_launch(unsigned);
const char *cyd_builtin_source(const char *path, size_t *size);
