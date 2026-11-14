#pragma once
#include <stddef.h>
typedef struct {unsigned address;} ip_addr_t;
typedef int err_t;
#define ERR_OK 0
#define ERR_INPROGRESS -5
#define LWIP_DNS_ADDRTYPE_IPV4 0
err_t dns_gethostbyname_addrtype(const char *,ip_addr_t *,void (*)(const char *,const ip_addr_t *,void *),void *,int);
char *ipaddr_ntoa_r(const ip_addr_t *,char *,int);
