#include "cyd_services.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static cyd_services_t *session;
static unsigned char input[32768],output[65536];static size_t input_size,input_offset,output_size,window;static int opens,closes,pending,force_eof,stall_write;
static void *mock_open(const cyd_endpoint_t *e,char *error,size_t n){(void)e;(void)error;(void)n;opens++;return (void *)1;}
static int mock_connect(void *p,char *e,size_t n){(void)p;(void)e;(void)n;return pending?0:1;}
static int mock_read(void *p,void *out,size_t n){(void)p;if(input_offset==input_size)return force_eof?-1:0;size_t available=input_size-input_offset;if(n>available)n=available;if(window&&n>window)n=window;memcpy(out,input+input_offset,n);input_offset+=n;return n;}
static int mock_write(void *p,const void *in,size_t n){(void)p;if(stall_write)return 0;if(window&&n>window)n=window;if(n>sizeof(output)-output_size)return -1;memcpy(output+output_size,in,n);output_size+=n;return n;}
static void mock_close(void *p){(void)p;closes++;}
void test_service_reset(void){if(session){cyd_services_close(session);free(session);}session=calloc(1,sizeof(*session));cyd_io_t io={mock_open,mock_connect,mock_read,mock_write,mock_close};cyd_services_init(session,&io);input_size=input_offset=output_size=window=opens=closes=pending=force_eof=stall_write=0;}
void test_service_tick(unsigned long long now){cyd_services_tick(session,now);}
void test_service_input(const void *data,size_t n,unsigned chunk){if(n>sizeof(input))abort();memcpy(input,data,n);input_size=n;input_offset=0;window=chunk;}
void test_service_pending(int v){pending=v;}void test_service_eof(int v){force_eof=v;}
int test_service_opens(void){return opens;}int test_service_closes(void){return closes;}
size_t test_service_output(void *data,size_t max){size_t n=output_size<max?output_size:max;memcpy(data,output,n);return n;}
size_t test_event_size(void){return sizeof(cyd_service_event_t);}size_t test_service_size(void){return sizeof(cyd_services_t);}
int test_service_pop(cyd_service_event_t *e){return cyd_services_pop(session,e);}
int test_service_http(const char *url,int timeout){cyd_endpoint_t ep;if(!cyd_endpoint_parse(url,"","",&ep))return -1;uint32_t gen;return cyd_services_http(session,&ep,false,"",NULL,0,timeout,&gen)==0?(int)gen:-1;}
int test_service_http_custom(const char *url,const char *method,const char *headers,const void *body,size_t n,int timeout){cyd_endpoint_t ep;if(!cyd_endpoint_parse(url,"","",&ep))return -1;uint32_t gen;return cyd_services_http(session,&ep,!strcmp(method,"POST"),headers,body,n,timeout,&gen)==0?(int)gen:-1;}
int test_service_mqtt(void){cyd_endpoint_t ep;cyd_endpoint_parse("mqtt://127.0.0.1","","",&ep);uint32_t gen;int n=cyd_services_mqtt(session,&ep,"test-client","user","password",&gen);return n<0?-1:(int)gen;}
int test_service_subscribe(unsigned slot,unsigned gen,const char *topic){return cyd_services_subscribe(session,slot,gen,topic);}
int test_service_publish(unsigned slot,unsigned gen,const char *topic,const void *data,size_t size){return cyd_services_publish(session,slot,gen,topic,data,size,false);}
int test_service_cancel(unsigned slot,unsigned gen){return cyd_services_disconnect(session,slot,gen);}
int test_service_timer(unsigned ms,int repeat){uint32_t gen;int slot=cyd_services_timer(session,ms,repeat,&gen);return slot<0?-1:(int)((gen<<8)|slot);}
int test_timer_cancel(unsigned packed){return cyd_services_timer_cancel(session,packed&255,packed>>8);}
void test_service_close(void){cyd_services_close(session);}
int test_endpoint(const char *url,const char *ca,const char *name){cyd_endpoint_t ep;return cyd_endpoint_parse(url,ca,name,&ep);}
#ifdef CYD_HOST_NETWORK
cyd_io_t cyd_test_network_io(void);
void test_network_reset(void){if(session){cyd_services_close(session);free(session);}session=calloc(1,sizeof(*session));cyd_io_t io=cyd_test_network_io();cyd_services_init(session,&io);}
int test_network_http(const char *url,const char *ca,const char *name,int post,const void *body,size_t n,unsigned timeout){cyd_endpoint_t ep;if(!cyd_endpoint_parse(url,ca,name,&ep))return -1;uint32_t gen;return cyd_services_http(session,&ep,post,"Content-Type: application/json\r\n",body,n,timeout,&gen)==0?(int)gen:-1;}
int test_network_mqtt(const char *url,const char *ca,const char *name){cyd_endpoint_t ep;if(!cyd_endpoint_parse(url,ca,name,&ep))return -1;uint32_t gen;int slot=cyd_services_mqtt(session,&ep,"loopback-client","user","password",&gen);return slot<0?-1:(int)gen;}
#endif

void test_service_stall_write(int yes){stall_write=yes;}
