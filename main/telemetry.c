#include "telemetry.h"
#include "telemetry_math.h"
#include "clip_detector.h"
#include "clip_health.h"
#include "clip_alert.h"
#include "clip_protection.h"
#include "panel_control.h"
#include "wifi_power.h"
#include "thermal_test.h"
#include "thermal_observer.h"
#include "monitor_log.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "cJSON.h"
#include "source_control.h"
#include "status_led.h"

// 3V3 -> fixed 10k -> ADC -> grounded sensor. 17=LDR, 18=NTC.
static adc_oneshot_unit_handle_t adc;
static adc_cali_handle_t calibration[2];
static const adc_channel_t channels[]={ADC_CHANNEL_6,ADC_CHANNEL_7};
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static monitor_log_t history_log;
static clip_detector_t clip_detector;
static clip_health_t clip_health;
static clip_alert_t clip_alert;
static thermal_observer_t observer;
static struct {
    int raw[2],mv[2];bool valid[2];int64_t sampled[2];
    double baseline,threshold,release;bool clip_ready,clip_active,rail_high,flash;unsigned clips;uint32_t clip_low_ms;int64_t clip_sampled,last_clip;
} readings;
static void sensor_task(void *arg) {
    (void)arg;unsigned report=0;int previous_ntc=-1,previous_ldr=-1;int64_t next_sample=0;TickType_t cadence=xTaskGetTickCount();
    while(1){
        for(int ch=0;ch<2;ch++){
            int sum=0,count=0;
            for(int i=0;i<4;i++){int raw;if(adc_oneshot_read(adc,channels[ch],&raw)==ESP_OK){sum+=raw;count++;}}
            int value=count?sum/count:0,mv=-1;
            bool valid=count>=2;
            if(valid&&calibration[ch]&&adc_cali_raw_to_voltage(calibration[ch],value,&mv)!=ESP_OK)mv=-1;
            int64_t now=esp_timer_get_time();bool edge=false,trial_due=false;
            double resistance=0,temperature=0;
            bool temperature_valid=ch==1&&valid&&mv>=0&&telemetry_ntc_estimate(mv,&resistance,&temperature);
            portENTER_CRITICAL(&mux);
            readings.valid[ch]=valid;readings.raw[ch]=value;readings.mv[ch]=mv;
            if(valid)readings.sampled[ch]=now;
            if(ch==0){
                monitor_log_raw(&history_log,now/1000,valid?value:-1);
                bool previous=readings.clip_active;
                clip_health_sample(&clip_health,(uint32_t)(now/1000),valid,value);
                readings.rail_high=clip_health.rail_high;
                clip_detector_sample(&clip_detector,(uint32_t)(now/1000),valid,value);
                readings.baseline=clip_detector.baseline;readings.threshold=clip_detector.trigger;
                readings.release=clip_detector.release;readings.clip_ready=clip_detector.ready;readings.clip_active=clip_detector.active;
                clip_alert_sample(&clip_alert,now/1000,clip_detector.ready,clip_detector.hard_pulse,clip_detector.long_confirmed);readings.flash=clip_alert.flash;
                if(previous!=readings.clip_active){
                    monitor_log_event(&history_log,now/1000,readings.clip_active?MONITOR_CLIP_ON:MONITOR_CLIP_OFF,value);
                    if(readings.clip_active){readings.clips++;edge=true;clip_health.rail_high=false;clip_health.timing_rail=false;readings.rail_high=false;}
                }
                uint32_t delta=readings.clip_sampled?(uint32_t)((now-readings.clip_sampled)/1000):0;
                if(delta<=100&&readings.clip_active&&value<readings.threshold)readings.clip_low_ms+=delta;
                readings.clip_sampled=now;
                if(readings.clip_active)readings.last_clip=now;
                if(previous_ldr!=(int)valid){monitor_log_event(&history_log,now/1000,valid?MONITOR_LDR_VALID:MONITOR_LDR_INVALID,0);previous_ldr=valid;}
            }else{
                if(previous_ntc!=(int)temperature_valid){monitor_log_event(&history_log,now/1000,temperature_valid?MONITOR_NTC_VALID:MONITOR_NTC_INVALID,0);previous_ntc=temperature_valid;}
                if(now>=next_sample){
                    thermal_observer_sample(&observer,now/1000,temperature_valid,temperature);
                    monitor_sample_t sample={.at_ms=now/1000,.temperature_c=(float)temperature,.temperature_valid=temperature_valid,
                        .ldr_mv=readings.valid[0]?readings.mv[0]:-1,
                        .clip=readings.valid[0]&&readings.clip_ready?(int8_t)readings.clip_active:-1};
                    monitor_log_sample(&history_log,sample);next_sample=now+1000000;
                    trial_due=true;
                }
            }
            portEXIT_CRITICAL(&mux);
            if(ch==0){
                portENTER_CRITICAL(&mux);__typeof__(readings) trace=readings;bool candidate=clip_detector.candidate,sustained=clip_detector.long_confirmed;unsigned low_run=clip_detector.low_run;portEXIT_CRITICAL(&mux);
                status_led_clip(valid&&trace.clip_active);
                ESP_LOGI("clip_raw","CLIP_RAW t=%llu r=%d b=%.1f ready=%d a=%d n=%u cand=%d low=%u sustained=%d",(unsigned long long)(now/1000),valid?value:-1,trace.baseline,trace.clip_ready,trace.clip_active,trace.clips,candidate,low_run,sustained);
            }
            if(trial_due){
                thermal_test_sample(now/1000,temperature_valid,temperature,value,mv);
                portENTER_CRITICAL(&mux);thermal_observer_t est=observer;portEXIT_CRITICAL(&mux);
                ESP_LOGI("thermal_observer","ESTIMATE t_ms=%llu model=joint_v2_ntc valid=%d mean10_c=%.3f coil_c=%.3f ambient_c=%.2f",(unsigned long long)(now/1000),est.valid,est.mean,est.valid?est.coil:NAN,est.ambient);
            }
            if(edge)ESP_LOGW("sensors","CLIP LED detected; no automatic gain intervention");
        }
        if(++report>=250){
            portENTER_CRITICAL(&mux);__typeof__(readings) s=readings;size_t samples=history_log.sample_count,events=history_log.event_count,raw_count=history_log.raw_count;portEXIT_CRITICAL(&mux);
            ESP_LOGI("sensors","GPIO17 raw=%d mv=%d valid=%d; GPIO18 raw=%d mv=%d valid=%d; CLIP relative60s=%d limit=%.1f count=%u; history=%u events=%u raw_window=%u",s.raw[0],s.mv[0],s.valid[0],s.raw[1],s.mv[1],s.valid[1],s.clip_ready,s.threshold,s.clips,(unsigned)samples,(unsigned)events,(unsigned)raw_count);
            report=0;
        }
        vTaskDelayUntil(&cadence,pdMS_TO_TICKS(20));
    }
}
void telemetry_start(void) {
    clip_detector_reset(&clip_detector);
    thermal_observer_init(&observer,27.8);
    monitor_log_event(&history_log,esp_timer_get_time()/1000,MONITOR_STARTED,0);
    nvs_handle_t store;
    if(nvs_open("sensors",NVS_READONLY,&store)==ESP_OK){
        int32_t value;if(nvs_get_i32(store,"ambient_centi",&value)==ESP_OK&&value>=0&&value<=6000)thermal_observer_init(&observer,value/100.);
        clip_config_t saved;size_t len=sizeof(saved);
        if(nvs_get_blob(store,"clip_cfg_v1",&saved,&len)==ESP_OK && len==sizeof(saved) && clip_config_valid(&saved))clip_detector_configure(&clip_detector,&saved);
        nvs_close(store);
    }
    ESP_LOGI("clip_config","Loaded detector: drop=%lu%% edge=%lu ms mean=%lu ms min=%lu sustained=%lu hard=%lu",(unsigned long)clip_detector.config.drop_percent,(unsigned long)clip_detector.config.edge_ms,(unsigned long)clip_detector.config.window_ms,(unsigned long)clip_detector.config.min_low_samples,(unsigned long)clip_detector.config.sustained_samples,(unsigned long)clip_detector.config.hard_min_samples);
    const adc_oneshot_unit_init_cfg_t unit={.unit_id=ADC_UNIT_2};
    esp_err_t err=adc_oneshot_new_unit(&unit,&adc);
    if(err!=ESP_OK){monitor_log_event(&history_log,esp_timer_get_time()/1000,MONITOR_ADC_ERROR,err);ESP_LOGE("sensors","ADC2 unavailable: %s",esp_err_to_name(err));return;}
    const adc_oneshot_chan_cfg_t config={.atten=ADC_ATTEN_DB_12,.bitwidth=ADC_BITWIDTH_12};
    for(int i=0;i<2;i++){
        err=adc_oneshot_config_channel(adc,channels[i],&config);
        if(err!=ESP_OK){ESP_LOGE("sensors","ADC channel setup: %s",esp_err_to_name(err));return;}
        adc_cali_curve_fitting_config_t cal={.unit_id=ADC_UNIT_2,.chan=channels[i],.atten=ADC_ATTEN_DB_12,.bitwidth=ADC_BITWIDTH_12};
        err=adc_cali_create_scheme_curve_fitting(&cal,&calibration[i]);
        if(err!=ESP_OK){calibration[i]=NULL;ESP_LOGW("sensors","Channel %d voltage calibration unavailable",17+i);}
    }
    if(xTaskCreate(sensor_task,"sensors",3072,NULL,2,NULL)!=pdPASS){ESP_LOGE("sensors","Cannot start sensor task");return;}
    ESP_LOGI("sensors","LDR GPIO17, NTC GPIO18; low-side sensors, 10k pull-ups, NTC B3950 estimated; monitoring only");
}
static void add_nullable(cJSON *o,const char *key,bool valid,double value){if(valid)cJSON_AddNumberToObject(o,key,value);else cJSON_AddNullToObject(o,key);}
static esp_err_t send_json(httpd_req_t *r,cJSON *o){
    char *s=cJSON_PrintUnformatted(o);httpd_resp_set_type(r,"application/json");httpd_resp_set_hdr(r,"Cache-Control","no-store");
    esp_err_t result=httpd_resp_sendstr(r,s);free(s);cJSON_Delete(o);return result;
}
static esp_err_t fail(httpd_req_t *r,const char *code,const char *message){httpd_resp_set_status(r,code);cJSON *o=cJSON_CreateObject();cJSON_AddStringToObject(o,"error",message);return send_json(r,o);}
typedef struct {httpd_req_t *req;char bytes[1024];size_t used;esp_err_t error;} history_writer_t;
static void history_flush(history_writer_t *w){
    if(w->error==ESP_OK&&w->used)w->error=httpd_resp_send_chunk(w->req,w->bytes,w->used);
    w->used=0;
}
static void history_write(history_writer_t *w,const char *text){
    size_t n=strlen(text);if(w->error!=ESP_OK)return;
    if(n>sizeof(w->bytes)){w->error=ESP_ERR_INVALID_SIZE;return;}
    if(w->used+n>sizeof(w->bytes))history_flush(w);
    if(w->error==ESP_OK){memcpy(w->bytes+w->used,text,n);w->used+=n;}
}
static esp_err_t history_get(httpd_req_t *r){
    monitor_log_t *snapshot=malloc(sizeof(*snapshot));
    if(!snapshot)return fail(r,"503 Service Unavailable","Memoria indisponivel para historico");
    portENTER_CRITICAL(&mux);memcpy(snapshot,&history_log,sizeof(*snapshot));portEXIT_CRITICAL(&mux);
    ESP_LOGI("sensors","Serving stored history: %u samples, %u events",(unsigned)snapshot->sample_count,(unsigned)snapshot->event_count);
    httpd_resp_set_type(r,"application/json");httpd_resp_set_hdr(r,"Cache-Control","no-store");
    history_writer_t writer={.req=r,.error=ESP_OK};char entry[192],temperature[32],ldr[16];
    snprintf(entry,sizeof(entry),"{\"uptime_ms\":%llu,\"window_ms\":300000,\"sample_interval_ms\":1000,\"storage\":\"ram\",\"samples\":[",(unsigned long long)(esp_timer_get_time()/1000));
    history_write(&writer,entry);
    for(size_t i=0;i<snapshot->sample_count&&writer.error==ESP_OK;i++){
        const monitor_sample_t *s=monitor_log_sample_at(snapshot,i);
        if(s->temperature_valid)snprintf(temperature,sizeof(temperature),"%.2f",(double)s->temperature_c);else strcpy(temperature,"null");
        if(s->ldr_mv>=0)snprintf(ldr,sizeof(ldr),"%d",s->ldr_mv);else strcpy(ldr,"null");
        snprintf(entry,sizeof(entry),"%s{\"at_ms\":%llu,\"temperature_c\":%s,\"ldr_mv\":%s,\"clip\":%s}",i?",":"",(unsigned long long)s->at_ms,temperature,ldr,s->clip<0?"null":s->clip?"true":"false");
        history_write(&writer,entry);
    }
    history_write(&writer,"],\"events\":[");
    for(size_t i=0;i<snapshot->event_count&&writer.error==ESP_OK;i++){
        const monitor_event_t *e=monitor_log_event_at(snapshot,i);
        snprintf(entry,sizeof(entry),"%s{\"at_ms\":%llu,\"code\":\"%s\",\"detail\":%d}",i?",":"",(unsigned long long)e->at_ms,monitor_event_name(e->code),e->detail);
        history_write(&writer,entry);
    }
    history_write(&writer,"]}");history_flush(&writer);free(snapshot);
    return writer.error==ESP_OK?httpd_resp_send_chunk(r,NULL,0):writer.error;
}
static esp_err_t get(httpd_req_t *r) {
    portENTER_CRITICAL(&mux);__typeof__(readings) s=readings;thermal_observer_t estimate=observer;clip_config_t cc=clip_detector.config;portEXIT_CRITICAL(&mux);
    int64_t now=esp_timer_get_time();
    bool valid[2]={s.valid[0]&&now-s.sampled[0]<500000,s.valid[1]&&now-s.sampled[1]<500000};
    double resistance=0,temp=0;bool temperature_ok=valid[1]&&s.mv[1]>=0&&telemetry_ntc_estimate(s.mv[1],&resistance,&temp);
    bool ldr_ready=s.clip_ready;
    cJSON *o=cJSON_CreateObject();cJSON_AddNumberToObject(o,"uptime_ms",now/1000);cJSON_AddBoolToObject(o,"automatic_protection",false);
    cJSON_AddBoolToObject(o,"bluetooth_mode",source_control_ready());
    cJSON *t=cJSON_AddObjectToObject(o,"temperature");
    cJSON_AddStringToObject(t,"status",!valid[1]?"unavailable":s.mv[1]<0?"adc_uncalibrated":temperature_ok?"estimated":"out_of_range");
    add_nullable(t,"celsius",temperature_ok,temp);add_nullable(t,"resistance_ohm",temperature_ok,resistance);
    add_nullable(t,"raw",valid[1],s.raw[1]);add_nullable(t,"millivolts",valid[1]&&s.mv[1]>=0,s.mv[1]);
    cJSON_AddNumberToObject(t,"gpio",18);cJSON_AddNumberToObject(t,"beta",3950);
    cJSON *coil=cJSON_AddObjectToObject(o,"coil");
    bool fresh=estimate.initialized&&now/1000-estimate.last_ms<2500;
    bool coil_valid=temperature_ok&&fresh&&estimate.valid;
    cJSON_AddStringToObject(coil,"model","joint_v2_ntc_observer");
    cJSON_AddStringToObject(coil,"status",coil_valid?"estimated":fresh?"warming_up":"unavailable");
    add_nullable(coil,"celsius",coil_valid,estimate.coil);
    add_nullable(coil,"ntc_mean10_c",temperature_ok&&fresh&&estimate.count==10,estimate.mean);
    cJSON_AddNumberToObject(coil,"ambient_c",estimate.ambient);
    cJSON_AddNumberToObject(coil,"samples",estimate.count);
    cJSON *c=cJSON_AddObjectToObject(o,"clip");
    cJSON_AddStringToObject(c,"status",!valid[0]?"unavailable":s.rail_high?"rail_high":ldr_ready?"ok":"warming_up");
    if(valid[0]&&ldr_ready)cJSON_AddBoolToObject(c,"active",s.last_clip>0&&now-s.last_clip<2000000);else cJSON_AddNullToObject(c,"active");
    add_nullable(c,"raw",valid[0],s.raw[0]);add_nullable(c,"millivolts",valid[0]&&s.mv[0]>=0,s.mv[0]);
    cJSON_AddBoolToObject(c,"flash",valid[0]&&s.flash);
    cJSON_AddNumberToObject(c,"hard_min_samples",cc.hard_min_samples);cJSON_AddNumberToObject(c,"hard_window_ms",5000);
    cJSON_AddNumberToObject(c,"count",s.clips);cJSON_AddNumberToObject(c,"low_ms",s.clip_low_ms);
    add_nullable(c,"baseline_raw",valid[0]&&s.baseline>=1,s.baseline);
    add_nullable(c,"threshold",valid[0]&&ldr_ready,s.threshold);
    add_nullable(c,"release_raw",valid[0]&&ldr_ready,s.release);
    cJSON_AddStringToObject(c,"mode","relative");cJSON_AddNumberToObject(c,"window_ms",cc.window_ms);cJSON_AddNumberToObject(c,"edge_window_ms",cc.edge_ms);cJSON_AddNumberToObject(c,"min_low_samples",cc.min_low_samples);cJSON_AddNumberToObject(c,"sustained_samples",cc.sustained_samples);
    cJSON_AddNumberToObject(c,"drop_percent",cc.drop_percent);cJSON_AddNumberToObject(c,"release_drop_percent",cc.release_drop_percent);
    cJSON_AddNumberToObject(c,"gpio",17);clip_protection_add_status(o);panel_add_status(o);wifi_power_add_status(o);return send_json(r,o);
}
static esp_err_t raw_capture(httpd_req_t *r){
    monitor_log_t *snapshot=malloc(sizeof(*snapshot));
    if(!snapshot)return fail(r,"503 Service Unavailable","Memoria indisponivel para captura");
    portENTER_CRITICAL(&mux);memcpy(snapshot,&history_log,sizeof(*snapshot));double trigger=readings.threshold;bool ready=readings.clip_ready;portEXIT_CRITICAL(&mux);
    uint64_t now=esp_timer_get_time()/1000;
    httpd_resp_set_type(r,"application/json");httpd_resp_set_hdr(r,"Cache-Control","no-store");
    history_writer_t writer={.req=r,.error=ESP_OK};char entry[256],limit[24];
    if(ready)snprintf(limit,sizeof(limit),"%.2f",trigger);else strcpy(limit,"null");
    snprintf(entry,sizeof(entry),"{\"uptime_ms\":%llu,\"window_ms\":2000,\"sample_interval_ms\":20,\"threshold\":%s,\"samples\":[",(unsigned long long)now,limit);history_write(&writer,entry);
    bool first=true;
    for(size_t i=0;i<snapshot->raw_count&&writer.error==ESP_OK;i++){
        const monitor_raw_t *s=monitor_log_raw_at(snapshot,i);if(now>s->at_ms&&now-s->at_ms>2000)continue;
        if(s->raw>=0)snprintf(entry,sizeof(entry),"%s{\"at_ms\":%llu,\"raw\":%d}",first?"":",",(unsigned long long)s->at_ms,s->raw);
        else snprintf(entry,sizeof(entry),"%s{\"at_ms\":%llu,\"raw\":null}",first?"":",",(unsigned long long)s->at_ms);
        history_write(&writer,entry);first=false;
    }
    history_write(&writer,"]}");history_flush(&writer);free(snapshot);
    ESP_LOGI("sensors","Served frozen 2-second RAW capture");
    return writer.error==ESP_OK?httpd_resp_send_chunk(r,NULL,0):writer.error;
}
static esp_err_t legacy_ldr_config(httpd_req_t *r){
    char header[8],body[513];
    if(httpd_req_get_hdr_value_str(r,"X-BLDSP",header,sizeof(header))!=ESP_OK||strcmp(header,"1"))return fail(r,"403 Forbidden","Cabecalho ausente");
    if(r->content_len<1||r->content_len>512)return fail(r,"400 Bad Request","Corpo invalido");
    int got=0;while(got<r->content_len){int n=httpd_req_recv(r,body+got,r->content_len-got);if(n<=0)return ESP_FAIL;got+=n;}body[got]=0;
    cJSON *o=cJSON_Parse(body);clip_config_t c={0};
    const char *keys[]={"window_ms","edge_window_ms","drop_percent","release_drop_percent","min_low_samples","sustained_samples","hard_min_samples"};
    uint32_t *fields[]={&c.window_ms,&c.edge_ms,&c.drop_percent,&c.release_drop_percent,&c.min_low_samples,&c.sustained_samples,&c.hard_min_samples};
    bool valid=true;
    for(unsigned i=0;i<7;i++){cJSON *v=cJSON_GetObjectItemCaseSensitive(o,keys[i]);if(!cJSON_IsNumber(v)||!isfinite(v->valuedouble)||v->valuedouble<0||v->valuedouble>60000||floor(v->valuedouble)!=v->valuedouble){valid=false;break;}*fields[i]=(uint32_t)v->valuedouble;}
    cJSON_Delete(o);if(!valid||!clip_config_valid(&c))return fail(r,"400 Bad Request","Parametros invalidos: recuperacao deve ser menor que queda; sustentado >= minimo");
    nvs_handle_t store;esp_err_t err=nvs_open("sensors",NVS_READWRITE,&store);
    if(err==ESP_OK){err=nvs_set_blob(store,"clip_cfg_v1",&c,sizeof(c));if(err==ESP_OK)err=nvs_commit(store);nvs_close(store);}
    if(err!=ESP_OK)return fail(r,"500 Internal Server Error","Falha ao salvar detector");
    portENTER_CRITICAL(&mux);clip_detector_configure(&clip_detector,&c);memset(&clip_alert,0,sizeof(clip_alert));readings.clip_ready=false;readings.clip_active=false;readings.flash=false;readings.last_clip=0;portEXIT_CRITICAL(&mux);
    ESP_LOGI("clip_config","Saved NVS: window=%lu edge=%lu drop=%lu release=%lu min=%lu sustained=%lu hard=%lu; reference restarted",(unsigned long)c.window_ms,(unsigned long)c.edge_ms,(unsigned long)c.drop_percent,(unsigned long)c.release_drop_percent,(unsigned long)c.min_low_samples,(unsigned long)c.sustained_samples,(unsigned long)c.hard_min_samples);
    return get(r);
}
static esp_err_t thermal_config(httpd_req_t *r){
    char header[8],body[65];
    if(httpd_req_get_hdr_value_str(r,"X-BLDSP",header,sizeof(header))!=ESP_OK||strcmp(header,"1"))return fail(r,"403 Forbidden","Cabecalho ausente");
    if(r->content_len<1||r->content_len>64)return fail(r,"400 Bad Request","Corpo invalido");
    int got=0;while(got<r->content_len){int n=httpd_req_recv(r,body+got,r->content_len-got);if(n<=0)return ESP_FAIL;got+=n;}body[got]=0;
    cJSON *o=cJSON_Parse(body),*v=cJSON_GetObjectItemCaseSensitive(o,"ambient_c");
    bool ok=cJSON_IsNumber(v)&&isfinite(v->valuedouble)&&v->valuedouble>=0&&v->valuedouble<=60;
    int centi=ok?(int)lround(v->valuedouble*100):0;cJSON_Delete(o);
    if(!ok)return fail(r,"400 Bad Request","Ambiente deve ficar entre 0 e 60 graus");
    nvs_handle_t store;esp_err_t err=nvs_open("sensors",NVS_READWRITE,&store);
    if(err==ESP_OK){err=nvs_set_i32(store,"ambient_centi",centi);if(err==ESP_OK)err=nvs_commit(store);nvs_close(store);}
    if(err!=ESP_OK)return fail(r,"500 Internal Server Error","Falha ao salvar ambiente");
    portENTER_CRITICAL(&mux);thermal_observer_init(&observer,centi/100.);portEXIT_CRITICAL(&mux);
    ESP_LOGI("thermal_observer","Ambient set %.2f C; observer restarted",centi/100.);return get(r);
}
void telemetry_register(httpd_handle_t server){
    const httpd_uri_t thermal={.uri="/api/telemetry/thermal",.method=HTTP_POST,.handler=thermal_config};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server,&thermal));
    const httpd_uri_t read={.uri="/api/telemetry",.method=HTTP_GET,.handler=get};
    const httpd_uri_t cal={.uri="/api/telemetry/ldr",.method=HTTP_POST,.handler=legacy_ldr_config};
    const httpd_uri_t history={.uri="/api/telemetry/history",.method=HTTP_GET,.handler=history_get};
    const httpd_uri_t capture={.uri="/api/telemetry/ldr/capture",.method=HTTP_GET,.handler=raw_capture};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server,&read));ESP_ERROR_CHECK(httpd_register_uri_handler(server,&cal));ESP_ERROR_CHECK(httpd_register_uri_handler(server,&history));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server,&capture));
}

void telemetry_clip_state(telemetry_clip_t *out){
    int64_t now=esp_timer_get_time();
    portENTER_CRITICAL(&mux);
    *out=(telemetry_clip_t){.ready=readings.valid[0]&&now-readings.sampled[0]<500000&&readings.clip_ready,
        .sustained=clip_detector.long_confirmed,.sensor_warning=readings.rail_high,.lit=readings.clip_active&&readings.raw[0]<readings.threshold,.count=readings.clips,.low_ms=readings.clip_low_ms};
    portEXIT_CRITICAL(&mux);
}
