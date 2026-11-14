#include "system_sdk/cyd.h"
void nvs_close(nvs_handle_t h);
esp_err_t nvs_erase_key(nvs_handle_t h,const char *key);
#include "../components/app_services/credentials.c"
static unsigned char bytes[8][2048],staged[8][2048];static size_t sizes[8],staged_sizes[8];static bool fail_save;
void test_creds_reset(void){memset(bytes,0,sizeof(bytes));memset(staged,0,sizeof(staged));memset(sizes,0,sizeof(sizes));memset(staged_sizes,0,sizeof(staged_sizes));fail_save=false;}
void test_creds_fail(int value){fail_save=value;}
static int index_of(const char *k){if(strlen(k)!=3||k[0]!='a'||k[1]!='0'||k[2]<'0'||k[2]>'7')abort();return k[2]-'0';}
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *h){(void)mode;assert(!strcmp(name,"cyd_app_creds"));*h=1;memcpy(staged,bytes,sizeof(bytes));memcpy(staged_sizes,sizes,sizeof(sizes));return 0;}
void nvs_close(nvs_handle_t h){(void)h;}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *n){(void)h;int i=index_of(key);if(!sizes[i])return ESP_ERR_NVS_NOT_FOUND;if(*n<sizes[i])return ESP_ERR_INVALID_SIZE;memcpy(out,bytes[i],sizes[i]);*n=sizes[i];return 0;}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *v,size_t n){(void)h;int i=index_of(key);assert(n<=2048);memcpy(staged[i],v,n);staged_sizes[i]=n;return 0;}
esp_err_t nvs_commit(nvs_handle_t h){(void)h;if(fail_save)return ESP_FAIL;memcpy(bytes,staged,sizeof(bytes));memcpy(sizes,staged_sizes,sizeof(sizes));return 0;}
esp_err_t nvs_erase_key(nvs_handle_t h,const char *key){(void)h;staged_sizes[index_of(key)]=0;return 0;}
void test_creds_corrupt(void){if(sizes[0])bytes[0][0]=99;}
