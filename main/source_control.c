#include "source_control.h"
#include "source_policy.h"
#include "status_led.h"
#include <string.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define MODE_GPIO GPIO_NUM_2
#define PULSE_US 100000
#define COOLDOWN_US 1500000
static SemaphoreHandle_t available;
static esp_timer_handle_t release_timer;
static int64_t last_start;
static bool has_pulsed, exhaustion_logged;
static source_policy_t policy;
static portMUX_TYPE policy_lock=portMUX_INITIALIZER_UNLOCKED;

static void release_line(void *arg) {
    (void)arg;
    ESP_ERROR_CHECK(gpio_set_level(MODE_GPIO,1));
    ESP_LOGI("source","GPIO%d released; pulse elapsed=%ld us",MODE_GPIO,(long)(esp_timer_get_time()-last_start));
    xSemaphoreGive(available);
}
void source_control_init(void) {
    available=xSemaphoreCreateBinary();configASSERT(available);
    ESP_ERROR_CHECK(gpio_set_level(MODE_GPIO,1)); // released latch before output enable
    gpio_config_t cfg={.pin_bit_mask=1ULL<<MODE_GPIO,.mode=GPIO_MODE_OUTPUT_OD,
        .pull_up_en=GPIO_PULLUP_DISABLE,.pull_down_en=GPIO_PULLDOWN_DISABLE,.intr_type=GPIO_INTR_DISABLE};
    ESP_ERROR_CHECK(gpio_config(&cfg));
    const esp_timer_create_args_t timer={.callback=release_line,.name="source_release"};
    ESP_ERROR_CHECK(esp_timer_create(&timer,&release_timer));
    xSemaphoreGive(available);
    ESP_LOGI("source","Auto Bluetooth GPIO%d open-drain; waiting for USB; pulse=100 ms",MODE_GPIO);
}
void source_control_usb_connected(uint16_t pid) {
    portENTER_CRITICAL(&policy_lock);
    source_policy_connected(&policy,pid,esp_timer_get_time());
    if(pid==SOURCE_BLUETOOTH_PID)exhaustion_logged=false;
    unsigned attempts=policy.attempts;
    portEXIT_CRITICAL(&policy_lock);
    status_led_usb(true,pid==SOURCE_BLUETOOTH_PID);
    if(pid==SOURCE_BLUETOOTH_PID)ESP_LOGI("source","Target USB 171e confirmed; Bluetooth/HID mode; no pulse");
    else ESP_LOGI("source","USB audio mode %04x; wait for stable enumeration; attempts=%u",pid,attempts);
}
void source_control_usb_disconnected(void) {
    portENTER_CRITICAL(&policy_lock);source_policy_disconnected(&policy);portEXIT_CRITICAL(&policy_lock);
    status_led_usb(false,false);
}
bool source_control_ready(void) {
    portENTER_CRITICAL(&policy_lock);bool ready=source_policy_target(&policy);portEXIT_CRITICAL(&policy_lock);return ready;
}
static esp_err_t start_pulse(bool automatic) {
    if(xSemaphoreTake(available,0)!=pdTRUE)return ESP_ERR_TIMEOUT;
    int64_t now=esp_timer_get_time();
    if(has_pulsed && now-last_start<COOLDOWN_US){xSemaphoreGive(available);return ESP_ERR_TIMEOUT;}
    portENTER_CRITICAL(&policy_lock);
    bool allowed=policy.present && policy.pid==SOURCE_USB_AUDIO_PID && (!automatic || source_policy_due(&policy,now));
    if(allowed){
        if(!automatic)policy.attempts=0; // explicit recovery after exhausted automatic attempts
        source_policy_pulsed(&policy,now);exhaustion_logged=false;
    }
    unsigned attempt=policy.attempts;
    portEXIT_CRITICAL(&policy_lock);
    if(!allowed){xSemaphoreGive(available);return ESP_ERR_INVALID_STATE;}
    status_led_source_error(false);
    last_start=now;has_pulsed=true;
    ESP_ERROR_CHECK(gpio_set_level(MODE_GPIO,0));
    esp_err_t err=esp_timer_start_once(release_timer,PULSE_US);
    if(err!=ESP_OK){gpio_set_level(MODE_GPIO,1);xSemaphoreGive(available);
        status_led_source_error(true);
        ESP_LOGE("source","Timer failed: %s; line released",esp_err_to_name(err));return err;}
    ESP_LOGI("source","%s pulse GPIO%d low 100 ms, attempt=%u; awaiting USB 171e",automatic?"Automatic":"Requested",MODE_GPIO,attempt);
    return ESP_OK;
}
void source_control_poll(void) {
    int64_t now=esp_timer_get_time();
    portENTER_CRITICAL(&policy_lock);
    bool due=source_policy_due(&policy,now);
    bool exhausted=policy.present && policy.pid==SOURCE_USB_AUDIO_PID &&
        policy.attempts>=SOURCE_MAX_ATTEMPTS && now>=policy.next_attempt && !exhaustion_logged;
    if(exhausted)exhaustion_logged=true;
    portEXIT_CRITICAL(&policy_lock);
    if(due)start_pulse(true);
    if(exhausted){status_led_source_error(true);ESP_LOGE("source","No target USB 171e after %d attempts; stopped pulses, check GPIO2 wiring",SOURCE_MAX_ATTEMPTS);}
}
static esp_err_t response(httpd_req_t *r,const char *code,const char *json) {
    httpd_resp_set_status(r,code);httpd_resp_set_type(r,"application/json");
    httpd_resp_set_hdr(r,"Cache-Control","no-store");return httpd_resp_sendstr(r,json);
}
static esp_err_t pulse(httpd_req_t *r) {
    char header[8];
    if(httpd_req_get_hdr_value_str(r,"X-BLDSP",header,sizeof(header))!=ESP_OK || strcmp(header,"1"))
        return response(r,"403 Forbidden","{\"error\":\"Cabecalho ausente\"}");
    if(r->content_len!=2)return response(r,"400 Bad Request","{\"error\":\"Envie {}\"}");
    char body[2];int got=0;
    while(got<2){int n=httpd_req_recv(r,body+got,2-got);if(n<=0)return ESP_FAIL;got+=n;}
    if(memcmp(body,"{}",2))return response(r,"400 Bad Request","{\"error\":\"Envie {}\"}");
    if(source_control_ready())return response(r,"200 OK","{\"accepted\":true,\"already_target\":true,\"pulse_ms\":0}");
    esp_err_t err=start_pulse(false);
    if(err==ESP_ERR_TIMEOUT)return response(r,"429 Too Many Requests","{\"error\":\"Aguarde o pulso anterior\"}");
    if(err==ESP_ERR_INVALID_STATE)return response(r,"503 Service Unavailable","{\"error\":\"Aguardando USB da BLDSP\"}");
    if(err!=ESP_OK)return response(r,"500 Internal Server Error","{\"error\":\"Falha no temporizador\"}");
    return response(r,"202 Accepted","{\"accepted\":true,\"pulse_ms\":100}");
}
void source_control_register(httpd_handle_t server) {
    const httpd_uri_t route={.uri="/api/source/pulse",.method=HTTP_POST,.handler=pulse};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server,&route));
}
