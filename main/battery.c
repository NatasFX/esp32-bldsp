#include "battery.h"
#include "battery_policy.h"
#include "vcc_calibration.h"
#include "telemetry.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdlib.h>
#include <string.h>
static SemaphoreHandle_t lock;
static battery_config_t config={12.2,11.8,11.,10.5};
static battery_policy_t policy;
static uint64_t last,clip_at;
static uint32_t boot,clip_id,clip_boot;
static double mean_v,min_v,clip_min_v;
static unsigned samples,missed;static bool valid;
void battery_start(void){
 lock=xSemaphoreCreateMutex();configASSERT(lock);nvs_handle_t n;
 if(nvs_open("panel",NVS_READONLY,&n)==ESP_OK){battery_config_t c;size_t size=sizeof(c);if(nvs_get_blob(n,"battery_v1",&c,&size)==ESP_OK&&size==sizeof(c)&&battery_config_valid(&c))config=c;nvs_close(n);}
}
void battery_ingest(const panel_vcc_t *p,uint32_t primary){
 if(!lock)return;
 uint64_t now=esp_timer_get_time()/1000;telemetry_clip_t clip;telemetry_clip_state(&clip);
 xSemaphoreTake(lock,portMAX_DELAY);
 if(boot!=p->boot){policy=(battery_policy_t){0};boot=p->boot;}
 valid=p->valid&&vcc_raw_valid(p->raw)&&p->samples>=80&&p->window_ms>=800&&p->window_ms<=1500&&!p->recovery_state;
 mean_v=vcc_from_mv(p->mv);min_v=vcc_from_mv(p->min_mv);samples=p->samples;missed=p->missed;last=now;
 battery_sample(&policy,&config,now,valid,mean_v,min_v);
 if(valid&&p->clip_min_mv&&p->clip_primary_boot==primary&&p->clip_count&&p->clip_count<=clip.count){
  double voltage=vcc_from_mv(p->clip_min_mv);
  if(clip_boot!=primary||clip_id!=p->clip_count||!clip_at){clip_min_v=voltage;clip_id=p->clip_count;clip_boot=primary;clip_at=now;}
  else if(voltage<clip_min_v)clip_min_v=voltage;
 }
 xSemaphoreGive(lock);
}
unsigned battery_alert(void){if(!lock)return 0;uint64_t now=esp_timer_get_time()/1000;xSemaphoreTake(lock,portMAX_DELAY);unsigned level=valid?battery_level(&policy,now):0;xSemaphoreGive(lock);return level;}
void battery_status(cJSON *root){
 if(!lock)return;
 uint64_t now=esp_timer_get_time()/1000;xSemaphoreTake(lock,portMAX_DELAY);
 cJSON *o=cJSON_AddObjectToObject(root,"battery");bool fresh=valid&&last&&now-last<2500;unsigned level=fresh?battery_level(&policy,now):0;
 cJSON_AddBoolToObject(o,"valid",fresh);cJSON_AddNumberToObject(o,"level",level);
 cJSON_AddNumberToObject(o,"warn_v",config.warn_v);cJSON_AddNumberToObject(o,"critical_v",config.critical_v);cJSON_AddNumberToObject(o,"dip_warn_v",config.dip_warn_v);cJSON_AddNumberToObject(o,"dip_critical_v",config.dip_critical_v);
 if(fresh){cJSON_AddNumberToObject(o,"mean_v",mean_v);cJSON_AddNumberToObject(o,"min_v",min_v);cJSON_AddNumberToObject(o,"slow_mean_v",policy.slow_sum/policy.count);}else{cJSON_AddNullToObject(o,"mean_v");cJSON_AddNullToObject(o,"min_v");cJSON_AddNullToObject(o,"slow_mean_v");}
 cJSON_AddNumberToObject(o,"slow_samples",policy.count);cJSON_AddNumberToObject(o,"samples",samples);cJSON_AddNumberToObject(o,"missed",missed);
 if(clip_at){cJSON_AddNumberToObject(o,"clip_min_v",clip_min_v);cJSON_AddNumberToObject(o,"clip_count",clip_id);cJSON_AddNumberToObject(o,"clip_age_ms",now-clip_at);}else cJSON_AddNullToObject(o,"clip_min_v");
 xSemaphoreGive(lock);
}
static esp_err_t respond(httpd_req_t *r,const char *error){
 httpd_resp_set_type(r,"application/json");httpd_resp_set_hdr(r,"Cache-Control","no-store");cJSON *o=cJSON_CreateObject();
 if(error){httpd_resp_set_status(r,"400 Bad Request");cJSON_AddStringToObject(o,"error",error);}else battery_status(o);
 char *s=cJSON_PrintUnformatted(o);esp_err_t e=httpd_resp_sendstr(r,s);free(s);cJSON_Delete(o);return e;
}
static bool number(cJSON *o,const char *key,double *v){cJSON *p=cJSON_GetObjectItemCaseSensitive(o,key);if(!cJSON_IsNumber(p)||!isfinite(p->valuedouble))return false;*v=p->valuedouble;return true;}
static esp_err_t route(httpd_req_t *r){
 if(r->method==HTTP_GET)return respond(r,NULL);
 char h[8],body[257];if(httpd_req_get_hdr_value_str(r,"X-BLDSP",h,sizeof(h))!=ESP_OK||strcmp(h,"1"))return respond(r,"Cabeçalho ausente");
 if(r->content_len<1||r->content_len>256)return respond(r,"Corpo inválido");
 int got=0;while(got<r->content_len){int n=httpd_req_recv(r,body+got,r->content_len-got);if(n<=0)return ESP_FAIL;got+=n;}body[got]=0;
 cJSON *o=cJSON_Parse(body);battery_config_t c={0};bool ok=number(o,"warn_v",&c.warn_v)&&number(o,"critical_v",&c.critical_v)&&number(o,"dip_warn_v",&c.dip_warn_v)&&number(o,"dip_critical_v",&c.dip_critical_v)&&battery_config_valid(&c);cJSON_Delete(o);
 if(!ok)return respond(r,"Limiares inválidos; crítico deve ser menor que aviso");
 nvs_handle_t n;esp_err_t err=nvs_open("panel",NVS_READWRITE,&n);if(err==ESP_OK){err=nvs_set_blob(n,"battery_v1",&c,sizeof(c));if(err==ESP_OK)err=nvs_commit(n);nvs_close(n);}if(err!=ESP_OK)return respond(r,"Falha ao salvar NVS");
 xSemaphoreTake(lock,portMAX_DELAY);config=c;policy=(battery_policy_t){0};valid=false;xSemaphoreGive(lock);return respond(r,NULL);
}
void battery_register(httpd_handle_t server){const httpd_uri_t get={.uri="/api/battery",.method=HTTP_GET,.handler=route},post={.uri="/api/battery",.method=HTTP_POST,.handler=route};ESP_ERROR_CHECK(httpd_register_uri_handler(server,&get));ESP_ERROR_CHECK(httpd_register_uri_handler(server,&post));}
