#include "panel_control.h"
#include "battery.h"
#include "vcc_calibration.h"
#include "lighting.h"
#include "panel_ota.h"
#include "panel_protocol.h"
#include "strip_palette.h"
#include "knob_policy.h"
#include "dsp_usb.h"
#include "source_control.h"
#include "telemetry.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_log.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
typedef struct {uint32_t version;uint8_t enabled;knob_binding_t binding[5];} panel_config_t;
static panel_config_t config;
typedef struct {uint32_t version;uint16_t count;uint8_t brightness,mode,enabled,palette;} strip_config_t;
_Static_assert(sizeof(strip_config_t)==12,"strip NVS layout changed");
static strip_config_t strip={.version=3,.count=22,.brightness=30,.mode=0,.enabled=1};
static uint32_t primary_boot;
static knob_state_t knobs[5];
static QueueHandle_t inbox;static SemaphoreHandle_t action;
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static uint32_t input_sequence,input_boot;
static panel_vcc_t vcc_diagnostics;static uint64_t vcc_at;
static panel_adc_t adc_diagnostics;static uint64_t diagnostics_at;
static uint64_t received_at;static uint16_t positions[5];static uint8_t valid_mask,picked_mask;
static esp_err_t last_error;static bool initialized;
static panel_config_t defaults(void){
    panel_config_t c={.version=1,.enabled=0};
    c.binding[0]=(knob_binding_t){0x93,1,-60*256,0};
    for(int i=1;i<5;i++)c.binding[i]=(knob_binding_t){0x91,(uint8_t)(7+5*(i-1)),-12*256,12*256};
    return c;
}
static bool config_valid(const panel_config_t *c){
    if(c->version!=1||c->enabled>1)return false;
    for(int i=0;i<5;i++){
        if(!knob_binding_valid(&c->binding[i]))return false;
        for(int j=0;j<i;j++)if(c->binding[i].module&&c->binding[i].module==c->binding[j].module&&c->binding[i].selector==c->binding[j].selector)return false;
    }return true;
}
static void receive(const esp_now_recv_info_t *info,const uint8_t *data,int len){
    if(!info||memcmp(info->src_addr,panel_remote_mac,6))return;
    if(panel_ota_receive(data,len))return;
    if(len==sizeof(panel_vcc_t)){
        panel_vcc_t d;memcpy(&d,data,sizeof(d));
        if(d.magic!=PANEL_MAGIC||d.version!=PANEL_VERSION||d.type!=7||d.valid>1||d.calibrated>1||d.raw>4095||d.recovery_state>2||d.min_mv>d.mv||d.samples>200||d.missed>200)return;
        portENTER_CRITICAL(&mux);bool changed=d.boot!=vcc_diagnostics.boot||d.recovery_state!=vcc_diagnostics.recovery_state||d.recoveries!=vcc_diagnostics.recoveries;vcc_diagnostics=d;vcc_at=esp_timer_get_time()/1000;portEXIT_CRITICAL(&mux);
        battery_ingest(&d,primary_boot);
        if(changed&&d.recovery_state)ESP_LOGW("panel_vcc","C3 recovery state=%u attempts=%lu",d.recovery_state,(unsigned long)d.recoveries);
        ESP_LOGI("panel_vcc","seq=%lu raw=%u mv=%u cal=%u err=%ld read_us=%lu avg=%.2fV min=%.2fV n=%u missed=%u",(unsigned long)d.sequence,d.raw,d.mv,d.calibrated,(long)d.error,(unsigned long)d.duration_us,(d.valid&&vcc_raw_valid(d.raw))?vcc_from_mv(d.mv):NAN,d.valid?vcc_from_mv(d.min_mv):NAN,d.samples,d.missed);return;
    }
    if(len==sizeof(panel_adc_t)){
        panel_adc_t d;memcpy(&d,data,sizeof(d));
        if(d.magic!=PANEL_MAGIC||d.version!=PANEL_VERSION||d.type!=6||(d.valid_mask&~31)||(d.calibrated_mask&~31))return;
        for(int i=0;i<5;i++)if(d.raw[i]>4095)return;
        portENTER_CRITICAL(&mux);adc_diagnostics=d;diagnostics_at=esp_timer_get_time()/1000;portEXIT_CRITICAL(&mux);return;
    }
    if(len!=sizeof(panel_input_t))return;
    panel_input_t p;memcpy(&p,data,sizeof(p));
    if(p.magic!=PANEL_MAGIC||p.version!=PANEL_VERSION||p.type!=1||(p.valid_mask&~31))return;
    xQueueOverwrite(inbox,&p);
}
static void worker(void *arg){
    (void)arg;panel_input_t p={0};uint32_t boot=0,sequence=0;bool seen=false,was_ready=false;uint8_t takeover=0;uint64_t last=0,reply_at=0,logged_at=0,retry_at[5]={0};
    while(1){
        uint64_t now=esp_timer_get_time()/1000;
        if(xSemaphoreTake(action,pdMS_TO_TICKS(20))!=pdTRUE){vTaskDelay(pdMS_TO_TICKS(20));continue;}
        panel_input_t incoming;
        if(xQueueReceive(inbox,&incoming,0)==pdTRUE && (!seen||incoming.boot!=boot||(int32_t)(incoming.sequence-sequence)>0)){
            if(!seen||incoming.boot!=boot||now-last>=1500){memset(knobs,0,sizeof(knobs));memset(retry_at,0,sizeof(retry_at));takeover=31;ESP_LOGI("panel","C3 link online; physical gains pending authoritative apply");}
            p=incoming;boot=p.boot;sequence=p.sequence;seen=true;last=now;
        }
        bool online=seen&&now-last<1500;
        bool ready=online&&source_control_ready()&&config.enabled;
        if(ready&&!was_ready)takeover=31;
        was_ready=ready;
        if(!ready)memset(knobs,0,sizeof(knobs));
        uint8_t picked=0;int16_t current_gain[5]={0};bool wrote=false;
        for(int i=0;i<5;i++){
            knob_binding_t b=config.binding[i];uint16_t words[53];uint32_t rev;
            if(!b.module||!dsp_eq_snapshot(b.module,words,&rev))continue;
            current_gain[i]=(int16_t)words[b.selector];
            if(!(p.valid_mask&(1<<i))){memset(&knobs[i],0,sizeof(knobs[i]));continue;}
            if(online&&config.enabled&&source_control_ready()&&!panel_ota_busy()){
                int16_t target;
                bool active=words[0]&&(b.selector==1||(words[b.selector-4]&&words[b.selector-3]<=2));
                if(!active)memset(&knobs[i],0,sizeof(knobs[i]));
                else if(!wrote&&now>=retry_at[i]){
                    bool claim=takeover&(1u<<i);
                    bool change=claim?knob_claim(&knobs[i],&b,knob_control_position(p.position[i]),current_gain[i],&target):knob_update(&knobs[i],&b,knob_control_position(p.position[i]),current_gain[i],&target);
                    if(!change){if(claim)takeover&=~(1u<<i);}
                    else {
                    esp_err_t err=dsp_set_gain(b.module,b.selector,target);
                    portENTER_CRITICAL(&mux);last_error=err;portEXIT_CRITICAL(&mux);
                    if(err==ESP_OK){takeover&=~(1u<<i);knobs[i].observed=target;knobs[i].last_sent=target;current_gain[i]=target;ESP_LOGI("panel","Pot%d -> effect=%d selector=%u gain=%.2f dB",i+1,b.module==0x91?2:4,b.selector,target/256.);}
                    else {retry_at[i]=now+1000;ESP_LOGW("panel","Pot%d pending: %s",i+1,esp_err_to_name(err));}
                    // One USB transaction per pass; latest positions coalesce.
                    wrote=true;
                    }
                }
            }
            if(knobs[i].picked)picked|=1<<i;
        }
        portENTER_CRITICAL(&mux);received_at=last;input_sequence=p.sequence;input_boot=p.boot;valid_mask=online?p.valid_mask:0;picked_mask=picked;for(int i=0;i<5;i++){positions[i]=p.position[i];}portEXIT_CRITICAL(&mux);
        if(online&&now-logged_at>=2000){
            ESP_LOGI("panel_input","seq=%lu adc_pos=%u,%u,%u,%u,%u valid=%02x picked=%02x enabled=%u",(unsigned long)p.sequence,p.position[0],p.position[1],p.position[2],p.position[3],p.position[4],p.valid_mask,picked,config.enabled);
            portENTER_CRITICAL(&mux);panel_adc_t d=adc_diagnostics;uint64_t at=diagnostics_at;portEXIT_CRITICAL(&mux);
            if(at&&now-at<2000&&d.boot==p.boot)ESP_LOGI("panel_adc","RAW=%u,%u,%u,%u,%u mV=%u,%u,%u,%u,%u cal=%02x",d.raw[0],d.raw[1],d.raw[2],d.raw[3],d.raw[4],d.mv[0],d.mv[1],d.mv[2],d.mv[3],d.mv[4],d.calibrated_mask);
            logged_at=now;
        }
        if(online&&now-reply_at>=250){
            telemetry_clip_t clip;telemetry_clip_state(&clip);
            panel_reply_t r={.magic=PANEL_MAGIC,.version=PANEL_VERSION,.type=2,.sequence=sequence,.enabled=config.enabled,.picked_mask=picked,.valid_mask=p.valid_mask,.clip=clip.sustained};
            r.primary_boot=primary_boot;r.uptime_ms=(uint32_t)(esp_timer_get_time()/1000);r.clip_count=clip.count;r.clip_low_ms=clip.low_ms;r.clip=clip.lit;r.sensor_ready=clip.ready;
            r.led_count=strip.count;r.led_brightness=lighting_brightness(strip.brightness);r.led_mode=strip.mode;r.led_enabled=strip.enabled;r.reserved[0]=strip.palette;r.battery_level=battery_alert();
            portENTER_CRITICAL(&mux);if(vcc_diagnostics.boot==p.boot)r.reserved[1]=vcc_diagnostics.recovery_state;portEXIT_CRITICAL(&mux);
            for(int i=0;i<5;i++)r.gain_q8[i]=current_gain[i];
            esp_now_send(panel_remote_mac,(uint8_t*)&r,sizeof(r));reply_at=now;
        }
        xSemaphoreGive(action);vTaskDelay(pdMS_TO_TICKS(20));
    }
}
void panel_start(void){
    lighting_start();battery_start();config=defaults();primary_boot=esp_random();nvs_handle_t nvs;
    if(nvs_open("panel",NVS_READONLY,&nvs)==ESP_OK){panel_config_t saved;size_t size=sizeof(saved);if(nvs_get_blob(nvs,"config_v1",&saved,&size)==ESP_OK&&size==sizeof(saved)&&config_valid(&saved))config=saved;nvs_close(nvs);}
    if(nvs_open("panel",NVS_READONLY,&nvs)==ESP_OK){strip_config_t saved;size_t size=sizeof(saved);if(nvs_get_blob(nvs,"strip_v1",&saved,&size)==ESP_OK&&size==sizeof(saved)&&(saved.version>=1&&saved.version<=3)&&saved.count>=1&&saved.count<=150&&saved.brightness<=100&&saved.mode<=2&&saved.enabled<=1&&(saved.version<3||saved.palette<STRIP_PALETTE_COUNT)){strip=saved;}nvs_close(nvs);}
    if(strip.version<3){
        if(strip.version==1)strip.count=22;
        strip.version=3;strip.palette=0;
        if(nvs_open("panel",NVS_READWRITE,&nvs)==ESP_OK){
            esp_err_t e=nvs_set_blob(nvs,"strip_v1",&strip,sizeof(strip));if(e==ESP_OK)e=nvs_commit(nvs);nvs_close(nvs);
            if(e!=ESP_OK)ESP_LOGW("panel","Could not persist 22-LED migration: %s",esp_err_to_name(e));
        }
    }
    inbox=xQueueCreate(1,sizeof(panel_input_t));action=xSemaphoreCreateMutex();configASSERT(inbox&&action);
    panel_ota_init();
    ESP_ERROR_CHECK(esp_now_init());ESP_ERROR_CHECK(esp_now_set_pmk(panel_pmk));
    esp_now_peer_info_t peer={.channel=PANEL_CHANNEL,.ifidx=WIFI_IF_AP,.encrypt=true};memcpy(peer.peer_addr,panel_remote_mac,6);memcpy(peer.lmk,panel_lmk,16);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));ESP_ERROR_CHECK(esp_now_register_recv_cb(receive));
    uint8_t mac[6];ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_AP,mac));
    ESP_LOGI("panel","Primary AP MAC %02x:%02x:%02x:%02x:%02x:%02x; ESP-NOW encrypted channel6 enabled=%d",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5],config.enabled);
    if(memcmp(mac,panel_primary_mac,6))ESP_LOGE("panel","Primary MAC differs from paired C3 firmware");
    initialized=xTaskCreate(worker,"panel",4096,NULL,2,NULL)==pdPASS;
}
void panel_add_status(cJSON *root){
    uint64_t now=esp_timer_get_time()/1000;panel_config_t c;strip_config_t led;uint16_t pos[5];uint8_t mask,picked;uint64_t last;uint32_t sequence,boot;panel_adc_t diag;uint64_t diag_at;esp_err_t error;
    portENTER_CRITICAL(&mux);c=config;led=strip;memcpy(pos,positions,sizeof(pos));mask=valid_mask;picked=picked_mask;last=received_at;sequence=input_sequence;boot=input_boot;diag=adc_diagnostics;diag_at=diagnostics_at;error=last_error;portEXIT_CRITICAL(&mux);
    cJSON *led_json=cJSON_AddObjectToObject(root,"strip");cJSON_AddNumberToObject(led_json,"count",led.count);cJSON_AddNumberToObject(led_json,"brightness",led.brightness);cJSON_AddNumberToObject(led_json,"mode",led.mode);cJSON_AddNumberToObject(led_json,"palette",led.palette);cJSON_AddBoolToObject(led_json,"enabled",led.enabled);
    lighting_status(root,led.brightness);battery_status(root);
    cJSON *p=cJSON_AddObjectToObject(root,"panel");cJSON_AddBoolToObject(p,"available",initialized);cJSON_AddBoolToObject(p,"online",last&&now-last<1500);cJSON_AddBoolToObject(p,"enabled",c.enabled);
    cJSON_AddNumberToObject(p,"sequence",sequence);cJSON_AddNumberToObject(p,"boot",boot);
    if(last)cJSON_AddNumberToObject(p,"age_ms",now-last);else cJSON_AddNullToObject(p,"age_ms");
    cJSON_AddStringToObject(p,"error",error==ESP_OK?"":esp_err_to_name(error));
    portENTER_CRITICAL(&mux);panel_vcc_t vd=vcc_diagnostics;uint64_t va=vcc_at;portEXIT_CRITICAL(&mux);
    bool vf=last&&now-last<1500&&va&&now-va<2500&&vd.boot==boot;
    cJSON *v=cJSON_AddObjectToObject(p,"vcc");cJSON_AddNumberToObject(v,"recovery_state",vf?vd.recovery_state:0);cJSON_AddNumberToObject(v,"recoveries",vd.recoveries);cJSON_AddBoolToObject(v,"fresh",vf);cJSON_AddBoolToObject(v,"valid",vf&&vd.valid&&vcc_raw_valid(vd.raw));
    cJSON_AddBoolToObject(v,"calibrated",vd.calibrated);cJSON_AddNumberToObject(v,"raw",vd.raw);cJSON_AddNumberToObject(v,"mv",vd.mv);
    if(vf&&vd.valid&&vcc_raw_valid(vd.raw))cJSON_AddNumberToObject(v,"volts",vcc_from_mv(vd.mv));else cJSON_AddNullToObject(v,"volts");cJSON_AddNumberToObject(v,"duration_us",vd.duration_us);
    cJSON_AddStringToObject(v,"error",vf?(!vcc_raw_valid(vd.raw)?"ADC2 no limite da escala; leitura inválida":esp_err_to_name(vd.error)):"Sem amostra recente");
    cJSON *a=cJSON_AddArrayToObject(p,"bindings");
    for(int i=0;i<5;i++){
        const knob_binding_t *binding=&c.binding[i];cJSON *b=cJSON_CreateObject();
        cJSON_AddNumberToObject(b,"module",binding->module);cJSON_AddNumberToObject(b,"selector",binding->selector);
        cJSON_AddNumberToObject(b,"min_db",binding->min_q8/256.);cJSON_AddNumberToObject(b,"max_db",binding->max_q8/256.);
        cJSON_AddNumberToObject(b,"position",knob_control_position(pos[i]));
        cJSON_AddNumberToObject(b,"adc_position",pos[i]);cJSON_AddBoolToObject(b,"inverted",true);
        bool diag_valid=last&&now-last<1500&&diag_at&&now-diag_at<1500&&diag.boot==boot&&(diag.valid_mask&(1<<i));
        if(diag_valid){cJSON_AddNumberToObject(b,"adc_raw",diag.raw[i]);cJSON_AddNumberToObject(b,"adc_mv",diag.mv[i]);cJSON_AddBoolToObject(b,"adc_calibrated",diag.calibrated_mask&(1<<i));}
        else{cJSON_AddNullToObject(b,"adc_raw");cJSON_AddNullToObject(b,"adc_mv");}
        if(binding->module)cJSON_AddNumberToObject(b,"target_gain_db",knob_gain(binding,knob_control_position(pos[i]))/256.);else cJSON_AddNullToObject(b,"target_gain_db");
        uint16_t current[53];uint32_t rev;
        bool available=binding->module&&dsp_eq_snapshot(binding->module,current,&rev);
        if(available)cJSON_AddNumberToObject(b,"gain_db",(int16_t)current[binding->selector]/256.);else cJSON_AddNullToObject(b,"gain_db");
        bool active=available&&current[0]&&(binding->selector==1||(current[binding->selector-4]&&current[binding->selector-3]<=2));
        cJSON_AddBoolToObject(b,"active",active);cJSON_AddBoolToObject(b,"valid",mask&(1<<i));cJSON_AddBoolToObject(b,"picked",picked&(1<<i));cJSON_AddItemToArray(a,b);
    }
    uint16_t words[53];uint32_t revision=0;
    const uint8_t modules[]={0x93,0x91};const char *names[]={"sub","stereo"};
    for(int j=0;j<2;j++)if(dsp_eq_snapshot(modules[j],words,&revision)){cJSON *w=cJSON_AddArrayToObject(p,names[j]);for(int i=0;i<53;i++)cJSON_AddItemToArray(w,cJSON_CreateNumber(words[i]));}
    cJSON_AddNumberToObject(p,"revision",revision);
}
static esp_err_t respond(httpd_req_t *r,const char *status,const char *message){
    httpd_resp_set_status(r,status);httpd_resp_set_type(r,"application/json");httpd_resp_set_hdr(r,"Cache-Control","no-store");
    cJSON *o=cJSON_CreateObject();if(message)cJSON_AddStringToObject(o,"error",message);else panel_add_status(o);char *s=cJSON_PrintUnformatted(o);esp_err_t err=httpd_resp_sendstr(r,s);free(s);cJSON_Delete(o);return err;
}
static bool integer(cJSON *o,const char *key,int *value){cJSON *n=cJSON_GetObjectItemCaseSensitive(o,key);if(!cJSON_IsNumber(n)||!isfinite(n->valuedouble)||n->valuedouble < -20000||n->valuedouble>20000||floor(n->valuedouble)!=n->valuedouble)return false;*value=n->valueint;return true;}
static esp_err_t route(httpd_req_t *r){
    if(r->method==HTTP_GET)return respond(r,"200 OK",NULL);
    char header[8],body[1537];if(httpd_req_get_hdr_value_str(r,"X-BLDSP",header,sizeof(header))!=ESP_OK||strcmp(header,"1"))return respond(r,"403 Forbidden","Cabecalho ausente");
    if(r->content_len<1||r->content_len>1536)return respond(r,"400 Bad Request","Corpo invalido");
    int got=0;while(got<r->content_len){int n=httpd_req_recv(r,body+got,r->content_len-got);if(n<=0)return ESP_FAIL;got+=n;}body[got]=0;
    cJSON *o=cJSON_Parse(body),*enabled=cJSON_GetObjectItemCaseSensitive(o,"enabled"),*a=cJSON_GetObjectItemCaseSensitive(o,"bindings");panel_config_t c={.version=1,.enabled=cJSON_IsTrue(enabled)};
    bool valid=cJSON_IsBool(enabled)&&cJSON_IsArray(a)&&cJSON_GetArraySize(a)==5;
    for(int i=0;valid&&i<5;i++){
        cJSON *b=cJSON_GetArrayItem(a,i),*lo=cJSON_GetObjectItemCaseSensitive(b,"min_db"),*hi=cJSON_GetObjectItemCaseSensitive(b,"max_db");int module=0,selector=0;
        valid=integer(b,"module",&module)&&integer(b,"selector",&selector)&&module>=0&&module<=255&&selector>=0&&selector<=52&&cJSON_IsNumber(lo)&&cJSON_IsNumber(hi)&&isfinite(lo->valuedouble)&&isfinite(hi->valuedouble)&&lo->valuedouble>=-60&&hi->valuedouble<=12&&lo->valuedouble<hi->valuedouble;
        if(valid)c.binding[i]=(knob_binding_t){module,selector,(int16_t)lround(lo->valuedouble*256),(int16_t)lround(hi->valuedouble*256)};
    }
    cJSON_Delete(o);if(!valid||!config_valid(&c))return respond(r,"400 Bad Request","Ganhos invalidos ou dois potenciometros no mesmo controle");
    if(xSemaphoreTake(action,pdMS_TO_TICKS(2000))!=pdTRUE)return respond(r,"503 Service Unavailable","Aguarde o ajuste USB");
    nvs_handle_t store;esp_err_t err=nvs_open("panel",NVS_READWRITE,&store);if(err==ESP_OK){err=nvs_set_blob(store,"config_v1",&c,sizeof(c));if(err==ESP_OK)err=nvs_commit(store);nvs_close(store);}
    if(err==ESP_OK){portENTER_CRITICAL(&mux);config=c;memset(knobs,0,sizeof(knobs));picked_mask=0;last_error=ESP_OK;portEXIT_CRITICAL(&mux);ESP_LOGI("panel","Panel mapping saved NVS; enabled=%d; pickup reset",c.enabled);}
    xSemaphoreGive(action);return respond(r,err==ESP_OK?"200 OK":"500 Internal Server Error",err==ESP_OK?NULL:"Falha ao salvar painel");
}
static esp_err_t strip_route(httpd_req_t *r){
    if(r->method==HTTP_GET)return respond(r,"200 OK",NULL);
    char header[8],body[129];if(httpd_req_get_hdr_value_str(r,"X-BLDSP",header,sizeof(header))!=ESP_OK||strcmp(header,"1"))return respond(r,"403 Forbidden","Cabecalho ausente");
    if(r->content_len<1||r->content_len>128)return respond(r,"400 Bad Request","Corpo invalido");
    int got=0;while(got<r->content_len){int n=httpd_req_recv(r,body+got,r->content_len-got);if(n<=0)return ESP_FAIL;got+=n;}body[got]=0;
    cJSON *o=cJSON_Parse(body),*enabled=cJSON_GetObjectItemCaseSensitive(o,"enabled");int count=0,brightness=0,mode=0,palette=0;
    bool valid=cJSON_IsBool(enabled)&&integer(o,"count",&count)&&integer(o,"brightness",&brightness)&&integer(o,"mode",&mode)&&count>=1&&count<=150&&brightness>=0&&brightness<=100&&mode>=0&&mode<=2;
    bool has_palette=cJSON_GetObjectItemCaseSensitive(o,"palette")!=NULL;
    if(has_palette)valid=valid&&integer(o,"palette",&palette)&&palette>=0&&palette<STRIP_PALETTE_COUNT;
    strip_config_t c={.version=3,.count=count,.brightness=brightness,.mode=mode,.enabled=cJSON_IsTrue(enabled),.palette=palette};cJSON_Delete(o);
    if(!valid)return respond(r,"400 Bad Request","LEDs: 1 a 150 segmentos, brilho 0 a 100%, modo 0 a 2");
    if(xSemaphoreTake(action,pdMS_TO_TICKS(2000))!=pdTRUE)return respond(r,"503 Service Unavailable","Aguarde o ajuste USB");
    if(!has_palette)c.palette=strip.palette;
    nvs_handle_t store;esp_err_t err=nvs_open("panel",NVS_READWRITE,&store);if(err==ESP_OK){err=nvs_set_blob(store,"strip_v1",&c,sizeof(c));if(err==ESP_OK)err=nvs_commit(store);nvs_close(store);}
    if(err==ESP_OK){portENTER_CRITICAL(&mux);strip=c;portEXIT_CRITICAL(&mux);ESP_LOGI("panel","WS2812 saved: count=%u brightness=%u mode=%u enabled=%u",c.count,c.brightness,c.mode,c.enabled);}
    xSemaphoreGive(action);return respond(r,err==ESP_OK?"200 OK":"500 Internal Server Error",err==ESP_OK?NULL:"Falha ao salvar fita");
}
void panel_register(httpd_handle_t server){
    panel_ota_register(server);
    lighting_register(server);battery_register(server);
    const httpd_uri_t get={.uri="/api/panel",.method=HTTP_GET,.handler=route},post={.uri="/api/panel",.method=HTTP_POST,.handler=route};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server,&get));ESP_ERROR_CHECK(httpd_register_uri_handler(server,&post));
    const httpd_uri_t led_get={.uri="/api/strip",.method=HTTP_GET,.handler=strip_route},led_post={.uri="/api/strip",.method=HTTP_POST,.handler=strip_route};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server,&led_get));ESP_ERROR_CHECK(httpd_register_uri_handler(server,&led_post));
}
