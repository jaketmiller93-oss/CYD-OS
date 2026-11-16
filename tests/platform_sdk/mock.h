#pragma once
#include "../system_sdk/fake_sdk.h"
#define ESP_LOGI(...) ((void)0)
SemaphoreHandle_t xSemaphoreCreateBinary(void);
void vSemaphoreDelete(SemaphoreHandle_t);

#define ESP_LOGE(...) ((void)0)
