#include "ota.h"
#include "panel_protocol.h"
#include "panel_ota_protocol.h"
#include "esp_ota_ops.h"
#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_random.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>
static QueueHandle_t inbox;
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static bool busy,healthy;
static uint64_t health_at;
static uint64_t now_ms(void){return esp_timer_get_time()/1000;}
bool panel_ota_busy(void){portENTER_CRITICAL(&mux);bool b=busy;portEXIT_CRITICAL(&mux);return b;}
void panel_ota_health(bool ok){portENTER_CRITICAL(&mux);healthy=ok;health_at=now_ms();portEXIT_CRITICAL(&mux);}
static void set_busy(bool b){portENTER_CRITICAL(&mux);busy=b;portEXIT_CRITICAL(&mux);}
bool panel_ota_receive(const uint8_t *data,int len){
 if(len!=(int)sizeof(panel_ota_request_t))return false;
 panel_ota_request_t p;memcpy(&p,data,sizeof(p));
 if(!panel_ota_request_valid(&p))return false;
 if(inbox)xQueueSend(inbox,&p,0);
 return true;
}
static void worker(void *arg){
 (void)arg;uint32_t session=0,offset=0,total=0,boot=esp_random();
 uint64_t touched=0,restart_at=0,started=now_ms(),last_info=0;
 esp_ota_handle_t handle=0;const esp_partition_t *target=NULL,*running=esp_ota_get_running_partition();
 esp_ota_img_states_t state=ESP_OTA_IMG_UNDEFINED;
 esp_err_t state_error=esp_ota_get_state_partition(running,&state);
 bool pending=state_error==ESP_OK&&state==ESP_OTA_IMG_PENDING_VERIFY;
 // First USB migration needs a VALID OTA entry, otherwise the first update
 // would have no recorded previous bank to roll back to.
 if(!pending&&(state_error!=ESP_OK||state==ESP_OTA_IMG_UNDEFINED)){
  ESP_ERROR_CHECK(esp_ota_set_boot_partition(running));
  ESP_ERROR_CHECK(esp_ota_mark_app_valid_cancel_rollback());
  ESP_LOGI("panel_ota","Initial USB image recorded as valid rollback baseline");
 }
 uint8_t sha[32]={0};esp_partition_get_sha256(running,sha);
 ESP_LOGI("panel_ota","OTA ready: running=%s address=0x%lx pending=%d slots=2",running->label,(unsigned long)running->address,pending);
 while(1){
  uint64_t now=now_ms();
  portENTER_CRITICAL(&mux);bool ok=healthy&&now-health_at<1500;portEXIT_CRITICAL(&mux);
  if(pending&&now-started>=20000&&ok){
   esp_err_t e=esp_ota_mark_app_valid_cancel_rollback();
   if(e==ESP_OK){pending=false;ESP_LOGI("panel_ota","New firmware confirmed: ADC and paired link healthy");}
  }
  if(pending&&now-started>60000){ESP_LOGE("panel_ota","Self-test timed out; rolling back");esp_ota_mark_app_invalid_rollback_and_reboot();esp_restart();}
  if(now-last_info>=1000){
   panel_ota_info_t info={.magic=PANEL_OTA_MAGIC,.boot=boot,.address=running->address,.version=PANEL_OTA_VERSION,.type=PANEL_OTA_INFO,.pending=pending};
   memcpy(info.sha256,sha,32);esp_now_send(panel_primary_mac,(uint8_t*)&info,sizeof(info));last_info=now;
  }
  if(restart_at&&now>=restart_at){ESP_LOGI("panel_ota","Validated image selected; rebooting");esp_restart();}
  if(handle&&now-touched>30000){esp_ota_abort(handle);handle=0;session=0;set_busy(false);ESP_LOGW("panel_ota","Transfer expired; current image retained");}
  panel_ota_request_t p;if(xQueueReceive(inbox,&p,pdMS_TO_TICKS(50))!=pdTRUE)continue;
  esp_err_t err=ESP_OK;
  if(p.command==PANEL_OTA_BEGIN){
   if(session==p.session&&total==p.total){/* Retried BEGIN preserves progress. */}
   else if(handle||restart_at)err=ESP_ERR_INVALID_STATE;
   else{
    target=esp_ota_get_next_update_partition(NULL);
    if(pending||!target||p.total>target->size)err=ESP_ERR_INVALID_STATE;
    else{
     err=esp_ota_begin(target,OTA_WITH_SEQUENTIAL_WRITES,&handle);
     if(err==ESP_OK){session=p.session;total=p.total;offset=0;set_busy(true);ESP_LOGI("panel_ota","Begin session=%lu bytes=%lu target=%s",(unsigned long)session,(unsigned long)total,target->label);}
    }
   }
  }else if(p.session!=session||p.total!=total)err=ESP_ERR_INVALID_STATE;
  else if(p.command==PANEL_OTA_ABORT){
   if(handle){esp_ota_abort(handle);handle=0;}session=0;set_busy(false);
  }else if(p.command==PANEL_OTA_DATA){
   int decision=panel_ota_data_action(&p,session,total,offset,handle!=0);
   if(decision==0){/* Lost ACK: do not write twice. */}
   else if(decision<0)err=ESP_ERR_INVALID_STATE;
   else{
    if(offset==0){
     esp_image_header_t h;memcpy(&h,p.data,sizeof(h));
     if(p.length<sizeof(h)||h.magic!=ESP_IMAGE_HEADER_MAGIC||h.chip_id!=ESP_CHIP_ID_ESP32C3||h.hash_appended!=1)err=ESP_ERR_INVALID_ARG;
    }
    if(err==ESP_OK)err=esp_ota_write(handle,p.data,p.length);
    if(err==ESP_OK)offset+=p.length;
   }
  }else if(p.command==PANEL_OTA_END){
   if(restart_at){/* Finish ACK may have been lost. */}
   else if(!handle||offset!=total)err=ESP_ERR_INVALID_STATE;
   else{
    esp_app_desc_t desc;err=esp_ota_get_partition_description(target,&desc);
    if(err==ESP_OK&&strcmp(desc.project_name,"bldsp_panel_c3"))err=ESP_ERR_INVALID_ARG;
    if(err==ESP_OK){err=esp_ota_end(handle);handle=0;}
    if(err==ESP_OK)err=esp_ota_set_boot_partition(target);
    if(err==ESP_OK)restart_at=now_ms()+1500;
   }
  }
  touched=now_ms();
  panel_ota_ack_t ack={.magic=PANEL_OTA_MAGIC,.session=p.session,.offset=offset,.error=err,.version=PANEL_OTA_VERSION,.type=PANEL_OTA_ACK,.command=p.command};
  esp_now_send(panel_primary_mac,(uint8_t*)&ack,sizeof(ack));
  if(err!=ESP_OK){
   ESP_LOGW("panel_ota","Command=%u offset=%lu error=%s",p.command,(unsigned long)offset,esp_err_to_name(err));
   if(handle&&p.session==session){esp_ota_abort(handle);handle=0;session=0;set_busy(false);}
  }
 }
}
void panel_ota_start(void){
 inbox=xQueueCreate(4,sizeof(panel_ota_request_t));configASSERT(inbox);
 configASSERT(xTaskCreate(worker,"panel_ota",6144,NULL,3,NULL)==pdPASS);
}
