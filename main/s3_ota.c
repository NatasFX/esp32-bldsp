#include "s3_ota.h"
#include "panel_ota.h"
#include "firmware_image.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_app_desc.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static uint32_t boot;
static esp_err_t answer(httpd_req_t *r,const char *status,const char *error){
 const esp_partition_t *part=esp_ota_get_running_partition();esp_ota_img_states_t state=ESP_OTA_IMG_UNDEFINED;
 esp_ota_get_state_partition(part,&state);uint8_t sha[32]={0};esp_partition_get_sha256(part,sha);char hash[65];
 for(unsigned i=0;i<32;i++)snprintf(hash+i*2,3,"%02x",sha[i]);
 cJSON *o=cJSON_CreateObject();cJSON_AddNumberToObject(o,"boot",boot);cJSON_AddNumberToObject(o,"address",part->address);cJSON_AddStringToObject(o,"partition",part->label);cJSON_AddStringToObject(o,"sha256",hash);cJSON_AddBoolToObject(o,"pending",state==ESP_OTA_IMG_PENDING_VERIFY);cJSON_AddBoolToObject(o,"supported",esp_ota_get_next_update_partition(NULL)!=NULL);if(error)cJSON_AddStringToObject(o,"error",error);
 char *s=cJSON_PrintUnformatted(o);httpd_resp_set_status(r,status);httpd_resp_set_type(r,"application/json");httpd_resp_set_hdr(r,"Cache-Control","no-store");esp_err_t e=httpd_resp_sendstr(r,s);free(s);cJSON_Delete(o);return e;
}
static esp_err_t status_route(httpd_req_t *r){return answer(r,"200 OK",NULL);}
static bool hex_digest(const char *s,uint8_t out[32]){if(strlen(s)!=64)return false;for(unsigned i=0;i<32;i++){unsigned x=0;for(unsigned j=0;j<2;j++){unsigned c=(unsigned char)s[i*2+j];unsigned n=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:99;if(n>15)return false;x=x*16+n;}out[i]=x;}return true;}
static esp_err_t upload(httpd_req_t *r){
 char auth[8],hash[65];uint8_t expected[32];
 if(httpd_req_get_hdr_value_str(r,"X-BLDSP",auth,sizeof(auth))!=ESP_OK||strcmp(auth,"1"))return answer(r,"403 Forbidden","Cabeçalho ausente");
 if(httpd_req_get_hdr_value_str(r,"X-Image-SHA256",hash,sizeof(hash))!=ESP_OK||!hex_digest(hash,expected))return answer(r,"400 Bad Request","SHA256 da imagem ausente/inválido");
 if(panel_ota_busy())return answer(r,"409 Conflict","Aguarde o OTA do C3");
 const esp_partition_t *part=esp_ota_get_next_update_partition(NULL);
 if(!part||r->content_len<288||(unsigned)r->content_len>part->size)return answer(r,"400 Bad Request","Imagem fora do tamanho permitido");
 uint8_t *buffer=malloc(8192);if(!buffer)return answer(r,"503 Service Unavailable","Memória insuficiente");
 unsigned prefix=0;while(prefix<288){int n=httpd_req_recv(r,(char*)buffer+prefix,288-prefix);if(n<=0){free(buffer);return answer(r,"400 Bad Request","Upload interrompido antes do cabeçalho");}prefix+=n;}
 if(!firmware_image_valid(buffer,prefix,9,"bldsp_control")){free(buffer);return answer(r,"400 Bad Request","Selecione bldsp_control.bin para ESP32-S3");}
 esp_ota_handle_t handle=0;esp_err_t e=esp_ota_begin(part,r->content_len,&handle);bool opened=e==ESP_OK;unsigned offset=0;
 if(e==ESP_OK){e=esp_ota_write(handle,buffer,prefix);offset=prefix;}
 while(e==ESP_OK&&offset<(unsigned)r->content_len){unsigned size=r->content_len-offset;if(size>8192)size=8192;int n=httpd_req_recv(r,(char*)buffer,size);if(n<=0){e=ESP_ERR_TIMEOUT;break;}e=esp_ota_write(handle,buffer,n);offset+=n;}
 free(buffer);
 if(e==ESP_OK){e=esp_ota_end(handle);opened=false;}
 if(opened)esp_ota_abort(handle);
 if(e==ESP_OK){uint8_t actual[32];e=esp_partition_get_sha256(part,actual);if(e==ESP_OK&&memcmp(actual,expected,32))e=ESP_ERR_INVALID_CRC;}
 if(e==ESP_OK)e=esp_ota_set_boot_partition(part);
 ESP_LOGI("s3_ota","Upload bank=%s bytes=%u result=%s",part->label,offset,esp_err_to_name(e));
 if(e!=ESP_OK)return answer(r,"400 Bad Request",esp_err_to_name(e));
 esp_err_t sent=answer(r,"202 Accepted",NULL);vTaskDelay(pdMS_TO_TICKS(500));esp_restart();return sent;
}
extern const unsigned char update_start[] asm("_binary_s3_update_html_start");
extern const unsigned char update_end[] asm("_binary_s3_update_html_end");
static esp_err_t page(httpd_req_t *r){httpd_resp_set_type(r,"text/html; charset=utf-8");httpd_resp_set_hdr(r,"Cache-Control","no-store");return httpd_resp_send(r,(const char*)update_start,update_end-update_start-1);}
static void confirm(void *arg){
 (void)arg;vTaskDelay(pdMS_TO_TICKS(5000));const esp_partition_t *p=esp_ota_get_running_partition();esp_ota_img_states_t state;
 if(esp_ota_get_state_partition(p,&state)==ESP_OK&&state==ESP_OTA_IMG_PENDING_VERIFY){esp_err_t e=esp_ota_mark_app_valid_cancel_rollback();ESP_LOGI("s3_ota","Boot confirmation %s bank=%s",esp_err_to_name(e),p->label);}
 vTaskDelete(NULL);
}
void s3_ota_ready(void){if(xTaskCreate(confirm,"s3_confirm",3072,NULL,1,NULL)!=pdPASS)ESP_LOGE("s3_ota","No boot confirmation task");}
void s3_ota_register(httpd_handle_t server){
 boot=esp_random();const httpd_uri_t routes[]={{.uri="/s3-update",.method=HTTP_GET,.handler=page},{.uri="/api/s3/ota",.method=HTTP_GET,.handler=status_route},{.uri="/api/s3/ota",.method=HTTP_POST,.handler=upload}};
 for(unsigned i=0;i<3;i++)ESP_ERROR_CHECK(httpd_register_uri_handler(server,&routes[i]));
}
