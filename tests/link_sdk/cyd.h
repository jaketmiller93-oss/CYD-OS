#pragma once
#include "fake_sdk.h"
#include "cJSON.h"
void cyd_log(const char *,const char *);
void vSemaphoreDelete(SemaphoreHandle_t);
SemaphoreHandle_t xSemaphoreCreateBinary(void);

const char *cyd_system_version(void);
bool cyd_storage_ready(void);
void cyd_network_address(char *, size_t);
