#pragma once
#include "cyd.h"
#define UART_NUM_1 1
#define UART_DATA_8_BITS 8
#define UART_PARITY_DISABLE 0
#define UART_STOP_BITS_1 1
#define UART_HW_FLOWCTRL_DISABLE 0
#define UART_SCLK_DEFAULT 0
#define UART_PIN_NO_CHANGE -1
typedef struct {int baud_rate,data_bits,parity,stop_bits,flow_ctrl,source_clk;} uart_config_t;
esp_err_t uart_param_config(int,const uart_config_t *);
esp_err_t uart_set_pin(int,int,int,int,int);
esp_err_t uart_driver_install(int,int,int,int,void *,int);
esp_err_t uart_driver_delete(int);
int uart_read_bytes(int,void *,size_t,unsigned);
int uart_tx_chars(int,const char *,uint32_t);
