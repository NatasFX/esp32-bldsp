#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "dsp_usb.h"
#include "esp_timer.h"
#include "presets.h"
#include "esp_log.h"
#include "source_control.h"

extern const unsigned char html_start[] asm("_binary_index_html_start");
extern const unsigned char html_end[] asm("_binary_index_html_end");
extern const unsigned char js_start[] asm("_binary_eq_response_js_start");
extern const unsigned char js_end[] asm("_binary_eq_response_js_end");
static esp_err_t script(httpd_req_t *r) {
    httpd_resp_set_type(r,"application/javascript; charset=utf-8");
    httpd_resp_set_hdr(r,"Cache-Control","no-store");
    return httpd_resp_send(r,(const char*)js_start,js_end-js_start-1);
}
static uint8_t output_module(httpd_req_t *r) {
    char query[64],output[16];
    if(httpd_req_get_url_query_len(r)==0)return 0x93;
    if(httpd_req_get_url_query_str(r,query,sizeof(query))!=ESP_OK ||
       httpd_query_key_value(query,"output",output,sizeof(output))!=ESP_OK)return 0;
    if(!strcmp(output,"sub"))return 0x93;
    if(!strcmp(output,"stereo"))return 0x91;
    return 0;
}
static esp_err_t error(httpd_req_t *r,const char *status,const char *message) {
    httpd_resp_set_status(r,status);httpd_resp_set_type(r,"application/json");
    cJSON *o=cJSON_CreateObject();cJSON_AddStringToObject(o,"error",message);
    char *s=cJSON_PrintUnformatted(o);httpd_resp_sendstr(r,s);free(s);cJSON_Delete(o);return ESP_OK;
}
static esp_err_t usb_error(httpd_req_t *r,esp_err_t err) {
    ESP_LOGW("http_eq","USB failed uri=%s error=%s",r->uri,esp_err_to_name(err));
    return error(r,"503 Service Unavailable",esp_err_to_name(err));
}
static esp_err_t root(httpd_req_t *r) {
    ESP_LOGI("http_eq","UI requested");
    httpd_resp_set_type(r,"text/html; charset=utf-8");
    return httpd_resp_send(r,(const char*)html_start,html_end-html_start-1);
}
static esp_err_t reply_eq(httpd_req_t *r,const uint16_t *w,bool matches) {
    cJSON *o=cJSON_CreateObject(),*a=cJSON_AddArrayToObject(o,"words");
    for(int i=0;i<53;i++)cJSON_AddItemToArray(a,cJSON_CreateNumber(w[i]));
    cJSON_AddBoolToObject(o,"matches",matches);
    char *s=cJSON_PrintUnformatted(o);httpd_resp_set_type(r,"application/json");
    httpd_resp_set_hdr(r,"Cache-Control","no-store");httpd_resp_sendstr(r,s);
    free(s);cJSON_Delete(o);return ESP_OK;
}
static esp_err_t read_eq(httpd_req_t *r) {
    uint8_t module=output_module(r);
    if(!module)return error(r,"400 Bad Request","Saída inválida");
    uint16_t w[53];esp_err_t err=dsp_read_eq(module,w);
    return err==ESP_OK?reply_eq(r,w,true):usb_error(r,err);
}
static bool valid_words(const uint16_t *w) {
    return dsp_valid_eq_words(w);
}
static esp_err_t apply_eq(httpd_req_t *r) {
    int64_t started=esp_timer_get_time();
    ESP_LOGI("http_eq","POST %s bytes=%d",r->uri,(int)r->content_len);
    uint8_t module=output_module(r);
    if(!module)return error(r,"400 Bad Request","Saída inválida");
    if(r->content_len<1 || r->content_len>1024)return error(r,"400 Bad Request","Corpo inválido");
    // Browser custom header prevents cross-origin HTML form submissions.
    char header[8];
    if(httpd_req_get_hdr_value_str(r,"X-BLDSP",header,sizeof(header))!=ESP_OK || strcmp(header,"1"))
        return error(r,"403 Forbidden","Cabeçalho ausente");
    char body[1025];int got=0;
    while(got<r->content_len){int n=httpd_req_recv(r,body+got,r->content_len-got);if(n<=0)return ESP_FAIL;got+=n;}
    body[got]=0;cJSON *o=cJSON_Parse(body),*a=cJSON_GetObjectItemCaseSensitive(o,"words");
    uint16_t w[53],actual[53];bool valid=cJSON_IsArray(a)&&cJSON_GetArraySize(a)==53;
    for(int i=0;valid && i<53;i++) {
        cJSON *v=cJSON_GetArrayItem(a,i);
        valid=cJSON_IsNumber(v)&&isfinite(v->valuedouble)&&v->valuedouble>=0&&v->valuedouble<=65535&&floor(v->valuedouble)==v->valuedouble;
        if(valid)w[i]=(uint16_t)v->valuedouble;
    }
    cJSON_Delete(o);
    if(!valid || !valid_words(w)){ESP_LOGW("http_eq","Invalid EQ payload module=%02x",module);return error(r,"400 Bad Request","Valores de EQ inválidos");}
    ESP_LOGI("http_eq","Requested module=%02x enabled=%u pregain=%.3f dB",module,w[0],(double)(int16_t)w[1]/256);
    // Preserve the third word from a fresh read: its installed-firmware semantics
    // remain uncertain. Do not let a browser alter it.
    esp_err_t err=dsp_read_eq(module,actual);if(err!=ESP_OK)return usb_error(r,err);
    w[2]=actual[2];
    err=dsp_apply_eq(module,w,actual);if(err!=ESP_OK)return usb_error(r,err);
    bool matches=true;
    for(int i=0;i<53;i++)if(i!=2 && actual[i]!=w[i])matches=false;
    ESP_LOGI("http_eq","Confirmed module=%02x matches=%d pregain=%.3f dB duration=%ld ms",module,matches,(double)(int16_t)actual[1]/256,(long)((esp_timer_get_time()-started)/1000));
    return reply_eq(r,actual,matches);
}
static esp_err_t status(httpd_req_t *r) {
    httpd_resp_set_type(r,"application/json");
    httpd_resp_set_hdr(r,"Cache-Control","no-store");
    return httpd_resp_sendstr(r,"{\"demo\":false}");
}
void app_main(void) {
    source_control_init();
    esp_err_t err=nvs_flash_init();
    if(err==ESP_ERR_NVS_NO_FREE_PAGES || err==ESP_ERR_NVS_NEW_VERSION_FOUND){ESP_ERROR_CHECK(nvs_flash_erase());err=nvs_flash_init();}
    ESP_ERROR_CHECK(err);ESP_ERROR_CHECK(presets_init());ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();ESP_ERROR_CHECK(esp_wifi_init(&init));
    wifi_config_t wifi={.ap={.ssid="BLDSP-Control",.password="equaliza123",.channel=6,.max_connection=2,.authmode=WIFI_AUTH_WPA2_PSK}};
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP,&wifi));
    ESP_ERROR_CHECK(esp_wifi_start());
    dsp_usb_start();
    httpd_config_t cfg=HTTPD_DEFAULT_CONFIG();cfg.stack_size=8192;cfg.recv_wait_timeout=5;cfg.send_wait_timeout=5;
    httpd_handle_t server;ESP_ERROR_CHECK(httpd_start(&server,&cfg));
    const httpd_uri_t routes[]={
        {.uri="/",.method=HTTP_GET,.handler=root},
        {.uri="/eq-response.js",.method=HTTP_GET,.handler=script},
        {.uri="/api/eq",.method=HTTP_GET,.handler=read_eq},
        {.uri="/api/eq",.method=HTTP_POST,.handler=apply_eq},
        {.uri="/api/status",.method=HTTP_GET,.handler=status}};
    for(size_t i=0;i<sizeof(routes)/sizeof(routes[0]);i++)ESP_ERROR_CHECK(httpd_register_uri_handler(server,&routes[i]));
    presets_register(server);
    source_control_register(server);
    ESP_LOGI("bldsp", "Ready: Wi-Fi BLDSP-Control, http://192.168.4.1/; USB host waiting for DSP");
}
