#pragma once
#define OPENSSL_SUPPRESS_DEPRECATED
#include <openssl/sha.h>
#include <string.h>
typedef SHA256_CTX mbedtls_sha256_context;
static inline void mbedtls_sha256_init(mbedtls_sha256_context *c){memset(c,0,sizeof(*c));}
static inline int mbedtls_sha256_starts(mbedtls_sha256_context *c,int is224){(void)is224;return SHA256_Init(c)?0:-1;}
static inline int mbedtls_sha256_update(mbedtls_sha256_context *c,const unsigned char *p,size_t n){return SHA256_Update(c,p,n)?0:-1;}
static inline int mbedtls_sha256_finish(mbedtls_sha256_context *c,unsigned char *p){return SHA256_Final(p,c)?0:-1;}
static inline void mbedtls_sha256_free(mbedtls_sha256_context *c){memset(c,0,sizeof(*c));}
