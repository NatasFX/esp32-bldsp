#include "telemetry.h"
#include "telemetry_math.h"
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

// 3V3 -> fixed 10k -> ADC -> grounded sensor. 17=LDR, 18=NTC.
static adc_oneshot_unit_handle_t adc;
static adc_cali_handle_t calibration[2];
static const adc_channel_t channels[]={ADC_CHANNEL_6,ADC_CHANNEL_7};
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static struct {
    int raw[2],mv[2];bool valid[2];int64_t sampled[2];
    int dark,lit;bool led_on;unsigned debounce,clips;int64_t last_clip;
} readings={.dark=-1,.lit=-1};
static bool calibrated_ldr(int dark,int lit){return dark>=0&&lit>=0&&dark-lit>=100;}
static void sensor_task(void *arg) {
    (void)arg;unsigned report=0;
    while(1){
        for(int ch=0;ch<2;ch++){
            int sum=0,count=0;
            for(int i=0;i<4;i++){int raw;if(adc_oneshot_read(adc,channels[ch],&raw)==ESP_OK){sum+=raw;count++;}}
            int value=count?sum/count:0,mv=-1;
            bool valid=count>=2;
            if(valid&&calibration[ch]&&adc_cali_raw_to_voltage(calibration[ch],value,&mv)!=ESP_OK)mv=-1;
            int64_t now=esp_timer_get_time();bool edge=false;
            portENTER_CRITICAL(&mux);
            readings.valid[ch]=valid;readings.raw[ch]=value;readings.mv[ch]=mv;
            if(valid)readings.sampled[ch]=now;
            if(ch==0){
                if(valid&&calibrated_ldr(readings.dark,readings.lit)){
                    int mid=(readings.dark+readings.lit)/2,hyst=(readings.dark-readings.lit)/10;
                    bool next=readings.led_on?value<mid+hyst:value<mid-hyst;
                    if(next!=readings.led_on){
                        if(++readings.debounce>=2){readings.led_on=next;readings.debounce=0;if(next){readings.clips++;edge=true;}}
                    }else readings.debounce=0;
                    if(readings.led_on)readings.last_clip=now;
                }else readings.debounce=0;
            }
            portEXIT_CRITICAL(&mux);
            if(edge)ESP_LOGW("sensors","CLIP LED detected; no automatic gain intervention");
        }
        if(++report>=250){
            portENTER_CRITICAL(&mux);__typeof__(readings) s=readings;portEXIT_CRITICAL(&mux);
            ESP_LOGI("sensors","GPIO17 raw=%d mv=%d valid=%d; GPIO18 raw=%d mv=%d valid=%d; CLIP calibrated=%d count=%u",s.raw[0],s.mv[0],s.valid[0],s.raw[1],s.mv[1],s.valid[1],calibrated_ldr(s.dark,s.lit),s.clips);
            report=0;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
void telemetry_start(void) {
    nvs_handle_t store;
    if(nvs_open("sensors",NVS_READONLY,&store)==ESP_OK){
        int32_t value;if(nvs_get_i32(store,"ldr_dark",&value)==ESP_OK)readings.dark=value;
        if(nvs_get_i32(store,"ldr_lit",&value)==ESP_OK)readings.lit=value;
        nvs_close(store);
        if(readings.dark>4095||readings.lit>4095){readings.dark=-1;readings.lit=-1;}
    }
    const adc_oneshot_unit_init_cfg_t unit={.unit_id=ADC_UNIT_2};
    esp_err_t err=adc_oneshot_new_unit(&unit,&adc);
    if(err!=ESP_OK){ESP_LOGE("sensors","ADC2 unavailable: %s",esp_err_to_name(err));return;}
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
static esp_err_t get(httpd_req_t *r) {
    portENTER_CRITICAL(&mux);__typeof__(readings) s=readings;portEXIT_CRITICAL(&mux);
    int64_t now=esp_timer_get_time();
    bool valid[2]={s.valid[0]&&now-s.sampled[0]<500000,s.valid[1]&&now-s.sampled[1]<500000};
    double resistance=0,temp=0;bool temperature_ok=valid[1]&&s.mv[1]>=0&&telemetry_ntc_estimate(s.mv[1],&resistance,&temp);
    bool ldr_ready=calibrated_ldr(s.dark,s.lit);
    cJSON *o=cJSON_CreateObject();cJSON_AddNumberToObject(o,"uptime_ms",now/1000);cJSON_AddBoolToObject(o,"automatic_protection",false);
    cJSON_AddBoolToObject(o,"bluetooth_mode",source_control_ready());
    cJSON *t=cJSON_AddObjectToObject(o,"temperature");
    cJSON_AddStringToObject(t,"status",!valid[1]?"unavailable":s.mv[1]<0?"adc_uncalibrated":temperature_ok?"estimated":"out_of_range");
    add_nullable(t,"celsius",temperature_ok,temp);add_nullable(t,"resistance_ohm",temperature_ok,resistance);
    add_nullable(t,"raw",valid[1],s.raw[1]);add_nullable(t,"millivolts",valid[1]&&s.mv[1]>=0,s.mv[1]);
    cJSON_AddNumberToObject(t,"gpio",18);cJSON_AddNumberToObject(t,"beta",3950);
    cJSON *c=cJSON_AddObjectToObject(o,"clip");
    cJSON_AddStringToObject(c,"status",!valid[0]?"unavailable":ldr_ready?"ok":"uncalibrated");
    if(valid[0]&&ldr_ready)cJSON_AddBoolToObject(c,"active",s.last_clip>0&&now-s.last_clip<2000000);else cJSON_AddNullToObject(c,"active");
    add_nullable(c,"raw",valid[0],s.raw[0]);add_nullable(c,"millivolts",valid[0]&&s.mv[0]>=0,s.mv[0]);
    add_nullable(c,"count",ldr_ready,s.clips);add_nullable(c,"dark_reference",s.dark>=0,s.dark);add_nullable(c,"lit_reference",s.lit>=0,s.lit);
    cJSON_AddNumberToObject(c,"gpio",17);return send_json(r,o);
}
static esp_err_t calibrate(httpd_req_t *r) {
    char header[8];if(httpd_req_get_hdr_value_str(r,"X-BLDSP",header,sizeof(header))!=ESP_OK||strcmp(header,"1"))return fail(r,"403 Forbidden","Cabecalho ausente");
    if(r->content_len<1||r->content_len>64)return fail(r,"400 Bad Request","Corpo invalido");
    char body[65];int got=0;while(got<r->content_len){int n=httpd_req_recv(r,body+got,r->content_len-got);if(n<=0)return ESP_FAIL;got+=n;}body[got]=0;
    cJSON *o=cJSON_Parse(body),*v=cJSON_GetObjectItemCaseSensitive(o,"state");
    bool dark=cJSON_IsString(v)&&!strcmp(v->valuestring,"off"),lit=cJSON_IsString(v)&&!strcmp(v->valuestring,"on");cJSON_Delete(o);
    if(!dark&&!lit)return fail(r,"400 Bad Request","Escolha off ou on");
    portENTER_CRITICAL(&mux);__typeof__(readings) s=readings;portEXIT_CRITICAL(&mux);
    if(!s.valid[0]||esp_timer_get_time()-s.sampled[0]>250000)return fail(r,"503 Service Unavailable","Sem leitura recente do LDR");
    int new_dark=dark?s.raw[0]:s.dark,new_lit=lit?s.raw[0]:s.lit;
    if(new_dark>=0&&new_lit>=0&&!calibrated_ldr(new_dark,new_lit))return fail(r,"400 Bad Request","LED aceso deve reduzir a leitura em pelo menos 100 pontos; refaca as referencias");
    nvs_handle_t store;esp_err_t err=nvs_open("sensors",NVS_READWRITE,&store);
    if(err!=ESP_OK)return fail(r,"500 Internal Server Error","Falha ao salvar calibracao");
    err=nvs_set_i32(store,dark?"ldr_dark":"ldr_lit",s.raw[0]);if(err==ESP_OK)err=nvs_commit(store);nvs_close(store);
    if(err!=ESP_OK)return fail(r,"500 Internal Server Error","Falha ao salvar calibracao");
    portENTER_CRITICAL(&mux);readings.dark=new_dark;readings.lit=new_lit;readings.led_on=false;readings.last_clip=0;readings.debounce=0;portEXIT_CRITICAL(&mux);
    ESP_LOGI("sensors","LDR reference %s=%d; calibrated=%d",dark?"off":"on",s.raw[0],calibrated_ldr(new_dark,new_lit));return get(r);
}
void telemetry_register(httpd_handle_t server){
    const httpd_uri_t read={.uri="/api/telemetry",.method=HTTP_GET,.handler=get};
    const httpd_uri_t cal={.uri="/api/telemetry/ldr",.method=HTTP_POST,.handler=calibrate};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server,&read));ESP_ERROR_CHECK(httpd_register_uri_handler(server,&cal));
}
