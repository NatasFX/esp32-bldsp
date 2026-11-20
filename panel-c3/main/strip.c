#include "strip.h"
#include "strip_visual.h"
#include "strip_palette.h"
#include "ws2812_encode.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>
#define MAX_PIXELS 150
#define RESET_BYTES 128
#define FRAME_PERIOD_US 8333 // Approximately 120 Hz; animation speed remains time-based.
static spi_device_handle_t spi;
static uint8_t *encoded;
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static uint16_t pot_position[5];static uint8_t pot_valid;static uint32_t pot_generation;
static panel_reply_t latest;static uint64_t received_at;static uint32_t generation;
void panel_strip_receive(const panel_reply_t *r){
    portENTER_CRITICAL(&mux);latest=*r;received_at=esp_timer_get_time()/1000;generation++;portEXIT_CRITICAL(&mux);
}
void panel_strip_positions(const uint16_t adc_position[5],uint8_t valid){
    portENTER_CRITICAL(&mux);
    for(int i=0;i<5;i++)pot_position[i]=65535u-adc_position[i];
    pot_valid=valid;pot_generation++;
    portEXIT_CRITICAL(&mux);
}
static esp_err_t frame(unsigned count,unsigned mode,unsigned brightness,bool black,bool online,const strip_visual_t *state,uint64_t now){
    memset(encoded,0,count*12+RESET_BYTES);
    for(unsigned i=0;i<count;i++){
        strip_rgb_t c=black?(strip_rgb_t){0}:strip_visual_pixel(state,now,i,count,mode,brightness,online);
        // WS2812: GRB, 24 bits. Each bit becomes 1000 (0) or 1110 (1).
        // SPI 3.2MHz: 312.5/937.5ns and 937.5/312.5ns; trailing low 320us.
        ws2812_encode(c.r,c.g,c.b,encoded+i*12);
    }
    spi_transaction_t tx={.length=(count*12+RESET_BYTES)*8,.tx_buffer=encoded};
    return spi_device_transmit(spi,&tx);
}
static void frame_ready(void *arg){xTaskNotifyGive((TaskHandle_t)arg);}
static void worker(void *arg){
    (void)arg;strip_visual_t visual={0};uint32_t last_generation=0,last_pot_generation=0;unsigned previous_count=22;
    uint64_t started=esp_timer_get_time()/1000;bool was_online=false;uint64_t logged=0;
    esp_timer_handle_t cadence;
    const esp_timer_create_args_t timer_cfg={.callback=frame_ready,.arg=xTaskGetCurrentTaskHandle(),.name="strip120",.skip_unhandled_events=true};
    ESP_ERROR_CHECK(esp_timer_create(&timer_cfg,&cadence));
    ESP_ERROR_CHECK(esp_timer_start_periodic(cadence,FRAME_PERIOD_US));
    while(1){
        // Drop missed deadlines rather than replaying stale frames after a stall.
        ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
        uint64_t now=esp_timer_get_time()/1000;
        portENTER_CRITICAL(&mux);panel_reply_t r=latest;uint64_t at=received_at;uint32_t gen=generation;portEXIT_CRITICAL(&mux);
        bool online=at&&now-at<1500;
        uint16_t pots[5];uint8_t valid;uint32_t pg;
        portENTER_CRITICAL(&mux);memcpy(pots,pot_position,sizeof(pots));valid=pot_valid;pg=pot_generation;portEXIT_CRITICAL(&mux);
        if(pg!=last_pot_generation){strip_motion_observe(&visual.motion,now,pots,valid);last_pot_generation=pg;}
        unsigned count=gen?r.led_count:22,brightness=gen?r.led_brightness:30,mode=gen?r.led_mode:0;
        bool enabled=!gen||r.led_enabled;
        if(count<1||count>MAX_PIXELS)count=22;
        if(brightness>100)brightness=30;
        if(mode>2)mode=0;
        if(gen!=last_generation){
            strip_visual_observe(&visual,now,r.primary_boot,r.uptime_ms,r.clip_low_ms,r.clip_count,r.clip&&r.sensor_ready);last_generation=gen;
        }
        visual.palette=gen&&r.reserved[0]<STRIP_PALETTE_COUNT?r.reserved[0]:0;
        strip_visual_tick(&visual,now,online);
        if(count<previous_count)frame(previous_count,mode,0,true,online,&visual,now);
        unsigned fade=now-started<3000?0:now-started<4000?(unsigned)(now-started-3000):1000;
        esp_err_t err=frame(count,mode,brightness*fade/1000,!enabled||now-started<3000,online,&visual,now);previous_count=count;
        if(err!=ESP_OK){ESP_LOGE("strip","SPI failed: %s; strip disabled",esp_err_to_name(err));esp_timer_stop(cadence);esp_timer_delete(cadence);vTaskDelete(NULL);return;}
        if(online!=was_online||now-logged>5000){ESP_LOGI("strip","WS2812 SPI-DMA 120Hz GPIO7 pixels=%u brightness=%u%% mode=%u link=%d alert=%.2f duty=%.2f",count,brightness,mode,online,visual.envelope,visual.occupancy);was_online=online;logged=now;}
    }
}
void panel_strip_start(void){
    encoded=heap_caps_calloc(1,MAX_PIXELS*12+RESET_BYTES,MALLOC_CAP_DMA);
    if(!encoded){ESP_LOGE("strip","No DMA buffer; strip disabled");return;}
    const spi_bus_config_t bus={.mosi_io_num=7,.miso_io_num=-1,.sclk_io_num=-1,.quadwp_io_num=-1,.quadhd_io_num=-1,.max_transfer_sz=MAX_PIXELS*12+RESET_BYTES};
    esp_err_t err=spi_bus_initialize(SPI2_HOST,&bus,SPI_DMA_CH_AUTO);
    if(err!=ESP_OK){ESP_LOGE("strip","SPI bus: %s",esp_err_to_name(err));free(encoded);return;}
    const spi_device_interface_config_t device={.clock_speed_hz=3200000,.mode=0,.spics_io_num=-1,.queue_size=1};
    err=spi_bus_add_device(SPI2_HOST,&device,&spi);
    if(err!=ESP_OK){ESP_LOGE("strip","SPI device: %s",esp_err_to_name(err));spi_bus_free(SPI2_HOST);free(encoded);return;}
    strip_visual_t black={0};err=frame(MAX_PIXELS,0,0,true,false,&black,0);
    if(err!=ESP_OK){ESP_LOGE("strip","Startup clear failed: %s",esp_err_to_name(err));return;}
    if(xTaskCreate(worker,"strip",3072,NULL,2,NULL)!=pdPASS)ESP_LOGE("strip","Cannot start strip task");
}
