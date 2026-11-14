#include <assert.h>
#include <errno.h>
#include "../components/app_services/esp_io.c"
static struct {void (*fn)(void *);void *arg;} scheduled[4];static unsigned schedule_count;
static struct {void (*fn)(const char *,const ip_addr_t *,void *);void *arg;} callbacks[4];static unsigned callback_count;
static bool dns_later,dns_fail,tls_fail;static int created,destroyed,read_result;static char verified[254];static bool bundle_seen,ca_seen,plain_seen;
err_t tcpip_callback(void (*f)(void *),void *p){assert(schedule_count<4);scheduled[schedule_count].fn=f;scheduled[schedule_count++].arg=p;return 0;}
err_t dns_gethostbyname_addrtype(const char *h,ip_addr_t *ip,void (*cb)(const char *,const ip_addr_t *,void *),void *p,int type){(void)h;(void)type;if(dns_fail)return -1;if(!dns_later){ip->address=1;return 0;}assert(callback_count<4);callbacks[callback_count].fn=cb;callbacks[callback_count++].arg=p;return ERR_INPROGRESS;}
char *ipaddr_ntoa_r(const ip_addr_t *ip,char *out,int size){(void)ip;snprintf(out,size,"192.0.2.1");return out;}
esp_tls_t *esp_tls_init(void){created++;return calloc(1,sizeof(esp_tls_t));}
int esp_crt_bundle_attach(void *p){(void)p;return 0;}
int esp_tls_conn_new_async(const char *host,int len,int port,const esp_tls_cfg_t *c,esp_tls_t *t){(void)t;(void)port;assert(len==9&&!strcmp(host,"192.0.2.1"));assert(c->non_block&&!c->skip_common_name&&c->timeout_ms==100);snprintf(verified,sizeof(verified),"%s",c->common_name);bundle_seen=c->crt_bundle_attach==esp_crt_bundle_attach;ca_seen=c->cacert_buf&&c->cacert_bytes;plain_seen=c->is_plain_tcp;return tls_fail?-1:1;}
ssize_t esp_tls_conn_read(esp_tls_t *p,void *out,size_t n){(void)p;(void)out;(void)n;errno=EAGAIN;return read_result;}
ssize_t esp_tls_conn_write(esp_tls_t *p,const void *out,size_t n){(void)p;(void)out;(void)n;errno=EAGAIN;return read_result;}
void esp_tls_conn_destroy(esp_tls_t *p){destroyed++;free(p);}
static void pump(void){for(unsigned i=0;i<schedule_count;i++)scheduled[i].fn(scheduled[i].arg);schedule_count=0;}
#define CHECK(x) do{if(!(x))return __LINE__;}while(0)
int test_transport_case(int which){memset(dns_slots,0,sizeof(dns_slots));schedule_count=callback_count=0;dns_later=dns_fail=tls_fail=false;created=destroyed=0;bundle_seen=ca_seen=plain_seen=false;cyd_endpoint_t ep;CHECK(cyd_endpoint_parse("https://private.example/","","verify.example",&ep));char error[96];
 if(which==0){dns_later=true;void *clients[4];for(unsigned i=0;i<4;i++){clients[i]=open_transport(&ep,error,sizeof(error));CHECK(clients[i]);close_transport(clients[i]);}CHECK(!open_transport(&ep,error,sizeof(error)));pump();CHECK(callback_count==4);ip_addr_t address={1};for(unsigned i=0;i<4;i++)callbacks[i].fn("x",&address,callbacks[i].arg);callback_count=0;void *next=open_transport(&ep,error,sizeof(error));CHECK(next);close_transport(next);pump();return 0;}
 void *client=open_transport(&ep,error,sizeof(error));CHECK(client);CHECK(connect_transport(client,error,sizeof(error))==0);if(which==1)dns_fail=true;pump();if(which==1){CHECK(connect_transport(client,error,sizeof(error))==-1);CHECK(created==0);close_transport(client);return 0;}
 if(which==2)strcpy(ep.ca,"trusted PEM fixture");if(which==3)tls_fail=true;
 CHECK(connect_transport(client,error,sizeof(error))==(which==3?-1:1));CHECK(!strcmp(verified,"verify.example"));CHECK(!plain_seen);CHECK(which==2?ca_seen&&!bundle_seen:bundle_seen&&!ca_seen);if(which==4){read_result=-1;CHECK(read_transport(client,error,1)==0);CHECK(write_transport(client,error,1)==0);read_result=0;CHECK(read_transport(client,error,1)==-1);}close_transport(client);CHECK(created==destroyed);return 0;}
