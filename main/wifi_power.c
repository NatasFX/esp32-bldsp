#include "wifi_power.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
static bool supported(int value){
    const int levels[]={8,20,28,34,44,52,56,60,66,72,80};
    for(unsigned i=0;i<sizeof(levels)/sizeof(levels[0]);i++)if(value==levels[i])return true;
    return false;
}
void wifi_power_start(void){
    uint8_t value=8;nvs_handle_t nvs;
    if(nvs_open("radio",NVS_READONLY,&nvs)==ESP_OK){uint8_t saved;if(nvs_get_u8(nvs,"tx_qdbm",&saved)==ESP_OK&&supported(saved))value=saved;nvs_close(nvs);}
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power((int8_t)value));
    int8_t actual=0;ESP_ERROR_CHECK(esp_wifi_get_max_tx_power(&actual));
    ESP_LOGI("wifi_power","AP configured TX limit %.2f dBm (driver=%d quarter-dBm); NVS/default",actual/4.,actual);
}
void wifi_power_add_status(cJSON *root){
    int8_t power=0;esp_err_t err=esp_wifi_get_max_tx_power(&power);
    cJSON *o=cJSON_AddObjectToObject(root,"wifi");cJSON_AddBoolToObject(o,"available",err==ESP_OK);
    if(err==ESP_OK){cJSON_AddNumberToObject(o,"power_qdbm",power);cJSON_AddNumberToObject(o,"power_dbm",power/4.);}
    cJSON_AddStringToObject(o,"scope","primary_ap_and_espnow");
}
static esp_err_t reply(httpd_req_t *r,const char *status,const char *message){
    cJSON *o=cJSON_CreateObject();if(message)cJSON_AddStringToObject(o,"error",message);else wifi_power_add_status(o);
    char *s=cJSON_PrintUnformatted(o);httpd_resp_set_status(r,status);httpd_resp_set_type(r,"application/json");httpd_resp_set_hdr(r,"Cache-Control","no-store");esp_err_t err=httpd_resp_sendstr(r,s);free(s);cJSON_Delete(o);return err;
}
static esp_err_t route(httpd_req_t *r){
    if(r->method==HTTP_GET)return reply(r,"200 OK",NULL);
    char header[8],body[65];
    if(httpd_req_get_hdr_value_str(r,"X-BLDSP",header,sizeof(header))!=ESP_OK||strcmp(header,"1"))return reply(r,"403 Forbidden","Cabecalho ausente");
    if(r->content_len<1||r->content_len>64)return reply(r,"400 Bad Request","Corpo invalido");
    int got=0;while(got<r->content_len){int n=httpd_req_recv(r,body+got,r->content_len-got);if(n<=0)return ESP_FAIL;got+=n;}body[got]=0;
    cJSON *o=cJSON_Parse(body),*v=cJSON_GetObjectItemCaseSensitive(o,"power_qdbm");
    bool valid=cJSON_IsNumber(v)&&isfinite(v->valuedouble)&&v->valuedouble>=8&&v->valuedouble<=80&&floor(v->valuedouble)==v->valuedouble&&supported(v->valueint);
    int value=valid?v->valueint:8;cJSON_Delete(o);if(!valid)return reply(r,"400 Bad Request","Selecione uma potencia suportada");
    int8_t previous=8,actual=8;esp_err_t err=esp_wifi_get_max_tx_power(&previous);
    if(err==ESP_OK)err=esp_wifi_set_max_tx_power((int8_t)value);
    if(err==ESP_OK)err=esp_wifi_get_max_tx_power(&actual);
    if(err!=ESP_OK){esp_wifi_set_max_tx_power(previous);return reply(r,"500 Internal Server Error","Falha ao aplicar potencia");}
    nvs_handle_t store;err=nvs_open("radio",NVS_READWRITE,&store);
    if(err==ESP_OK){err=nvs_set_u8(store,"tx_qdbm",(uint8_t)actual);if(err==ESP_OK)err=nvs_commit(store);nvs_close(store);}
    if(err!=ESP_OK){esp_wifi_set_max_tx_power(previous);return reply(r,"500 Internal Server Error","Falha ao salvar; potencia anterior restaurada");}
    ESP_LOGI("wifi_power","User TX limit requested=%.2f confirmed=%.2f dBm; saved NVS",value/4.,actual/4.);
    return reply(r,"200 OK",NULL);
}
void wifi_power_register(httpd_handle_t server){
    const httpd_uri_t get={.uri="/api/wifi",.method=HTTP_GET,.handler=route},post={.uri="/api/wifi",.method=HTTP_POST,.handler=route};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server,&get));ESP_ERROR_CHECK(httpd_register_uri_handler(server,&post));
}
