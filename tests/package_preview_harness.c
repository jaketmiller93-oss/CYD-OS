#include "cyd.h"
#include <stdlib.h>
#include <stdio.h>
int test_package_preview(const void *data,size_t n,char *out,size_t cap){char error[128];cJSON *o=cyd_package_inspect(data,n,error,sizeof(error));if(!o){snprintf(out,cap,"%s",error);return -1;}char *json=cJSON_PrintUnformatted(o);cJSON_Delete(o);if(!json)return -1;snprintf(out,cap,"%s",json);free(json);return 0;}
