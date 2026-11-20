#include "panel_ota.h"
#include "panel_protocol.h"
#include "panel_ota_protocol.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "cJSON.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
static QueueHandle_t acknowledgements;
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static panel_ota_info_t device;
static uint64_t seen,touched;
static uint32_t session,total,offset;
static bool active;
static uint64_t now_ms(void){return esp_timer_get_time()/1000;}
void panel_ota_init(void){acknowledgements=xQueueCreate(8,sizeof(panel_ota_ack_t));configASSERT(acknowledgements);}
bool panel_ota_busy(void){portENTER_CRITICAL(&mux);bool b=active&&now_ms()-touched<30000;portEXIT_CRITICAL(&mux);return b;}
bool panel_ota_receive(const uint8_t *data,int len){
 if(len==(int)sizeof(panel_ota_ack_t)){
  panel_ota_ack_t a;memcpy(&a,data,sizeof(a));
  if(a.magic!=PANEL_OTA_MAGIC||a.version!=PANEL_OTA_VERSION||a.type!=PANEL_OTA_ACK)return false;
  if(acknowledgements)xQueueSend(acknowledgements,&a,0);
  return true;
 }
 if(len==(int)sizeof(panel_ota_info_t)){
  panel_ota_info_t i;memcpy(&i,data,sizeof(i));
  if(i.magic!=PANEL_OTA_MAGIC||i.version!=PANEL_OTA_VERSION||i.type!=PANEL_OTA_INFO)return false;
  portENTER_CRITICAL(&mux);device=i;seen=now_ms();portEXIT_CRITICAL(&mux);return true;
 }return false;
}
static esp_err_t exchange(panel_ota_request_t *p,uint32_t expected){
 xQueueReset(acknowledgements);
 for(int retry=0;retry<10;retry++){
  if(esp_now_send(panel_remote_mac,(uint8_t*)p,sizeof(*p))!=ESP_OK){vTaskDelay(pdMS_TO_TICKS(50));continue;}
  uint64_t end=now_ms()+750;
  while(now_ms()<end){
   panel_ota_ack_t a;if(xQueueReceive(acknowledgements,&a,pdMS_TO_TICKS(50))!=pdTRUE)continue;
   if(a.session!=p->session||a.command!=p->command)continue;
   if(a.error!=ESP_OK)return a.error;
   if(a.offset==expected){portENTER_CRITICAL(&mux);touched=now_ms();portEXIT_CRITICAL(&mux);return ESP_OK;}
  }
 }
 return ESP_ERR_TIMEOUT;
}
static panel_ota_request_t request(uint8_t command){
 return (panel_ota_request_t){.magic=PANEL_OTA_MAGIC,.version=PANEL_OTA_VERSION,.type=PANEL_OTA_REQUEST,.session=session,.total=total,.offset=offset,.command=command};
}
static esp_err_t response(httpd_req_t *r,const char *status,const char *error){
 panel_ota_info_t info;uint64_t at;portENTER_CRITICAL(&mux);info=device;at=seen;portEXIT_CRITICAL(&mux);
 char hash[65];for(int i=0;i<32;i++)snprintf(hash+i*2,3,"%02x",info.sha256[i]);
 cJSON *j=cJSON_CreateObject();cJSON_AddBoolToObject(j,"online",at&&now_ms()-at<5000);
 cJSON_AddBoolToObject(j,"pending",info.pending);cJSON_AddBoolToObject(j,"busy",panel_ota_busy());
 cJSON_AddNumberToObject(j,"session",session);cJSON_AddNumberToObject(j,"offset",offset);cJSON_AddNumberToObject(j,"total",total);
 cJSON_AddNumberToObject(j,"boot",info.boot);cJSON_AddNumberToObject(j,"address",info.address);cJSON_AddStringToObject(j,"sha256",hash);
 if(error)cJSON_AddStringToObject(j,"error",error);
 char *text=cJSON_PrintUnformatted(j);httpd_resp_set_status(r,status);httpd_resp_set_type(r,"application/json");httpd_resp_set_hdr(r,"Cache-Control","no-store");
 esp_err_t result=httpd_resp_sendstr(r,text);free(text);cJSON_Delete(j);return result;
}
static bool authorized(httpd_req_t *r){char h[8];return httpd_req_get_hdr_value_str(r,"X-BLDSP",h,sizeof(h))==ESP_OK&&!strcmp(h,"1");}
static cJSON *body_json(httpd_req_t *r){
 char body[129];if(r->content_len<1||r->content_len>128)return NULL;
 int got=0;while(got<r->content_len){int n=httpd_req_recv(r,body+got,r->content_len-got);if(n<=0)return NULL;got+=n;}body[got]=0;return cJSON_Parse(body);
}
static bool u32(cJSON *j,const char *key,uint32_t *value){cJSON *n=cJSON_GetObjectItemCaseSensitive(j,key);if(!cJSON_IsNumber(n)||!isfinite(n->valuedouble)||n->valuedouble<0||n->valuedouble>UINT32_MAX||(double)(uint32_t)n->valuedouble!=n->valuedouble)return false;*value=(uint32_t)n->valuedouble;return true;}
static esp_err_t status_route(httpd_req_t *r){return response(r,"200 OK",NULL);}
static esp_err_t begin_route(httpd_req_t *r){
 if(!authorized(r))return response(r,"403 Forbidden","Cabecalho ausente");
 cJSON *j=body_json(r);uint32_t size=0;bool valid=u32(j,"size",&size)&&size>=288&&size<=PANEL_OTA_MAX_SIZE;cJSON_Delete(j);
 if(!valid)return response(r,"400 Bad Request","Tamanho de firmware invalido");
 if(panel_ota_busy())return response(r,"409 Conflict","Outra atualizacao esta em andamento");
 portENTER_CRITICAL(&mux);bool ready=seen&&now_ms()-seen<5000&&!device.pending;portEXIT_CRITICAL(&mux);
 if(!ready)return response(r,"503 Service Unavailable","C3 sem OTA disponivel ou aguardando confirmar boot");
 session=esp_random();if(!session)session=1;total=size;offset=0;
 portENTER_CRITICAL(&mux);active=true;touched=now_ms();portEXIT_CRITICAL(&mux);
 panel_ota_request_t p=request(PANEL_OTA_BEGIN);esp_err_t e=exchange(&p,0);
 if(e!=ESP_OK){portENTER_CRITICAL(&mux);active=false;portEXIT_CRITICAL(&mux);}
 ESP_LOGI("panel_ota","Begin session=%lu size=%lu result=%s",(unsigned long)session,(unsigned long)size,esp_err_to_name(e));
 return response(r,e==ESP_OK?"200 OK":"503 Service Unavailable",e==ESP_OK?NULL:esp_err_to_name(e));
}
static bool query_number(httpd_req_t *r,const char *key,uint32_t *out){
 char query[96],value[16],*end;
 if(httpd_req_get_url_query_str(r,query,sizeof(query))!=ESP_OK||httpd_query_key_value(query,key,value,sizeof(value))!=ESP_OK||!value[0])return false;
 for(char *p=value;*p;p++)if(*p<'0'||*p>'9')return false;
 unsigned long long n=strtoull(value,&end,10);if(*end||n>UINT32_MAX)return false;*out=(uint32_t)n;return true;
}
static esp_err_t data_route(httpd_req_t *r){
 if(!authorized(r))return response(r,"403 Forbidden","Cabecalho ausente");
 uint32_t id=0,start=0;
 if(!query_number(r,"session",&id)||!query_number(r,"offset",&start)||id!=session||!panel_ota_busy())return response(r,"409 Conflict","Sessao OTA expirada");
 if(r->content_len<1||r->content_len>4096||start>total||(uint32_t)r->content_len>total-start||start>offset)return response(r,"400 Bad Request","Bloco fora de ordem ou tamanho invalido");
 uint8_t *data=malloc(r->content_len);if(!data)return response(r,"503 Service Unavailable","Memoria insuficiente");
 int got=0;while(got<r->content_len){int n=httpd_req_recv(r,(char*)data+got,r->content_len-got);if(n<=0){free(data);return ESP_FAIL;}got+=n;}
 if(start<offset){free(data);return response(r,start+(uint32_t)got<=offset?"200 OK":"409 Conflict",start+(uint32_t)got<=offset?NULL:"Bloco parcialmente repetido");}
 esp_err_t e=ESP_OK;
 for(int i=0;i<got;){
  panel_ota_request_t p=request(PANEL_OTA_DATA);p.length=got-i>PANEL_OTA_DATA_SIZE?PANEL_OTA_DATA_SIZE:got-i;memcpy(p.data,data+i,p.length);
  e=exchange(&p,offset+p.length);if(e!=ESP_OK)break;offset+=p.length;i+=p.length;
 }
 free(data);
 return response(r,e==ESP_OK?"200 OK":"503 Service Unavailable",e==ESP_OK?NULL:esp_err_to_name(e));
}
static esp_err_t end_route(httpd_req_t *r){
 if(!authorized(r))return response(r,"403 Forbidden","Cabecalho ausente");
 cJSON *j=body_json(r);uint32_t id=0;bool valid=u32(j,"session",&id)&&id==session;cJSON_Delete(j);
 if(!valid||!panel_ota_busy()||offset!=total)return response(r,"409 Conflict","Transferencia incompleta ou sessao invalida");
 panel_ota_request_t p=request(PANEL_OTA_END);esp_err_t e=exchange(&p,total);
 portENTER_CRITICAL(&mux);active=false;portEXIT_CRITICAL(&mux);
 ESP_LOGI("panel_ota","Finish offset=%lu result=%s",(unsigned long)offset,esp_err_to_name(e));
 return response(r,e==ESP_OK?"200 OK":"503 Service Unavailable",e==ESP_OK?NULL:esp_err_to_name(e));
}
static esp_err_t abort_route(httpd_req_t *r){
 if(!authorized(r))return response(r,"403 Forbidden","Cabecalho ausente");
 cJSON *j=body_json(r);uint32_t id=0;bool valid=u32(j,"session",&id)&&id==session;cJSON_Delete(j);
 if(!valid||!panel_ota_busy())return response(r,"409 Conflict","Sessao OTA invalida");
 panel_ota_request_t p=request(PANEL_OTA_ABORT);esp_err_t e=exchange(&p,offset);
 portENTER_CRITICAL(&mux);active=false;portEXIT_CRITICAL(&mux);
 return response(r,e==ESP_OK?"200 OK":"503 Service Unavailable",e==ESP_OK?NULL:esp_err_to_name(e));
}
extern const unsigned char update_start[] asm("_binary_panel_update_html_start");
extern const unsigned char update_end[] asm("_binary_panel_update_html_end");
static esp_err_t update_page(httpd_req_t *r){
 httpd_resp_set_type(r,"text/html; charset=utf-8");httpd_resp_set_hdr(r,"Cache-Control","no-store");
 return httpd_resp_send(r,(const char*)update_start,update_end-update_start-1);
}
void panel_ota_register(httpd_handle_t server){
 const httpd_uri_t routes[]={
 {.uri="/panel-update",.method=HTTP_GET,.handler=update_page},
 {.uri="/api/panel/ota/abort",.method=HTTP_POST,.handler=abort_route},
 {.uri="/api/panel/ota",.method=HTTP_GET,.handler=status_route},
 {.uri="/api/panel/ota/begin",.method=HTTP_POST,.handler=begin_route},
 {.uri="/api/panel/ota/data",.method=HTTP_POST,.handler=data_route},
 {.uri="/api/panel/ota/end",.method=HTTP_POST,.handler=end_route}};
 for(unsigned i=0;i<sizeof(routes)/sizeof(routes[0]);i++)ESP_ERROR_CHECK(httpd_register_uri_handler(server,&routes[i]));
}

// Optional USB-to-ESP-NOW deployment bridge; absent from production builds.
#ifdef PANEL_OTA_SMOKE_TEST
#include "../tools/ota_bridge.inc"
#endif
