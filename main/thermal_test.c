#include "thermal_test.h"
#include "thermal_model.h"
#include "freertos/FreeRTOS.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_system.h"
#include "cJSON.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Separate bounded RAM recording: survives browser closure, not an ESP restart.
#define CAPACITY 2700
#define EVENT_CAPACITY 96
#define INVALID_TEMP INT16_MIN
typedef struct {uint32_t ms;int16_t ntc,average,coil_pred,ntc_pred;uint16_t raw,mv;uint8_t phase,averaged;} trial_sample_t;
typedef struct {uint32_t ms;uint8_t kind;float voltage,current,coil,body;} trial_event_t;
enum {IDLE,BASELINE,HEATING,COOLING,DONE,FULL};
static const char *phases[]={"idle","baseline","heating","cooling","done","full"};
static const char *kinds[]={"start","power_on","power_off","finish","reference","full","model_gap"};
static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
static trial_sample_t *samples;
static struct {
    uint32_t id;uint64_t started,last_model,last_sensor;size_t count,capacity,event_count;
    uint8_t phase;bool recording,model_valid;double voltage,limit,ambient;
    double initial_coil,initial_ntc;thermal_model_t model;trial_event_t events[EVENT_CAPACITY];char location[81];
} trial;
static double rolling[10];static size_t rolling_count,rolling_head;
static double latest_average=NAN;static uint64_t latest_at;
static double latest_temperature=NAN;
static int16_t centi(double v){return isfinite(v)&&v>-327&&v<327?(int16_t)lround(v*100):INVALID_TEMP;}
static void event_locked(uint64_t now,unsigned kind,float v,float i,float coil,float body){
    if(trial.event_count<EVENT_CAPACITY)trial.events[trial.event_count++]=(trial_event_t){.ms=(uint32_t)(now-trial.started),.kind=kind,.voltage=v,.current=i,.coil=coil,.body=body};
}
static void advance_locked(uint64_t now){
    if(!trial.recording)return;
    double dt=(now-trial.last_model)/1000.;trial.last_model=now;
    if(trial.model_valid&&!thermal_model_step(&trial.model,trial.voltage,trial.limit,trial.phase==HEATING,dt)){
        trial.model_valid=false;event_locked(now,6,NAN,NAN,NAN,NAN);
    }
}
void thermal_test_sample(uint64_t now,bool valid,double temperature,int raw,int mv){
    portENTER_CRITICAL(&lock);
    if(!valid||!isfinite(temperature)|| (latest_at&&now-latest_at>2500))rolling_count=rolling_head=0;
    latest_at=now;
    latest_temperature=valid?temperature:NAN;
    if(valid&&isfinite(temperature)){rolling[rolling_head]=temperature;rolling_head=(rolling_head+1)%10;if(rolling_count<10)rolling_count++;}
    latest_average=NAN;if(rolling_count){double sum=0;for(size_t i=0;i<rolling_count;i++)sum+=rolling[i];latest_average=sum/rolling_count;}
    if(trial.recording){
        advance_locked(now);trial.last_sensor=now;
        if(trial.count<trial.capacity){
            samples[trial.count++]=(trial_sample_t){.ms=(uint32_t)(now-trial.started),.ntc=valid?centi(temperature):INVALID_TEMP,
                .average=centi(latest_average),.averaged=(uint8_t)rolling_count,
                .raw=valid&&raw>=0?(uint16_t)raw:UINT16_MAX,.mv=valid&&mv>=0?(uint16_t)mv:UINT16_MAX,
                .coil_pred=trial.model_valid?centi(trial.ambient+trial.model.coil_delta):INVALID_TEMP,
                .ntc_pred=trial.model_valid?centi(trial.ambient+trial.model.ntc_delta):INVALID_TEMP,.phase=trial.phase};
        }
        if(trial.count==trial.capacity){event_locked(now,5,NAN,NAN,NAN,NAN);trial.recording=false;trial.phase=FULL;}
    }
    double average=latest_average;size_t averaged=rolling_count;unsigned phase=trial.phase;uint32_t session=trial.id;
    double cp=trial.recording&&trial.model_valid?trial.ambient+trial.model.coil_delta:NAN;
    double np=trial.recording&&trial.model_valid?trial.ambient+trial.model.ntc_delta:NAN;
    portEXIT_CRITICAL(&lock);
    ESP_LOGI("thermal_live","THERMAL t_ms=%llu valid=%d ntc_c=%.3f mean10_c=%.3f n=%u raw=%d mv=%d session=%u phase=%s coil_pred_c=%.3f ntc_pred_c=%.3f",
        (unsigned long long)now,valid,valid?temperature:NAN,average,(unsigned)averaged,raw,mv,(unsigned)session,phases[phase],cp,np);
}
static esp_err_t json_reply(httpd_req_t *r,cJSON *o){
    char *s=cJSON_PrintUnformatted(o);cJSON_Delete(o);if(!s)return ESP_ERR_NO_MEM;
    httpd_resp_set_type(r,"application/json");httpd_resp_set_hdr(r,"Cache-Control","no-store");esp_err_t e=httpd_resp_sendstr(r,s);free(s);return e;
}
static esp_err_t failure(httpd_req_t *r,const char *status,const char *text){
    httpd_resp_set_status(r,status);cJSON *o=cJSON_CreateObject();cJSON_AddStringToObject(o,"error",text);return json_reply(r,o);
}
static void num(cJSON *o,const char *key,double v){if(isfinite(v))cJSON_AddNumberToObject(o,key,v);else cJSON_AddNullToObject(o,key);}
static cJSON *status_object(void){
    // No JSON allocation, networking, or full-buffer copying inside critical sections.
    portENTER_CRITICAL(&lock);
    uint32_t id=trial.id;uint64_t start=trial.started,last=latest_at;size_t count=trial.count,capacity=trial.capacity,ec=trial.event_count,n=rolling_count;
    bool active=trial.recording,mvalid=trial.model_valid;unsigned phase=trial.phase;double v=trial.voltage,limit=trial.limit,ambient=trial.ambient,avg=latest_average;
    double current_temperature=latest_temperature,initial_coil=trial.initial_coil,initial_ntc=trial.initial_ntc;thermal_model_t model=trial.model;char location[81];memcpy(location,trial.location,sizeof(location));
    trial_sample_t sample={0};if(count)sample=samples[count-1];
    portEXIT_CRITICAL(&lock);
    uint64_t now=esp_timer_get_time()/1000;
    cJSON *o=cJSON_CreateObject();cJSON_AddStringToObject(o,"model_version","pre17_v1");cJSON_AddStringToObject(o,"storage","ram");
    cJSON_AddNumberToObject(o,"session_id",id);cJSON_AddNumberToObject(o,"uptime_ms",now);cJSON_AddNumberToObject(o,"start_ms",(double)start);
    cJSON_AddStringToObject(o,"phase",phases[phase]);cJSON_AddBoolToObject(o,"recording",active);cJSON_AddBoolToObject(o,"model_valid",mvalid);
    cJSON_AddNumberToObject(o,"elapsed_ms",start?(active?now-start:sample.ms):0);cJSON_AddNumberToObject(o,"samples",count);cJSON_AddNumberToObject(o,"capacity",capacity?capacity:CAPACITY);
    cJSON_AddNumberToObject(o,"events",ec);cJSON_AddNumberToObject(o,"event_capacity",EVENT_CAPACITY);
    num(o,"ntc_mean10_c",last&&now-last<2500?avg:NAN);cJSON_AddNumberToObject(o,"mean_count",last&&now-last<2500?n:0);
    num(o,"ntc_c",last&&now-last<2500?current_temperature:NAN);
    num(o,"coil_pred_c",count&&sample.coil_pred!=INVALID_TEMP?sample.coil_pred/100.:NAN);
    num(o,"ntc_pred_c",count&&sample.ntc_pred!=INVALID_TEMP?sample.ntc_pred/100.:NAN);
    num(o,"body_effect_c",count&&mvalid?model.body_effect:NAN);num(o,"ambient_c",ambient);num(o,"source_v",v);num(o,"current_limit_a",limit);
    num(o,"model_current_a",count&&mvalid&&phase==HEATING?thermal_model_current(&model,v,limit):0);
    num(o,"initial_coil_c",id?initial_coil:NAN);num(o,"initial_ntc_c",id?initial_ntc:NAN);
    cJSON_AddStringToObject(o,"reference_location",location);cJSON_AddNumberToObject(o,"free_heap_bytes",esp_get_free_heap_size());return o;
}
static esp_err_t status_get(httpd_req_t *r){return json_reply(r,status_object());}
static bool optional_number(cJSON *o,const char *key,double low,double high,float *out){
    cJSON *v=cJSON_GetObjectItemCaseSensitive(o,key);*out=NAN;if(!v||cJSON_IsNull(v))return true;
    if(!cJSON_IsNumber(v)||!isfinite(v->valuedouble)||v->valuedouble<low||v->valuedouble>high)return false;
    *out=(float)v->valuedouble;return true;
}
static esp_err_t command(httpd_req_t *r){
    char header[8];if(httpd_req_get_hdr_value_str(r,"X-BLDSP",header,sizeof(header))!=ESP_OK||strcmp(header,"1"))return failure(r,"403 Forbidden","Cabecalho ausente");
    if(r->content_len<1||r->content_len>512)return failure(r,"400 Bad Request","Corpo invalido");
    char body[513];int got=0;while(got<r->content_len){int n=httpd_req_recv(r,body+got,r->content_len-got);if(n<=0)return ESP_FAIL;got+=n;}body[got]=0;
    cJSON *o=cJSON_Parse(body),*a=cJSON_GetObjectItemCaseSensitive(o,"action");if(!cJSON_IsString(a)){cJSON_Delete(o);return failure(r,"400 Bad Request","Acao ausente");}
    char action[24];snprintf(action,sizeof(action),"%s",a->valuestring);
    float v,i,coil,frame,ambient,limit,initial_coil;
    bool valid=optional_number(o,"voltage",0,24,&v)&&optional_number(o,"current",0,10,&i)&&optional_number(o,"coil",-20,250,&coil)&&optional_number(o,"body",-20,200,&frame)&&optional_number(o,"ambient",0,60,&ambient)&&optional_number(o,"limit",0,10,&limit)&&optional_number(o,"initial_coil",0,200,&initial_coil);
    char location[81]="";cJSON *loc=cJSON_GetObjectItemCaseSensitive(o,"location");
    if(loc){if(!cJSON_IsString(loc)||strlen(loc->valuestring)>80||strpbrk(loc->valuestring,"\r\n"))valid=false;else strcpy(location,loc->valuestring);}
    cJSON_Delete(o);if(!valid)return failure(r,"400 Bad Request","Valor fora da faixa");
    if(!strcmp(action,"start")&&(!isfinite(v)||v<1||!isfinite(ambient)||!isfinite(limit)))return failure(r,"400 Bad Request","Informe tensao, limite de corrente e ambiente");
    if(!strcmp(action,"start")&&!samples){
        size_t capacity=CAPACITY;trial_sample_t *p=calloc(capacity,sizeof(*p));
        if(!p){capacity=1800;p=calloc(capacity,sizeof(*p));}
        if(!p)return failure(r,"503 Service Unavailable","Sem RAM para gravar o ensaio");
        portENTER_CRITICAL(&lock);samples=p;trial.capacity=capacity;portEXIT_CRITICAL(&lock);
    }
    uint64_t now=esp_timer_get_time()/1000;const char *err=NULL;
    portENTER_CRITICAL(&lock);
    if(!strcmp(action,"start")){
        if(trial.recording)err="Ensaio ja em andamento";
        else if(rolling_count<10||now-latest_at>2500)err="Aguarde 10 leituras validas do NTC";
        else{trial.id++;trial.started=trial.last_model=now;trial.count=trial.event_count=0;trial.phase=BASELINE;trial.recording=true;trial.model_valid=true;
            trial.voltage=v;trial.limit=limit;trial.ambient=ambient;trial.initial_ntc=latest_average;trial.initial_coil=isfinite(initial_coil)?initial_coil:latest_average;
            strcpy(trial.location,location);thermal_model_init_observed(&trial.model,ambient,trial.initial_coil,trial.initial_ntc);event_locked(now,0,v,limit,(float)trial.initial_coil,NAN);}
    }else if(!trial.recording)err="Nenhum ensaio em andamento";
    else if(trial.event_count>=EVENT_CAPACITY-1&&strcmp(action,"finish"))err="Limite de marcacoes; encerre o registro";
    else if(!strcmp(action,"on")){
        if(trial.phase!=BASELINE)err="Fonte ja marcada ou ensaio em resfriamento";
        else{advance_locked(now);trial.phase=HEATING;event_locked(now,1,trial.voltage,trial.limit,NAN,NAN);}
    }else if(!strcmp(action,"off")){
        if(trial.phase!=HEATING)err="Fonte nao marcada como ligada";
        else{advance_locked(now);trial.phase=COOLING;event_locked(now,2,0,0,NAN,NAN);}
    }else if(!strcmp(action,"finish")){
        advance_locked(now);event_locked(now,3,NAN,NAN,NAN,NAN);trial.recording=false;trial.phase=DONE;
    }else if(!strcmp(action,"reference")){
        if(!isfinite(v)&&!isfinite(i)&&!isfinite(coil)&&!isfinite(frame))err="Informe ao menos uma referencia";
        else event_locked(now,4,v,i,coil,frame);
    }else err="Acao invalida";
    portEXIT_CRITICAL(&lock);
    if(err)return failure(r,"409 Conflict",err);
    ESP_LOGI("thermal_test","MARK t_ms=%llu session=%u action=%s phase=%s source_v=%.3f limit_a=%.3f ambient_c=%.3f ref_v=%.3f ref_i=%.3f ref_coil_c=%.3f ref_body_c=%.3f samples=%u heap=%u",
        (unsigned long long)now,(unsigned)trial.id,action,phases[trial.phase],trial.voltage,trial.limit,trial.ambient,
        (double)v,(double)i,(double)coil,(double)frame,(unsigned)trial.count,(unsigned)esp_get_free_heap_size());return status_get(r);
}
static esp_err_t export_get(httpd_req_t *r){
    cJSON *meta=status_object();
    size_t count=(size_t)cJSON_GetObjectItemCaseSensitive(meta,"samples")->valuedouble;
    size_t events=(size_t)cJSON_GetObjectItemCaseSensitive(meta,"events")->valuedouble;
    char *s=cJSON_PrintUnformatted(meta);cJSON_Delete(meta);if(!s)return ESP_ERR_NO_MEM;
    size_t len=strlen(s);if(!len){free(s);return ESP_FAIL;}s[len-1]=0;
    httpd_resp_set_type(r,"application/json; charset=utf-8");httpd_resp_set_hdr(r,"Cache-Control","no-store");
    // Capture counts once: append-only sensor writes do not alter existing entries.
    // All HTTP mutations run on the same HTTP server task as this handler.
    esp_err_t err=httpd_resp_sendstr_chunk(r,s);free(s);if(err!=ESP_OK)return err;
    if((err=httpd_resp_sendstr_chunk(r,",\"records\":["))!=ESP_OK)return err;
    char buf[320],nt[24],avg[24],cp[24],np[24];
    for(size_t k=0;k<count;k++){
        portENTER_CRITICAL(&lock);trial_sample_t p=samples[k];portEXIT_CRITICAL(&lock);
        if(p.ntc==INVALID_TEMP)strcpy(nt,"null");else snprintf(nt,sizeof(nt),"%.2f",p.ntc/100.);
        if(p.average==INVALID_TEMP)strcpy(avg,"null");else snprintf(avg,sizeof(avg),"%.2f",p.average/100.);
        if(p.coil_pred==INVALID_TEMP)strcpy(cp,"null");else snprintf(cp,sizeof(cp),"%.2f",p.coil_pred/100.);
        if(p.ntc_pred==INVALID_TEMP)strcpy(np,"null");else snprintf(np,sizeof(np),"%.2f",p.ntc_pred/100.);
        snprintf(buf,sizeof(buf),"%s{\"ms\":%u,\"phase\":\"%s\",\"ntc_c\":%s,\"mean10_c\":%s,\"mean_count\":%u,\"raw\":%u,\"mv\":%u,\"coil_pred_c\":%s,\"ntc_pred_c\":%s}",k?",":"",(unsigned)p.ms,phases[p.phase],nt,avg,p.averaged,p.raw,p.mv,cp,np);
        if((err=httpd_resp_sendstr_chunk(r,buf))!=ESP_OK)return err;
    }
    if((err=httpd_resp_sendstr_chunk(r,"],\"marks\":["))!=ESP_OK)return err;
    for(size_t k=0;k<events;k++){
        portENTER_CRITICAL(&lock);trial_event_t p=trial.events[k];portEXIT_CRITICAL(&lock);
        cJSON *o=cJSON_CreateObject();cJSON_AddNumberToObject(o,"ms",p.ms);cJSON_AddStringToObject(o,"kind",kinds[p.kind]);num(o,"voltage",p.voltage);num(o,"current",p.current);num(o,"coil",p.coil);num(o,"body",p.body);
        s=cJSON_PrintUnformatted(o);cJSON_Delete(o);if(!s)return ESP_ERR_NO_MEM;
        if(k)err=httpd_resp_sendstr_chunk(r,",");
        if(err==ESP_OK)err=httpd_resp_sendstr_chunk(r,s);
        free(s);if(err!=ESP_OK)return err;
    }
    if((err=httpd_resp_sendstr_chunk(r,"]}"))!=ESP_OK)return err;
    return httpd_resp_send_chunk(r,NULL,0);
}
void thermal_test_register(httpd_handle_t server){
    const httpd_uri_t get={.uri="/api/thermal-test",.method=HTTP_GET,.handler=status_get};
    const httpd_uri_t post={.uri="/api/thermal-test",.method=HTTP_POST,.handler=command};
    const httpd_uri_t download={.uri="/api/thermal-test/export",.method=HTTP_GET,.handler=export_get};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server,&get));ESP_ERROR_CHECK(httpd_register_uri_handler(server,&post));ESP_ERROR_CHECK(httpd_register_uri_handler(server,&download));
}
