/* Real loopback TCP/OpenSSL adapter for the production portable engine.
   DNS is deliberately outside these tests; ESP adapter uses bounded async lwIP DNS. */
#include "cyd_services.h"
#include <openssl/ssl.h>
#include <openssl/pem.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef struct {int fd;bool tcp_ready,tls;SSL_CTX *ctx;SSL *ssl;} host_connection_t;
static void close_host(void *ptr){host_connection_t *c=ptr;if(c->ssl)SSL_free(c->ssl);if(c->ctx)SSL_CTX_free(c->ctx);if(c->fd>=0)close(c->fd);free(c);}
static void *open_host(const cyd_endpoint_t *ep,char *error,size_t cap){host_connection_t *c=calloc(1,sizeof(*c));if(!c)return NULL;c->fd=-1;struct sockaddr_in sa={.sin_family=AF_INET,.sin_port=htons(ep->port)};if(inet_pton(AF_INET,ep->host,&sa.sin_addr)!=1){snprintf(error,cap,"Host tests require numeric IPv4");goto fail;}c->fd=socket(AF_INET,SOCK_STREAM,0);if(c->fd<0||fcntl(c->fd,F_SETFL,O_NONBLOCK)<0)goto fail;int result=connect(c->fd,(void *)&sa,sizeof(sa));if(result<0&&errno!=EINPROGRESS)goto fail;c->tcp_ready=result==0;c->tls=ep->tls;
 if(c->tls){c->ctx=SSL_CTX_new(TLS_client_method());if(!c->ctx)goto fail;SSL_CTX_set_verify(c->ctx,SSL_VERIFY_PEER,NULL);if(*ep->ca){BIO *bio=BIO_new_mem_buf(ep->ca,-1);X509 *ca=PEM_read_bio_X509(bio,NULL,NULL,NULL);BIO_free(bio);if(!ca)goto fail;int good=X509_STORE_add_cert(SSL_CTX_get_cert_store(c->ctx),ca);X509_free(ca);if(!good)goto fail;}else if(!SSL_CTX_set_default_verify_paths(c->ctx))goto fail;c->ssl=SSL_new(c->ctx);if(!c->ssl||SSL_set_fd(c->ssl,c->fd)!=1||SSL_set1_host(c->ssl,ep->verify_name)!=1||SSL_set_tlsext_host_name(c->ssl,ep->verify_name)!=1)goto fail;}
 return c;
 fail:snprintf(error,cap,"TCP or TLS trust configuration failed");close_host(c);return NULL;
}
static int connect_host(void *ptr,char *error,size_t cap){host_connection_t *c=ptr;if(!c->tcp_ready){struct pollfd p={.fd=c->fd,.events=POLLOUT};int n=poll(&p,1,0);if(n<=0)return n;int e=0;socklen_t size=sizeof(e);if(getsockopt(c->fd,SOL_SOCKET,SO_ERROR,&e,&size)||e){snprintf(error,cap,"TCP connection refused");return -1;}c->tcp_ready=true;}if(!c->tls)return 1;int rc=SSL_connect(c->ssl);if(rc==1)return 1;int err=SSL_get_error(c->ssl,rc);if(err==SSL_ERROR_WANT_READ||err==SSL_ERROR_WANT_WRITE)return 0;snprintf(error,cap,"TLS certificate/hostname or handshake failed");return -1;}
static int read_host(void *ptr,void *out,size_t max){host_connection_t *c=ptr;if(c->tls){int n=SSL_read(c->ssl,out,(int)max);if(n>0)return n;int e=SSL_get_error(c->ssl,n);return e==SSL_ERROR_WANT_READ||e==SSL_ERROR_WANT_WRITE?0:-1;}ssize_t n=recv(c->fd,out,max,0);if(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK))return 0;return n<=0?-1:n;}
static int write_host(void *ptr,const void *in,size_t size){host_connection_t *c=ptr;if(c->tls){int n=SSL_write(c->ssl,in,(int)size);if(n>0)return n;int e=SSL_get_error(c->ssl,n);return e==SSL_ERROR_WANT_READ||e==SSL_ERROR_WANT_WRITE?0:-1;}ssize_t n=send(c->fd,in,size,MSG_NOSIGNAL);if(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK))return 0;return n<=0?-1:n;}
cyd_io_t cyd_test_network_io(void){return (cyd_io_t){open_host,connect_host,read_host,write_host,close_host};}
