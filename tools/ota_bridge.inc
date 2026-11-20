
#ifdef PANEL_OTA_SMOKE_TEST
extern const unsigned char smoke_start[] asm("_binary_panel_smoke_bin_start");
extern const unsigned char smoke_end[] asm("_binary_panel_smoke_bin_end");
static void smoke_worker(void *arg){
 (void)arg;const uint32_t size=smoke_end-smoke_start;uint64_t deadline=now_ms()+60000;
 panel_ota_info_t before={0};bool ready=false;
 while(now_ms()<deadline){
  portENTER_CRITICAL(&mux);before=device;ready=seen&&now_ms()-seen<3000&&!before.pending;portEXIT_CRITICAL(&mux);
  if(ready)break;
  vTaskDelay(pdMS_TO_TICKS(500));
 }
 esp_err_t e=ESP_ERR_TIMEOUT;
 if(!ready){ESP_LOGE("ota_smoke","C3 absent; no update attempted");vTaskDelete(NULL);return;}
 session=esp_random();if(!session)session=1;total=size;offset=0;
 portENTER_CRITICAL(&mux);active=true;touched=now_ms();portEXIT_CRITICAL(&mux);
 ESP_LOGI("ota_smoke","Wireless C3 update BEGIN bytes=%lu",(unsigned long)size);
 panel_ota_request_t p=request(PANEL_OTA_BEGIN);e=exchange(&p,0);
 while(e==ESP_OK&&offset<size){
  portENTER_CRITICAL(&mux);touched=now_ms();portEXIT_CRITICAL(&mux);
  p=request(PANEL_OTA_DATA);p.length=size-offset>200?200:size-offset;memcpy(p.data,smoke_start+offset,p.length);
  e=exchange(&p,offset+p.length);if(e==ESP_OK)offset+=p.length;
  if(offset%64000==0)ESP_LOGI("ota_smoke","Progress %lu/%lu",(unsigned long)offset,(unsigned long)size);
 }
 if(e==ESP_OK){p=request(PANEL_OTA_END);e=exchange(&p,size);}
 if(e!=ESP_OK){p=request(PANEL_OTA_ABORT);esp_now_send(panel_remote_mac,(uint8_t*)&p,sizeof(p));}
 portENTER_CRITICAL(&mux);active=false;portEXIT_CRITICAL(&mux);
 if(e==ESP_OK){
  deadline=now_ms()+85000;bool verified=false;
  while(now_ms()<deadline){
   portENTER_CRITICAL(&mux);panel_ota_info_t after=device;uint64_t at=seen;portEXIT_CRITICAL(&mux);
   if(at&&now_ms()-at<3000&&after.boot!=before.boot&&!after.pending&&!memcmp(after.sha256,smoke_start+size-32,32)){verified=true;ESP_LOGI("ota_smoke","PASS: new boot, SHA256 match, confirmed bank=0x%lx",(unsigned long)after.address);break;}
   vTaskDelay(pdMS_TO_TICKS(500));
  }
  if(!verified)ESP_LOGE("ota_smoke","FAIL: new firmware not confirmed");
 }else ESP_LOGE("ota_smoke","FAIL: %s at offset=%lu",esp_err_to_name(e),(unsigned long)offset);
 vTaskDelete(NULL);
}
void panel_ota_smoke_start(void){configASSERT(xTaskCreate(smoke_worker,"ota_smoke",6144,NULL,2,NULL)==pdPASS);}
#endif

