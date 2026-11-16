#include "status_led.h"
#include "status_visual.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static portMUX_TYPE state_lock=portMUX_INITIALIZER_UNLOCKED;
static status_snapshot_t state;
static unsigned requests,error_modules;
static uint64_t services_ready_ms;
static rmt_channel_handle_t channel;
static rmt_encoder_handle_t encoder;
// Persistent payload: never reused before the TX completion or channel disable.
static rmt_symbol_word_t symbols[25];

void status_led_system_ready(void){
    portENTER_CRITICAL(&state_lock);
    if(!state.system_ready)services_ready_ms=(uint64_t)esp_timer_get_time()/1000;
    state.system_ready=true;
    portEXIT_CRITICAL(&state_lock);
    ESP_LOGI("status_led","Services ready; RGB remains off for 3000 ms, then fades in over 1000 ms");
}
void status_led_usb(bool present,bool target) {
    portENTER_CRITICAL(&state_lock);
    if(state.usb_present!=present || state.bluetooth_mode!=target){
        state.eq_ready=false;state.bootstrap_error=false;state.eq_error=false;
        state.source_error=false;error_modules=0;
    }
    state.usb_present=present;state.bluetooth_mode=target;
    portEXIT_CRITICAL(&state_lock);
}
void status_led_source_error(bool failed){portENTER_CRITICAL(&state_lock);state.source_error=failed;portEXIT_CRITICAL(&state_lock);}
void status_led_bootstrap(bool success) {
    portENTER_CRITICAL(&state_lock);state.eq_ready=success;state.bootstrap_error=!success;portEXIT_CRITICAL(&state_lock);
}
void status_led_request_begin(void){portENTER_CRITICAL(&state_lock);requests++;state.working=true;portEXIT_CRITICAL(&state_lock);}
void status_led_request_end(uint8_t module,bool success) {
    unsigned bit=module==0x91?1:2;
    portENTER_CRITICAL(&state_lock);
    if(requests)requests--;
    state.working=requests!=0;
    if(success)error_modules&=~bit;else error_modules|=bit;
    state.eq_error=error_modules!=0;
    portEXIT_CRITICAL(&state_lock);
}
static esp_err_t write_color(status_rgb_t c) {
    uint8_t grb[]={c.g,c.r,c.b};
    for(int i=0;i<24;i++){
        bool one=(grb[i/8] & (0x80>>(i%8)))!=0;
        symbols[i]=(rmt_symbol_word_t){.level0=1,.duration0=one?9:3,.level1=0,.duration1=one?3:9};
    }
    // 300 us reset accommodates WS2812 variants; clock resolution 10 MHz.
    symbols[24]=(rmt_symbol_word_t){.level0=0,.duration0=1500,.level1=0,.duration1=1500};
    const rmt_transmit_config_t tx={.loop_count=0};
    esp_err_t err=rmt_transmit(channel,encoder,symbols,sizeof(symbols),&tx);
    return err==ESP_OK?rmt_tx_wait_all_done(channel,20):err;
}
static void led_task(void *arg) {
    (void)arg;int previous=-1;bool indicators_started=false;
    const char *names[]={"boot blue","waiting USB amber","switching source purple","sync cyan","error red","operational rainbow"};
    while(1){
        portENTER_CRITICAL(&state_lock);status_snapshot_t snapshot=state;uint64_t ready_ms=services_ready_ms;portEXIT_CRITICAL(&state_lock);
        uint64_t now_ms=(uint64_t)esp_timer_get_time()/1000;
        if(!snapshot.system_ready || now_ms-ready_ms<3000){vTaskDelay(pdMS_TO_TICKS(40));continue;}
        if(!indicators_started){ESP_LOGI("status_led","Startup delay complete; enabling RGB indicators gradually");indicators_started=true;}
        status_kind_t kind=status_visual_kind(&snapshot);
        if((int)kind!=previous){ESP_LOGI("status_led","State: %s",names[kind]);previous=kind;}
        esp_err_t err=write_color(status_visual_startup_color(&snapshot,now_ms,ready_ms));
        if(err!=ESP_OK){
            ESP_LOGE("status_led","RMT failed: %s; indicator stopped, audio control continues",esp_err_to_name(err));
            rmt_disable(channel);vTaskDelete(NULL);return;
        }
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}
void status_led_init(void) {
    const rmt_tx_channel_config_t tx={.gpio_num=48,.clk_src=RMT_CLK_SRC_DEFAULT,
        .resolution_hz=10000000,.mem_block_symbols=64,.trans_queue_depth=1};
    esp_err_t err=rmt_new_tx_channel(&tx,&channel);
    if(err!=ESP_OK){ESP_LOGE("status_led","GPIO48 RMT init: %s",esp_err_to_name(err));return;}
    const rmt_copy_encoder_config_t copy={0};
    err=rmt_new_copy_encoder(&copy,&encoder);
    if(err==ESP_OK)err=rmt_enable(channel);
    if(err!=ESP_OK){
        ESP_LOGE("status_led","RGB encoder init: %s",esp_err_to_name(err));
        if(encoder)rmt_del_encoder(encoder);
        rmt_del_channel(channel);return;
    }
    // Explicit black frame: pulling DIN low alone does not clear a latched WS2812.
    err=write_color((status_rgb_t){0,0,0});
    if(err!=ESP_OK){
        ESP_LOGE("status_led","Cannot clear RGB: %s",esp_err_to_name(err));
        rmt_disable(channel);rmt_del_encoder(encoder);rmt_del_channel(channel);return;
    }
    ESP_LOGI("status_led","RGB explicitly off at startup; waiting for services and stabilization");
    if(xTaskCreate(led_task,"status_rgb",3072,NULL,2,NULL)!=pdPASS){
        ESP_LOGE("status_led","Cannot create RGB task");rmt_disable(channel);rmt_del_encoder(encoder);rmt_del_channel(channel);return;
    }
    ESP_LOGI("status_led","WS2812 GPIO48, GRB, max brightness 128/255, refresh 25 Hz");
}
