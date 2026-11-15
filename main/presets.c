#include "presets.h"
#include "protocol.h"
#include "cJSON.h"
#include "nvs.h"
#include "preset1_acp.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Each versioned blob holds both independent EQs. No USB writes from this API.
typedef struct {uint32_t version;uint16_t sub[53];uint16_t stereo[53];} preset_t;
static esp_err_t seed_acp_preset(void) {
    nvs_handle_t store;
    esp_err_t err=nvs_open("eq_presets",NVS_READWRITE,&store);
    if(err!=ESP_OK)return err;
    uint8_t seeded=0;err=nvs_get_u8(store,"acp_seed_v1",&seeded);
    if(err==ESP_ERR_NVS_NOT_FOUND || (err==ESP_OK && seeded!=1)) {
        preset_t p={.version=1};
        memcpy(p.sub,acp_preset_sub,sizeof(p.sub));
        memcpy(p.stereo,acp_preset_stereo,sizeof(p.stereo));
        if(!dsp_valid_eq_words(p.sub)||!dsp_valid_eq_words(p.stereo))err=ESP_ERR_INVALID_STATE;
        else {
            err=nvs_set_blob(store,"slot1",&p,sizeof(p));
            if(err==ESP_OK)err=nvs_set_u8(store,"acp_seed_v1",1);
            if(err==ESP_OK)err=nvs_commit(store);
        }
    }
    nvs_close(store);return err;
}
static void flat(uint16_t *w) {
    memset(w,0,106);w[0]=1;
    for(int i=0;i<10;i++){w[5+5*i]=1000;w[6+5*i]=724;}
}
esp_err_t presets_init(void){return seed_acp_preset();}
esp_err_t presets_read(int slot,uint16_t *sub,uint16_t *stereo) {
    if(slot<1||slot>4)return ESP_ERR_INVALID_ARG;
    nvs_handle_t store;esp_err_t err=nvs_open("eq_presets",NVS_READONLY,&store);
    if(err!=ESP_OK)return err;
    char key[8];snprintf(key,sizeof(key),"slot%d",slot);
    preset_t p={.version=1};flat(p.sub);flat(p.stereo);
    size_t size=sizeof(p);err=nvs_get_blob(store,key,&p,&size);nvs_close(store);
    if(err==ESP_ERR_NVS_NOT_FOUND){err=ESP_OK;size=sizeof(p);}
    if(err==ESP_OK&&(size!=sizeof(p)||p.version!=1||!dsp_valid_eq_words(p.sub)||!dsp_valid_eq_words(p.stereo)))err=ESP_ERR_INVALID_STATE;
    if(err==ESP_OK){memcpy(sub,p.sub,sizeof(p.sub));memcpy(stereo,p.stereo,sizeof(p.stereo));}
    return err;
}
static esp_err_t fail(httpd_req_t *r,const char *code,const char *message) {
    httpd_resp_set_status(r,code);httpd_resp_set_type(r,"application/json");
    cJSON *o=cJSON_CreateObject();cJSON_AddStringToObject(o,"error",message);
    char *s=cJSON_PrintUnformatted(o);httpd_resp_sendstr(r,s);free(s);cJSON_Delete(o);return ESP_OK;
}
static bool parse_words(cJSON *o,const char *name,uint16_t *w) {
    cJSON *a=cJSON_GetObjectItemCaseSensitive(o,name);
    if(!cJSON_IsArray(a)||cJSON_GetArraySize(a)!=53)return false;
    for(int i=0;i<53;i++) {
        cJSON *v=cJSON_GetArrayItem(a,i);
        if(!cJSON_IsNumber(v)||!isfinite(v->valuedouble)||v->valuedouble<0||v->valuedouble>65535||floor(v->valuedouble)!=v->valuedouble)return false;
        w[i]=(uint16_t)v->valuedouble;
    }
    return dsp_valid_eq_words(w);
}
static void add_words(cJSON *o,const char *name,const uint16_t *w) {
    cJSON *a=cJSON_AddArrayToObject(o,name);
    for(int i=0;i<53;i++)cJSON_AddItemToArray(a,cJSON_CreateNumber(w[i]));
}
static esp_err_t handle(httpd_req_t *r) {
    char query[48],slot_text[8];
    if(httpd_req_get_url_query_str(r,query,sizeof(query))!=ESP_OK ||
       httpd_query_key_value(query,"slot",slot_text,sizeof(slot_text))!=ESP_OK ||
       strlen(slot_text)!=1 || slot_text[0]<'1'||slot_text[0]>'4')
        return fail(r,"400 Bad Request","Preset inválido (1 a 4)");
    char key[8];snprintf(key,sizeof(key),"slot%c",slot_text[0]);
    preset_t p={.version=1};flat(p.sub);flat(p.stereo);
    bool saved=false;
    nvs_handle_t store;
    esp_err_t err=nvs_open("eq_presets",NVS_READWRITE,&store);
    if(err!=ESP_OK)return fail(r,"500 Internal Server Error",esp_err_to_name(err));
    if(r->method==HTTP_POST) {
        char h[8];
        if(httpd_req_get_hdr_value_str(r,"X-BLDSP",h,sizeof(h))!=ESP_OK||strcmp(h,"1")) {
            nvs_close(store);return fail(r,"403 Forbidden","Cabeçalho ausente");
        }
        if(r->content_len<1||r->content_len>2048){nvs_close(store);return fail(r,"400 Bad Request","Corpo inválido");}
        char body[2049];int got=0;
        while(got<r->content_len){int n=httpd_req_recv(r,body+got,r->content_len-got);if(n<=0){nvs_close(store);return ESP_FAIL;}got+=n;}
        body[got]=0;cJSON *o=cJSON_Parse(body),*action=cJSON_GetObjectItemCaseSensitive(o,"action");
        if(cJSON_IsString(action)&&!strcmp(action->valuestring,"save")) {
            if(!parse_words(o,"sub",p.sub)||!parse_words(o,"stereo",p.stereo)) {
                cJSON_Delete(o);nvs_close(store);return fail(r,"400 Bad Request","Valores de preset inválidos");
            }
            err=nvs_set_blob(store,key,&p,sizeof(p));saved=true;
        } else if(cJSON_IsString(action)&&!strcmp(action->valuestring,"reset")) {
            err=nvs_erase_key(store,key);if(err==ESP_ERR_NVS_NOT_FOUND)err=ESP_OK;
        } else {cJSON_Delete(o);nvs_close(store);return fail(r,"400 Bad Request","Ação inválida");}
        cJSON_Delete(o);if(err==ESP_OK)err=nvs_commit(store);
    } else {
        size_t size=sizeof(p);err=nvs_get_blob(store,key,&p,&size);
        if(err==ESP_ERR_NVS_NOT_FOUND)err=ESP_OK;
        else if(err==ESP_OK) {
            if(size!=sizeof(p)||p.version!=1||!dsp_valid_eq_words(p.sub)||!dsp_valid_eq_words(p.stereo))err=ESP_ERR_INVALID_STATE;
            else saved=true;
        }
    }
    nvs_close(store);
    if(err!=ESP_OK)return fail(r,"500 Internal Server Error",esp_err_to_name(err));
    cJSON *o=cJSON_CreateObject();cJSON_AddBoolToObject(o,"saved",saved);
    add_words(o,"sub",p.sub);add_words(o,"stereo",p.stereo);
    char *s=cJSON_PrintUnformatted(o);httpd_resp_set_type(r,"application/json");
    httpd_resp_set_hdr(r,"Cache-Control","no-store");httpd_resp_sendstr(r,s);
    free(s);cJSON_Delete(o);return ESP_OK;
}
void presets_register(httpd_handle_t server) {
    const httpd_uri_t get={.uri="/api/preset",.method=HTTP_GET,.handler=handle};
    const httpd_uri_t post={.uri="/api/preset",.method=HTTP_POST,.handler=handle};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server,&get));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server,&post));
}
