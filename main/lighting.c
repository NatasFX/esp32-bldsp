#include "lighting.h"
#include "light_policy.h"
#include "esp_timer.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
static SemaphoreHandle_t lock;
static light_config_t config={.version=1,.smooth=1,.count=5,.latitude=-30,.longitude=-53,
 .steps={{-1,1,15},{0,1,60},{720,0,100},{1020,0,60},{30,2,15}}};
static int64_t epoch,anchor_us;static int utc_offset;
typedef struct {bool clock,solar,applied;int64_t unix_s;int offset;double minute,rise,set;unsigned brightness;light_config_t config;} state_t;
static state_t state(unsigned manual){
 state_t s={.brightness=manual};
 xSemaphoreTake(lock,portMAX_DELAY);s.config=config;s.offset=utc_offset;s.clock=epoch!=0;if(s.clock)s.unix_s=epoch+(esp_timer_get_time()-anchor_us)/1000000;xSemaphoreGive(lock);
 if(!s.clock)return s;
 time_t local=(time_t)(s.unix_s+s.offset*60);struct tm date;gmtime_r(&local,&date);int year=date.tm_year+1900;
 s.minute=date.tm_hour*60+date.tm_min+date.tm_sec/60.;
 s.solar=light_solar(date.tm_yday+1,(year%4==0&&(year%100!=0||year%400==0))?366:365,s.config.latitude,s.config.longitude,s.offset,&s.rise,&s.set);
 if(s.config.enabled&&s.solar)s.applied=light_evaluate(&s.config,s.minute,s.rise,s.set,&s.brightness);
 return s;
}
void lighting_start(void){
 lock=xSemaphoreCreateMutex();configASSERT(lock);nvs_handle_t n;
 if(nvs_open("panel",NVS_READONLY,&n)==ESP_OK){light_config_t saved;size_t len=sizeof(saved);if(nvs_get_blob(n,"light_v1",&saved,&len)==ESP_OK&&len==sizeof(saved)&&light_config_valid(&saved))config=saved;nvs_close(n);}
}
unsigned lighting_brightness(unsigned manual){return state(manual).brightness;}
void lighting_status(cJSON *root,unsigned manual){
 state_t s=state(manual);cJSON *o=cJSON_AddObjectToObject(root,"lighting");
 cJSON_AddBoolToObject(o,"enabled",s.config.enabled);cJSON_AddBoolToObject(o,"smooth",s.config.smooth);cJSON_AddBoolToObject(o,"clock_valid",s.clock);cJSON_AddBoolToObject(o,"solar_valid",s.solar);cJSON_AddBoolToObject(o,"applied",s.applied);
 cJSON_AddNumberToObject(o,"latitude",s.config.latitude);cJSON_AddNumberToObject(o,"longitude",s.config.longitude);cJSON_AddNumberToObject(o,"brightness",s.brightness);cJSON_AddNumberToObject(o,"utc_offset_minutes",s.offset);
 if(s.clock)cJSON_AddNumberToObject(o,"unix_seconds",(double)s.unix_s);else cJSON_AddNullToObject(o,"unix_seconds");
 if(s.solar){cJSON_AddNumberToObject(o,"sunrise_minute",s.rise);cJSON_AddNumberToObject(o,"sunset_minute",s.set);}
 cJSON *a=cJSON_AddArrayToObject(o,"steps");for(unsigned i=0;i<s.config.count;i++){cJSON *p=cJSON_CreateObject();cJSON_AddNumberToObject(p,"anchor",s.config.steps[i].anchor);cJSON_AddNumberToObject(p,"minute",s.config.steps[i].minute);cJSON_AddNumberToObject(p,"brightness",s.config.steps[i].brightness);cJSON_AddItemToArray(a,p);}
}
static esp_err_t response(httpd_req_t *r,const char *error){
 httpd_resp_set_type(r,"application/json");httpd_resp_set_hdr(r,"Cache-Control","no-store");cJSON *o=cJSON_CreateObject();
 if(error){httpd_resp_set_status(r,"400 Bad Request");cJSON_AddStringToObject(o,"error",error);}else lighting_status(o,30);
 char *text=cJSON_PrintUnformatted(o);esp_err_t e=httpd_resp_sendstr(r,text);free(text);cJSON_Delete(o);return e;
}
static bool number(cJSON *o,const char *key,double lo,double hi,double *v){cJSON *n=cJSON_GetObjectItemCaseSensitive(o,key);if(!cJSON_IsNumber(n)||!isfinite(n->valuedouble)||n->valuedouble<lo||n->valuedouble>hi)return false;*v=n->valuedouble;return true;}
static esp_err_t route(httpd_req_t *r){
 if(r->method==HTTP_GET)return response(r,NULL);
 char header[8],body[2049];if(httpd_req_get_hdr_value_str(r,"X-BLDSP",header,sizeof(header))!=ESP_OK||strcmp(header,"1"))return response(r,"Cabecalho ausente");
 if(r->content_len<1||r->content_len>2048)return response(r,"Corpo invalido");
 int got=0;while(got<r->content_len){int n=httpd_req_recv(r,body+got,r->content_len-got);if(n<=0)return ESP_FAIL;got+=n;}body[got]=0;cJSON *o=cJSON_Parse(body);bool valid=false;
 if(strstr(r->uri,"/clock")){
  double seconds,offset;valid=number(o,"unix_seconds",1704067200.,4102444800.,&seconds)&&number(o,"utc_offset_minutes",-840,840,&offset)&&floor(seconds)==seconds&&floor(offset)==offset;
  if(valid){xSemaphoreTake(lock,portMAX_DELAY);epoch=(int64_t)seconds;anchor_us=esp_timer_get_time();utc_offset=(int)offset;xSemaphoreGive(lock);}
 }else{
  light_config_t c={.version=1};double lat,lon; cJSON *enabled=cJSON_GetObjectItemCaseSensitive(o,"enabled"),*smooth=cJSON_GetObjectItemCaseSensitive(o,"smooth"),*steps=cJSON_GetObjectItemCaseSensitive(o,"steps");
  valid=cJSON_IsBool(enabled)&&cJSON_IsBool(smooth)&&number(o,"latitude",-89.999,89.999,&lat)&&number(o,"longitude",-180,180,&lon)&&cJSON_IsArray(steps)&&cJSON_GetArraySize(steps)>=2&&cJSON_GetArraySize(steps)<=LIGHT_MAX_STEPS;
  if(valid){c.enabled=cJSON_IsTrue(enabled);c.smooth=cJSON_IsTrue(smooth);c.latitude=lat;c.longitude=lon;c.count=cJSON_GetArraySize(steps);}
  for(unsigned i=0;valid&&i<c.count;i++){double anchor,minute,brightness;cJSON *p=cJSON_GetArrayItem(steps,i);valid=number(p,"anchor",0,2,&anchor)&&number(p,"minute",-720,1439,&minute)&&number(p,"brightness",0,100,&brightness)&&floor(anchor)==anchor&&floor(minute)==minute&&floor(brightness)==brightness;if(valid)c.steps[i]=(light_step_t){(int16_t)minute,(uint8_t)anchor,(uint8_t)brightness};}
  valid=valid&&light_config_valid(&c);
  if(valid){nvs_handle_t n;esp_err_t e=nvs_open("panel",NVS_READWRITE,&n);if(e==ESP_OK){e=nvs_set_blob(n,"light_v1",&c,sizeof(c));if(e==ESP_OK)e=nvs_commit(n);nvs_close(n);}valid=e==ESP_OK;if(valid){xSemaphoreTake(lock,portMAX_DELAY);config=c;xSemaphoreGive(lock);}}
 }
 cJSON_Delete(o);return response(r,valid?NULL:"Parametros invalidos ou falha ao salvar");
}
void lighting_register(httpd_handle_t server){
 const httpd_uri_t routes[]={{.uri="/api/lighting",.method=HTTP_GET,.handler=route},{.uri="/api/lighting",.method=HTTP_POST,.handler=route},{.uri="/api/clock",.method=HTTP_POST,.handler=route}};
 for(unsigned i=0;i<sizeof(routes)/sizeof(routes[0]);i++)ESP_ERROR_CHECK(httpd_register_uri_handler(server,&routes[i]));
}
