#include "clip_protection.h"
#include "protection_policy.h"
#include "telemetry.h"
#include "dsp_usb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static protection_policy_t policy;
static unsigned applied_q8;
static esp_err_t last_error;
static bool sensor_ready,sensor_warning,worker_ready;
typedef struct {uint32_t version;protection_config_t config;} stored_config_t;

static void worker(void *arg){
    (void)arg;uint64_t retry_after=0;
    while(1){
        uint64_t now=esp_timer_get_time()/1000;telemetry_clip_t clip;telemetry_clip_state(&clip);
        portENTER_CRITICAL(&mux);
        sensor_ready=clip.ready;sensor_warning=clip.sensor_warning;policy.recovery_inhibited=clip.sensor_warning;
        unsigned target=protection_policy_tick(&policy,now,clip.ready,clip.count,clip.sustained);
        bool changed=target!=applied_q8;
        portEXIT_CRITICAL(&mux);
        if(changed&&now>=retry_after){
            esp_err_t err=dsp_set_sub_attenuation(target);
            portENTER_CRITICAL(&mux);last_error=err;if(err==ESP_OK)applied_q8=target;portEXIT_CRITICAL(&mux);
            if(err!=ESP_OK)ESP_LOGW("clip_protection","Reduction pending %.2f dB: %s",target/256.,esp_err_to_name(err));
            retry_after=now+1000;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
void clip_protection_start(void){
    protection_config_t config=protection_defaults();nvs_handle_t store;
    if(nvs_open("clip_protect",NVS_READONLY,&store)==ESP_OK){
        stored_config_t saved;size_t n=sizeof(saved);
        if(nvs_get_blob(store,"config",&saved,&n)==ESP_OK&&n==sizeof(saved)&&saved.version==1&&protection_config_valid(&saved.config))config=saved.config;
        nvs_close(store);
    }
    protection_policy_init(&policy,config);
    worker_ready=xTaskCreate(worker,"clip_protection",4096,NULL,2,NULL)==pdPASS;
    ESP_LOGI("clip_protection","Sub-only protection enabled=%d; worker=%d; manual pregain and presets preserved",config.enabled,worker_ready);
}
static void add_config(cJSON *o,const protection_config_t *c){
    cJSON_AddBoolToObject(o,"enabled",c->enabled);cJSON_AddBoolToObject(o,"recovery_enabled",c->recovery_enabled);
    cJSON_AddNumberToObject(o,"event_count",c->event_count);cJSON_AddNumberToObject(o,"window_ms",c->window_ms);
    cJSON_AddNumberToObject(o,"interval_ms",c->interval_ms);cJSON_AddNumberToObject(o,"quiet_ms",c->quiet_ms);
    cJSON_AddNumberToObject(o,"recovery_interval_ms",c->recovery_interval_ms);
    cJSON_AddNumberToObject(o,"step_db",c->step_q8/256.);cJSON_AddNumberToObject(o,"max_atten_db",c->max_q8/256.);
    cJSON_AddNumberToObject(o,"recovery_step_db",c->recovery_step_q8/256.);
}
static cJSON *snapshot(void){
    portENTER_CRITICAL(&mux);
    protection_policy_t s=policy;unsigned applied=applied_q8;esp_err_t error=last_error;
    bool ready=sensor_ready,warning=sensor_warning,running=worker_ready;
    portEXIT_CRITICAL(&mux);
    cJSON *o=cJSON_CreateObject();add_config(cJSON_AddObjectToObject(o,"config"),&s.config);
    cJSON_AddNumberToObject(o,"attenuation_db",applied/256.);cJSON_AddNumberToObject(o,"target_attenuation_db",s.attenuation_q8/256.);
    cJSON_AddBoolToObject(o,"sensor_ready",ready);cJSON_AddBoolToObject(o,"sensor_warning",warning);
    cJSON_AddBoolToObject(o,"restoring",s.restoring);cJSON_AddStringToObject(o,"error",error==ESP_OK?"":esp_err_to_name(error));
    const char *state=!running?"unavailable":applied!=s.attenuation_q8?"pending":!ready?"waiting_sensor":warning?"sensor_warning":s.restoring?"recovering":!s.config.enabled?"disabled":applied?"reduced":"ready";
    cJSON_AddStringToObject(o,"state",state);return o;
}
void clip_protection_add_status(cJSON *parent){
    cJSON *o=snapshot();cJSON_AddItemToObject(parent,"protection",o);
    cJSON *enabled=cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(o,"config"),"enabled");
    cJSON_ReplaceItemInObject(parent,"automatic_protection",cJSON_CreateBool(worker_ready&&cJSON_IsTrue(enabled)));
}
static esp_err_t reply(httpd_req_t *r,cJSON *o){
    char *text=cJSON_PrintUnformatted(o);cJSON_Delete(o);
    if(!text)return ESP_ERR_NO_MEM;
    httpd_resp_set_type(r,"application/json");httpd_resp_set_hdr(r,"Cache-Control","no-store");
    esp_err_t err=httpd_resp_sendstr(r,text);free(text);return err;
}
static esp_err_t fail(httpd_req_t *r,const char *code,const char *message){
    httpd_resp_set_status(r,code);cJSON *o=cJSON_CreateObject();cJSON_AddStringToObject(o,"error",message);return reply(r,o);
}
static bool number(cJSON *o,const char *key,unsigned *out,bool db){
    cJSON *v=cJSON_GetObjectItemCaseSensitive(o,key);
    if(!cJSON_IsNumber(v)||!isfinite(v->valuedouble)||v->valuedouble<0||v->valuedouble>60000)return false;
    double value=db?round(v->valuedouble*256):v->valuedouble;
    if(value!=floor(value))return false;
    *out=(unsigned)value;return true;
}
static bool parse_config(cJSON *o,protection_config_t *c){
    cJSON *enabled=cJSON_GetObjectItemCaseSensitive(o,"enabled"),*recovery=cJSON_GetObjectItemCaseSensitive(o,"recovery_enabled");
    if(!cJSON_IsObject(o)||!cJSON_IsBool(enabled)||!cJSON_IsBool(recovery))return false;
    *c=(protection_config_t){.enabled=cJSON_IsTrue(enabled),.recovery_enabled=cJSON_IsTrue(recovery)};
    return number(o,"event_count",&c->event_count,false)&&number(o,"window_ms",&c->window_ms,false)
        &&number(o,"interval_ms",&c->interval_ms,false)&&number(o,"quiet_ms",&c->quiet_ms,false)
        &&number(o,"recovery_interval_ms",&c->recovery_interval_ms,false)&&number(o,"step_db",&c->step_q8,true)
        &&number(o,"max_atten_db",&c->max_q8,true)&&number(o,"recovery_step_db",&c->recovery_step_q8,true)&&protection_config_valid(c);
}
static esp_err_t handle(httpd_req_t *r){
    if(r->method==HTTP_GET)return reply(r,snapshot());
    char header[8],body[1025];
    if(httpd_req_get_hdr_value_str(r,"X-BLDSP",header,sizeof(header))!=ESP_OK||strcmp(header,"1"))return fail(r,"403 Forbidden","Cabecalho ausente");
    if(r->content_len<1||r->content_len>1024)return fail(r,"400 Bad Request","Corpo invalido");
    int got=0;while(got<r->content_len){int n=httpd_req_recv(r,body+got,r->content_len-got);if(n<=0)return ESP_FAIL;got+=n;}body[got]=0;
    cJSON *o=cJSON_Parse(body),*action=cJSON_GetObjectItemCaseSensitive(o,"action");
    if(cJSON_IsString(action)&&!strcmp(action->valuestring,"restore")){
        portENTER_CRITICAL(&mux);protection_policy_restore(&policy);portEXIT_CRITICAL(&mux);cJSON_Delete(o);
        return reply(r,snapshot());
    }
    protection_config_t config;
    bool valid=cJSON_IsString(action)&&!strcmp(action->valuestring,"configure")&&parse_config(cJSON_GetObjectItemCaseSensitive(o,"config"),&config);
    cJSON_Delete(o);if(!valid)return fail(r,"400 Bad Request","Parametros de protecao invalidos");
    stored_config_t saved={.version=1,.config=config};nvs_handle_t store;
    esp_err_t err=nvs_open("clip_protect",NVS_READWRITE,&store);
    if(err==ESP_OK){err=nvs_set_blob(store,"config",&saved,sizeof(saved));if(err==ESP_OK)err=nvs_commit(store);nvs_close(store);}
    if(err!=ESP_OK)return fail(r,"500 Internal Server Error","Falha ao salvar protecao");
    portENTER_CRITICAL(&mux);protection_policy_configure(&policy,config);portEXIT_CRITICAL(&mux);
    ESP_LOGI("clip_protection","Config saved: enabled=%d events=%u/%ums step=%.2f max=%.2f recovery=%d",config.enabled,config.event_count,config.window_ms,config.step_q8/256.,config.max_q8/256.,config.recovery_enabled);
    return reply(r,snapshot());
}
void clip_protection_register(httpd_handle_t server){
    const httpd_uri_t get={.uri="/api/protection",.method=HTTP_GET,.handler=handle};
    const httpd_uri_t post={.uri="/api/protection",.method=HTTP_POST,.handler=handle};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server,&get));ESP_ERROR_CHECK(httpd_register_uri_handler(server,&post));
}
