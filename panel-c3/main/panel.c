#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_system.h"
#include "vcc_recovery.h"
#include "nvs_flash.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/gpio.h"
#include "panel_protocol.h"
#include "strip.h"
#include "ota.h"
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static uint64_t last_reply;
static panel_reply_t reply;
static void received(const esp_now_recv_info_t *info,const uint8_t *data,int len){
    if(!info||memcmp(info->src_addr,panel_primary_mac,6))return;
    if(panel_ota_receive(data,len))return;
    if(len!=sizeof(panel_reply_t))return;
    panel_reply_t r;memcpy(&r,data,sizeof(r));
    if(r.magic!=PANEL_MAGIC||r.version!=PANEL_VERSION||r.type!=2)return;
    panel_strip_receive(&r);
    portENTER_CRITICAL(&mux);reply=r;last_reply=esp_timer_get_time()/1000;portEXIT_CRITICAL(&mux);
}
static uint32_t remote_boot;
static void vcc_worker(void *arg){
    (void)arg;
    adc_oneshot_unit_handle_t adc=NULL;
    adc_oneshot_unit_init_cfg_t init={.unit_id=ADC_UNIT_2};
    adc_oneshot_chan_cfg_t cfg={.atten=ADC_ATTEN_DB_12,.bitwidth=ADC_BITWIDTH_12};
    adc_cali_handle_t cal=NULL;
    adc_cali_curve_fitting_config_t ccfg={.unit_id=ADC_UNIT_2,.atten=ADC_ATTEN_DB_12,.bitwidth=ADC_BITWIDTH_12};
    esp_err_t setup=adc_oneshot_new_unit(&init,&adc);
    if(setup==ESP_OK)setup=adc_oneshot_config_channel(adc,ADC_CHANNEL_0,&cfg);
    if(setup==ESP_OK)adc_cali_create_scheme_curve_fitting(&ccfg,&cal);
    panel_vcc_t p={.magic=PANEL_MAGIC,.boot=remote_boot,.version=PANEL_VERSION,.type=7};
    vcc_recovery_t recovery={0};
    while(1){
        vTaskDelay(pdMS_TO_TICKS(1000));
        if(panel_ota_busy())continue;
        int raw=0,mv=0;int64_t start=esp_timer_get_time();
        esp_err_t err=setup==ESP_OK?adc_oneshot_read(adc,ADC_CHANNEL_0,&raw):setup;
        p.duration_us=(uint32_t)(esp_timer_get_time()-start);p.sequence++;
        p.valid=err==ESP_OK&&raw>0&&raw<4095;p.error=err;p.raw=raw>=0&&raw<=4095?raw:0;p.mv=0;p.calibrated=0;
        if(p.valid){
            p.calibrated=cal&&adc_cali_raw_to_voltage(cal,raw,&mv)==ESP_OK;
            if(!p.calibrated)mv=raw*2500/4095;
            p.mv=mv<0?0:mv>65535?65535:mv;
        }
        unsigned action=vcc_recovery_step(&recovery,esp_timer_get_time()/1000,p.valid);
        if(p.valid)p.recovery_state=0;
        if(action){p.recovery_state=action;p.recoveries++;}
        esp_now_send(panel_primary_mac,(uint8_t*)&p,sizeof(p));
        if(action==1){
            ESP_LOGW("vcc","Recreate ADC2 attempt=%lu raw=%d err=%s",(unsigned long)p.recoveries,raw,esp_err_to_name(err));
            if(adc){setup=adc_oneshot_del_unit(adc);if(setup==ESP_OK)adc=NULL;}
            if(!adc){setup=adc_oneshot_new_unit(&init,&adc);if(setup==ESP_OK)setup=adc_oneshot_config_channel(adc,ADC_CHANNEL_0,&cfg);}
        }else if(action==2){
            // Notify first; S3 echoes state in its reply. Limit wait to one second.
            uint64_t since=esp_timer_get_time()/1000;bool ack=false;
            for(int i=0;i<10&&!panel_ota_busy();i++){
                vTaskDelay(pdMS_TO_TICKS(100));
                portENTER_CRITICAL(&mux);ack=last_reply>=since&&reply.reserved[1]==2;portEXIT_CRITICAL(&mux);
                if(ack)break;
                esp_now_send(panel_primary_mac,(uint8_t*)&p,sizeof(p));
            }
            ESP_LOGW("vcc","ADC2 recovery failed; C3 software restart, S3 ack=%u",ack);
            if(!panel_ota_busy())esp_restart();
        }
    }
}
void app_main(void){
    esp_err_t err=nvs_flash_init();
    if(err==ESP_ERR_NVS_NO_FREE_PAGES||err==ESP_ERR_NVS_NEW_VERSION_FOUND){ESP_ERROR_CHECK(nvs_flash_erase());err=nvs_flash_init();}
    ESP_ERROR_CHECK(err);ESP_ERROR_CHECK(esp_netif_init());ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));ESP_ERROR_CHECK(esp_wifi_set_channel(PANEL_CHANNEL,WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(8));
    ESP_ERROR_CHECK(esp_now_init());ESP_ERROR_CHECK(esp_now_set_pmk(panel_pmk));
    esp_now_peer_info_t peer={.channel=PANEL_CHANNEL,.ifidx=WIFI_IF_STA,.encrypt=true};
    memcpy(peer.peer_addr,panel_primary_mac,6);memcpy(peer.lmk,panel_lmk,16);ESP_ERROR_CHECK(esp_now_add_peer(&peer));
    panel_ota_start();
    ESP_ERROR_CHECK(esp_now_register_recv_cb(received));
    adc_oneshot_unit_handle_t adc;adc_oneshot_unit_init_cfg_t init={.unit_id=ADC_UNIT_1};ESP_ERROR_CHECK(adc_oneshot_new_unit(&init,&adc));
    adc_oneshot_chan_cfg_t acfg={.atten=ADC_ATTEN_DB_12,.bitwidth=ADC_BITWIDTH_12};adc_cali_handle_t cal=NULL;
    adc_cali_curve_fitting_config_t ccfg={.unit_id=ADC_UNIT_1,.atten=ADC_ATTEN_DB_12,.bitwidth=ADC_BITWIDTH_12};
    err=adc_cali_create_scheme_curve_fitting(&ccfg,&cal);if(err!=ESP_OK)ESP_LOGW("panel","ADC calibration unavailable; raw fallback");
    for(int i=0;i<5;i++)ESP_ERROR_CHECK(adc_oneshot_config_channel(adc,(adc_channel_t)i,&acfg));
    // SPI MOSI owns GPIO7; strip power is external.
    panel_strip_start();
    ESP_LOGI("panel","C3 panel: ADC GPIO0,1,2,3,4; WS2812 GPIO7; OLED unused; ESP-NOW ch6 2dBm encrypted");
    ESP_LOGI("panel","B10K wiring: 3V3 -> 3k3 -> pot high; low -> GND; wiper -> 1k -> ADC; ADC -> 100nF -> GND; fullscale 2480mV");
    panel_input_t packet={.magic=PANEL_MAGIC,.version=PANEL_VERSION,.type=1,.boot=esp_random()};
    remote_boot=packet.boot;
    if(xTaskCreate(vcc_worker,"vcc_adc2",3072,NULL,1,NULL)!=pdPASS)ESP_LOGW("panel","VCC worker unavailable");
    panel_adc_t diagnostics={.magic=PANEL_MAGIC,.version=PANEL_VERSION,.type=6,.boot=packet.boot};
    int filtered[5]={0};bool seeded[5]={0};uint64_t last_send=0,last_log=0,last_diagnostics=0;uint16_t last_position[5]={0};
    TickType_t cadence=xTaskGetTickCount();
    while(1){
        packet.valid_mask=0;diagnostics.valid_mask=0;diagnostics.calibrated_mask=0;
        for(int ch=0;ch<5;ch++){
            int sum=0,n=0;for(int j=0;j<16;j++){int raw;if(adc_oneshot_read(adc,(adc_channel_t)ch,&raw)==ESP_OK){sum+=raw;n++;}}
            if(n<12)continue;
            int raw=sum/n,mv=0;
            bool calibrated=cal&&adc_cali_raw_to_voltage(cal,raw,&mv)==ESP_OK;
            if(!calibrated)mv=raw*2500/4095;
            diagnostics.raw[ch]=raw;diagnostics.mv[ch]=mv<0?0:mv>65535?65535:mv;diagnostics.valid_mask|=1<<ch;
            if(calibrated)diagnostics.calibrated_mask|=1<<ch;
            if(mv<0)mv=0;
            if(mv>2480)mv=2480;
            int position=(int)((int64_t)mv*65535/2480);
            if(!seeded[ch]){filtered[ch]=position;seeded[ch]=true;}else filtered[ch]+=(position-filtered[ch])/4;
            packet.position[ch]=(uint16_t)filtered[ch];diagnostics.position[ch]=packet.position[ch];packet.valid_mask|=1<<ch;
        }
        uint16_t visual_positions[5];for(int i=0;i<5;i++)visual_positions[i]=packet.position[i];
        panel_strip_positions(visual_positions,packet.valid_mask);
        uint64_t now=esp_timer_get_time()/1000;bool moved=false;
        for(int i=0;i<5;i++)if(abs((int)packet.position[i]-last_position[i])>=400)moved=true;
        if((!panel_ota_busy()&&moved&&now-last_send>=100)||now-last_send>=500){
            packet.sequence++;err=esp_now_send(panel_primary_mac,(uint8_t*)&packet,sizeof(packet));
            if(err!=ESP_OK)ESP_LOGW("panel","ESP-NOW send: %s",esp_err_to_name(err));
            for(int i=0;i<5;i++)last_position[i]=packet.position[i];
            last_send=now;
        }
        if(!panel_ota_busy()&&now-last_diagnostics>=500){
            diagnostics.sequence=packet.sequence;esp_now_send(panel_primary_mac,(uint8_t*)&diagnostics,sizeof(diagnostics));last_diagnostics=now;
        }
        portENTER_CRITICAL(&mux);bool linked=last_reply&&now-last_reply<1500;portEXIT_CRITICAL(&mux);
        panel_ota_health(linked&&packet.valid_mask==31);
        if(now-last_log>=2000){
            portENTER_CRITICAL(&mux);uint64_t age=last_reply?now-last_reply:UINT64_MAX;panel_reply_t state=reply;portEXIT_CRITICAL(&mux);
            ESP_LOGI("panel","LINK=%s enabled=%d picked=%02x ADC=%u,%u,%u,%u,%u valid=%02x",age<2000?"OK":"WAIT",state.enabled,state.picked_mask,packet.position[0],packet.position[1],packet.position[2],packet.position[3],packet.position[4],packet.valid_mask);
            last_log=now;
        }
        vTaskDelayUntil(&cadence,pdMS_TO_TICKS(20));
    }
}
