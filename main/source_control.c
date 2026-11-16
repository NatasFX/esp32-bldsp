#include "source_control.h"
#include <string.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define MODE_GPIO GPIO_NUM_4
#define PULSE_US 100000
#define COOLDOWN_US 1500000
static SemaphoreHandle_t available;
static esp_timer_handle_t release_timer;
static int64_t last_start;
static bool has_pulsed;

static void release_line(void *arg) {
    (void)arg;
    ESP_ERROR_CHECK(gpio_set_level(MODE_GPIO,1)); // open drain: release, never drive high
    ESP_LOGI("source","GPIO%d released; pulse elapsed=%ld us",MODE_GPIO,(long)(esp_timer_get_time()-last_start));
    xSemaphoreGive(available);
}
void source_control_init(void) {
    available=xSemaphoreCreateBinary();configASSERT(available);
    // Preload the released output latch before enabling open-drain output.
    ESP_ERROR_CHECK(gpio_set_level(MODE_GPIO,1));
    gpio_config_t cfg={.pin_bit_mask=1ULL<<MODE_GPIO,.mode=GPIO_MODE_OUTPUT_OD,
        .pull_up_en=GPIO_PULLUP_DISABLE,.pull_down_en=GPIO_PULLDOWN_DISABLE,.intr_type=GPIO_INTR_DISABLE};
    ESP_ERROR_CHECK(gpio_config(&cfg));
    const esp_timer_create_args_t timer={.callback=release_line,.name="source_release"};
    ESP_ERROR_CHECK(esp_timer_create(&timer,&release_timer));
    xSemaphoreGive(available);
    ESP_LOGI("source","Manual mode control GPIO%d open-drain released; pulse=100 ms; no boot pulse",MODE_GPIO);
}
static esp_err_t response(httpd_req_t *r,const char *code,const char *json) {
    httpd_resp_set_status(r,code);httpd_resp_set_type(r,"application/json");
    httpd_resp_set_hdr(r,"Cache-Control","no-store");return httpd_resp_sendstr(r,json);
}
static esp_err_t pulse(httpd_req_t *r) {
    char header[8];
    if(httpd_req_get_hdr_value_str(r,"X-BLDSP",header,sizeof(header))!=ESP_OK || strcmp(header,"1"))
        return response(r,"403 Forbidden","{\"error\":\"Cabeçalho ausente\"}");
    // Only an explicit empty JSON object is accepted. Duration/pin are not remotely configurable.
    if(r->content_len!=2)return response(r,"400 Bad Request","{\"error\":\"Envie {}\"}");
    char body[2];int got=0;
    while(got<2){int n=httpd_req_recv(r,body+got,2-got);if(n<=0)return ESP_FAIL;got+=n;}
    if(memcmp(body,"{}",2))return response(r,"400 Bad Request","{\"error\":\"Envie {}\"}");
    if(xSemaphoreTake(available,0)!=pdTRUE)
        return response(r,"429 Too Many Requests","{\"error\":\"Pulso em andamento\"}");
    int64_t now=esp_timer_get_time();
    if(has_pulsed && now-last_start<COOLDOWN_US){
        xSemaphoreGive(available);
        return response(r,"429 Too Many Requests","{\"error\":\"Aguarde antes de trocar novamente\"}");
    }
    last_start=now;has_pulsed=true;
    ESP_ERROR_CHECK(gpio_set_level(MODE_GPIO,0));
    esp_err_t err=esp_timer_start_once(release_timer,PULSE_US);
    if(err!=ESP_OK){gpio_set_level(MODE_GPIO,1);xSemaphoreGive(available);
        ESP_LOGE("source","Pulse timer: %s; line released",esp_err_to_name(err));
        return response(r,"500 Internal Server Error","{\"error\":\"Falha no temporizador\"}");}
    ESP_LOGI("source","Manual source pulse started GPIO%d low for 100 ms",MODE_GPIO);
    return response(r,"202 Accepted","{\"accepted\":true,\"pulse_ms\":100}");
}
void source_control_register(httpd_handle_t server) {
    const httpd_uri_t route={.uri="/api/source/pulse",.method=HTTP_POST,.handler=pulse};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server,&route));
}
