#include "dsp_usb.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "usb/usb_host.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "presets.h"
#include "source_control.h"
#include "status_led.h"
#include "pregain_control.h"

static const char *TAG="dsp_usb";
static usb_host_client_handle_t client;
static usb_device_handle_t dev;
static uint8_t address;
static int iface=-1;
static bool gone, in_transfer, finished, profile_ok;
static bool boot_ready;
// Owned exclusively by the USB task; presets/front retain the unattenuated gain.
static bool sub_base_valid;
static uint16_t sub_base_gain;
static unsigned sub_attenuation_q8;
static int64_t boot_retry;
static usb_transfer_t *transfer;
static QueueHandle_t queue;
static SemaphoreHandle_t lock, done;
typedef struct {int op; uint8_t module; uint16_t words[53]; esp_err_t result;} request_t;
static request_t request;
static portMUX_TYPE cache_mux=portMUX_INITIALIZER_UNLOCKED;
static uint16_t cached[2][DSP_EQ_WORDS];static bool cache_valid[2];static uint32_t cache_revision;
bool dsp_eq_snapshot(uint8_t module,uint16_t words[DSP_EQ_WORDS],uint32_t *revision){
    if(module!=0x91&&module!=0x93)return false;
    unsigned i=module==0x93;
    portENTER_CRITICAL(&cache_mux);bool valid=cache_valid[i];if(valid)memcpy(words,cached[i],106);if(revision)*revision=cache_revision;portEXIT_CRITICAL(&cache_mux);return valid;
}
static void cache_update(uint8_t module,const uint16_t *words){
    unsigned i=module==0x93;portENTER_CRITICAL(&cache_mux);
    if(!cache_valid[i]||memcmp(cached[i],words,106)){memcpy(cached[i],words,106);cache_valid[i]=true;cache_revision++;}
    portEXIT_CRITICAL(&cache_mux);
}

static void event(const usb_host_client_event_msg_t *msg, void *arg) {
    (void)arg;
    if(msg->event==USB_HOST_CLIENT_EVENT_NEW_DEV && !dev) address=msg->new_dev.address;
    if(msg->event==USB_HOST_CLIENT_EVENT_DEV_GONE && msg->dev_gone.dev_hdl==dev) {
        gone=true; profile_ok=false;
        source_control_usb_disconnected();
    }
}
static void complete(usb_transfer_t *t) {(void)t; finished=true;}
static esp_err_t control(uint8_t type, uint8_t req, uint16_t value,
                         uint16_t index, uint8_t *data, size_t len) {
    if(!dev || gone || len>512) return ESP_ERR_INVALID_STATE;
    uint8_t *b=transfer->data_buffer;
    memset(b,0,8+512);
    b[0]=type;b[1]=req;b[2]=value;b[3]=value>>8;
    b[4]=index;b[5]=index>>8;b[6]=len;b[7]=len>>8;
    if(!(type&0x80)) memcpy(b+8,data,len);
    transfer->device_handle=dev; transfer->bEndpointAddress=0;
    transfer->num_bytes=8+len; transfer->callback=complete;
    finished=false;
    esp_err_t err=usb_host_transfer_submit_control(client,transfer);
    if(err!=ESP_OK) return err;
    in_transfer=true;
    int64_t deadline=esp_timer_get_time()+3000000;
    while(!finished) {
        usb_host_client_handle_events(client,pdMS_TO_TICKS(10));
        // Do not free/reuse a live DMA buffer. A wedged EP0 resets only the ESP,
        // never sends reset/boot/flash commands to the DSP.
        if(esp_timer_get_time()>deadline) {ESP_LOGE(TAG,"EP0 timeout; restarting ESP");esp_restart();}
    }
    in_transfer=false;
    if(transfer->status!=USB_TRANSFER_STATUS_COMPLETED) return ESP_FAIL;
    if(type&0x80) {
        if(transfer->actual_num_bytes != (int)(8+len)) return ESP_ERR_INVALID_SIZE;
        memcpy(data,b+8,len);
    }
    return ESP_OK;
}
static void close_device(void) {
    if(!dev || in_transfer) return;
    ESP_LOGI(TAG,"Closing USB device");
    source_control_usb_disconnected();
    if(iface>=0) usb_host_interface_release(client,dev,iface);
    usb_host_device_close(client,dev);
    dev=NULL;iface=-1;gone=false;profile_ok=false;boot_ready=false;boot_retry=0;sub_base_valid=false;
}
// Accept only the measured vendor usage, unnumbered 256-byte Input/Output.
static bool report_matches(const uint8_t *p,size_t n) {
    unsigned page=0,size=0,count=0; bool usage=false,input=false,output=false;
    for(size_t i=0;i<n;) {
        uint8_t tag=p[i++]; if(tag==0xfe) return false;
        size_t len=tag&3; if(len==3)len=4;
        if(i+len>n)return false;
        uint32_t v=0;for(size_t j=0;j<len;j++)v|=(uint32_t)p[i+j]<<(8*j);i+=len;
        switch(tag&0xfc) {
            case 0x04:page=v;break;
            case 0x08:if(page==0xff00 && v==0x55aa)usage=true;break;
            case 0x74:size=v;break;
            case 0x94:count=v;break;
            case 0x84:return false; // numbered reports require a different profile
            case 0xa4:case 0xb4:return false; // global PUSH/POP not implemented
            case 0x80:if(size*count==2048)input=true;break;
            case 0x90:if(size*count==2048)output=true;break;
        }
    }
    return usage && input && output;
}
static void open_device(void) {
    uint8_t addr=address;address=0;
    if(usb_host_device_open(client,addr,&dev)!=ESP_OK)return;
    const usb_device_desc_t *dd;
    const usb_config_desc_t *cfg;
    if(usb_host_get_device_descriptor(dev,&dd)!=ESP_OK || dd->idVendor!=0x8888 ||
       (dd->idProduct!=0x171e && dd->idProduct!=0x1719) ||
       usb_host_get_active_config_descriptor(dev,&cfg)!=ESP_OK) {close_device();return;}
    ESP_LOGI(TAG,"BLDSP %04x:%04x",dd->idVendor,dd->idProduct);
    const uint8_t *p=(const uint8_t*)cfg;
    int candidate=-1; size_t total=cfg->wTotalLength;
    for(size_t off=0;off+2<=total;) {
        unsigned len=p[off],type=p[off+1];
        if(len<2 || off+len>total)break;
        if(type==4 && len>=9) candidate=(p[off+5]==3 && p[off+3]==0)?p[off+2]:-1;
        if(type==0x21 && candidate>=0 && len>=9 && p[off+6]==0x22) {
            size_t report_len=p[off+7]|(p[off+8]<<8);
            uint8_t descriptor[512];
            if(report_len && report_len<=sizeof(descriptor) &&
               usb_host_interface_claim(client,dev,candidate,0)==ESP_OK) {
                iface=candidate;
                if(control(0x81,6,0x2200,iface,descriptor,report_len)==ESP_OK &&
                   report_matches(descriptor,report_len)) {
                    ESP_LOGI(TAG,"Vendor HID interface %d, reports 256 bytes",iface);
                    source_control_usb_connected(dd->idProduct);return;
                }
                usb_host_interface_release(client,dev,iface);iface=-1;
            }
        }
        off+=len;
    }
    ESP_LOGW(TAG,"Descriptor incompatible; refusing writes");close_device();
}
static esp_err_t send_frame(uint8_t *frame) {return control(0x21,9,0x0200,iface,frame,256);}
static esp_err_t query(uint8_t module,uint8_t *frame) {
    for(int attempt=0;attempt<3;attempt++) {
        dsp_frame(frame,module,NULL,0);
        esp_err_t err=send_frame(frame);if(err!=ESP_OK)return err;
        vTaskDelay(pdMS_TO_TICKS(50));
        err=control(0xa1,1,0x0100,iface,frame,256);
        if(err==ESP_OK && dsp_valid_frame(frame,256,module))return ESP_OK;
    }
    return ESP_ERR_INVALID_RESPONSE;
}
static esp_err_t check_profile(void) {
    uint8_t frame[256];
    esp_err_t err=query(0,frame);if(err!=ESP_OK)return err;
    const uint8_t version[]={0x30,0,0,4,1,0x23,1};
    if(frame[3]!=7 || memcmp(frame+4,version,7)) return ESP_ERR_NOT_SUPPORTED;
    // Version alone is not sufficient: require the measured module name.
    const uint8_t modules[]={0x91,0x93};
    const char *names[]={"2:Music Pre Eq","4:Rec Pre EQ"};
    for(int m=0;m<2;m++) {
    uint8_t idx=modules[m]-0x81+1;
    dsp_frame(frame,0x80,&idx,1);
    err=send_frame(frame);if(err!=ESP_OK)return err;
    vTaskDelay(pdMS_TO_TICKS(50));
    err=control(0xa1,1,0x0100,iface,frame,256);
    if(err!=ESP_OK || !dsp_valid_frame(frame,256,0x80))return ESP_ERR_INVALID_RESPONSE;
    const char *name=names[m];size_t name_len=strlen(name);
    bool found=false;
    for(int i=4;i+(int)name_len<=4+frame[3];i++)
        if(!memcmp(frame+i,name,name_len))found=true;
    if(!found)return ESP_ERR_NOT_SUPPORTED;
    }
    profile_ok=true;return ESP_OK;
}
static esp_err_t write_pregain(uint8_t module,uint16_t pregain) {
    uint8_t frame[256];
    uint16_t neighbour=pregain==0x8000?0x8001:(uint16_t)(pregain-1);
    dsp_encode_eq_parameter(frame,module,1,neighbour);
    esp_err_t err=send_frame(frame);if(err!=ESP_OK)return err;
    vTaskDelay(pdMS_TO_TICKS(10));
    dsp_encode_eq_parameter(frame,module,1,pregain);
    err=send_frame(frame);if(err!=ESP_OK)return err;
    vTaskDelay(pdMS_TO_TICKS(50));return ESP_OK;
}
static esp_err_t read_words(uint8_t module,uint16_t *words) {
    uint8_t frame[256];esp_err_t err=query(module,frame);
    if(err!=ESP_OK)return err;
    return dsp_decode_eq_module(frame,256,module,words)?ESP_OK:ESP_ERR_INVALID_RESPONSE;
}
static bool words_match(const uint16_t *a,const uint16_t *b) {
    for(int i=0;i<DSP_EQ_WORDS;i++)if(i!=2&&a[i]!=b[i])return false;
    return true;
}
static esp_err_t apply_sub_attenuation(unsigned reduction) {
    if(!sub_base_valid||reduction>24*256)return ESP_ERR_INVALID_STATE;
    uint16_t before[DSP_EQ_WORDS],after[DSP_EQ_WORDS];
    esp_err_t err=read_words(0x93,before);if(err!=ESP_OK)return err;
    uint16_t target=pregain_attenuated(sub_base_gain,reduction);
    if(before[1]!=target){err=write_pregain(0x93,target);if(err!=ESP_OK)return err;}
    err=read_words(0x93,after);if(err!=ESP_OK)return err;
    before[1]=target;
    if(!words_match(before,after))return ESP_ERR_INVALID_RESPONSE;
    sub_attenuation_q8=reduction;
    ESP_LOGI(TAG,"Sub protection confirmed: reduction=%.3f dB base=%.3f dB effective=%.3f dB",reduction/256.,(int16_t)sub_base_gain/256.,(int16_t)target/256.);
    return ESP_OK;
}
static esp_err_t execute(request_t *r);
static esp_err_t execute(request_t *r) {
    if(!dev||gone||iface<0)return ESP_ERR_INVALID_STATE;
    esp_err_t err;uint8_t frame[256];
    if(!profile_ok&&(err=check_profile())!=ESP_OK)return err;
    if(r->op==2)return apply_sub_attenuation(r->words[0]);
    if(r->op==3){
        unsigned selector=r->words[0];int16_t gain=(int16_t)r->words[1];
        if((selector!=1 && !(selector>=7&&selector<=52&&(selector-7)%5==0)) ||
           (selector==1?(gain < -60*256||gain>0):(gain < -24*256||gain>12*256)))return ESP_ERR_INVALID_ARG;
        uint32_t revision;
        if(!dsp_eq_snapshot(r->module,r->words,&revision)){err=read_words(r->module,r->words);if(err!=ESP_OK)return err;}
        if(r->module==0x93&&sub_base_valid)r->words[1]=sub_base_gain;
        // Gain knobs never enable an EQ/band or change type, Q or frequency.
        if(!r->words[0] || (selector!=1&&!r->words[selector-4]))return ESP_ERR_INVALID_STATE;
        unsigned type=selector==1?0:r->words[selector-3];
        if(selector!=1&&type>2)return ESP_ERR_NOT_SUPPORTED;
        r->words[selector]=(uint16_t)gain;r->op=1;
        return execute(r);
    }
    uint16_t requested[DSP_EQ_WORDS];
    if(r->op==1){
        memcpy(requested,r->words,sizeof(requested));
        if(r->module==0x93)r->words[1]=pregain_attenuated(requested[1],sub_attenuation_q8);
        // The USB task serializes all writers. Compare with its last confirmed
        // user state; a single gain change can use the measured scalar command.
        uint16_t previous[DSP_EQ_WORDS];uint32_t revision;int changed=-1,count=0;
        if(dsp_eq_snapshot(r->module,previous,&revision)){
            for(int i=0;i<DSP_EQ_WORDS;i++)if(i!=2&&previous[i]!=requested[i]){changed=i;count++;}
        }else count=2;
        bool scalar=count==1&&(changed==1||(changed>=7&&(changed-7)%5==0));
        ESP_LOGI(TAG,"EQ TX module=%02x path=%s selector=%d",r->module,scalar?"scalar":"bulk",changed);
        if(scalar){
            if(changed==1)err=write_pregain(r->module,r->words[1]);
            else {dsp_encode_eq_parameter(frame,r->module,changed,r->words[changed]);err=send_frame(frame);}
            if(err!=ESP_OK)return err;
        }else{
            if(!dsp_encode_eq_module(frame,r->module,r->words))return ESP_ERR_INVALID_ARG;
            err=send_frame(frame);if(err!=ESP_OK)return err;
            vTaskDelay(pdMS_TO_TICKS(50));
            // Bulk stores gain; neighbour+target updates live pre-gain.
            err=write_pregain(r->module,r->words[1]);if(err!=ESP_OK)return err;
        }
        uint16_t actual[DSP_EQ_WORDS];err=read_words(r->module,actual);if(err!=ESP_OK)return err;
        if(scalar&&!words_match(r->words,actual)){
            ESP_LOGW(TAG,"Scalar readback mismatch; retrying confirmed bulk path");
            if(!dsp_encode_eq_module(frame,r->module,r->words))return ESP_ERR_INVALID_ARG;
            err=send_frame(frame);if(err!=ESP_OK)return err;
            vTaskDelay(pdMS_TO_TICKS(50));
            err=write_pregain(r->module,r->words[1]);if(err!=ESP_OK)return err;
            err=read_words(r->module,actual);if(err!=ESP_OK)return err;
        }
        if(!words_match(r->words,actual))return ESP_ERR_INVALID_RESPONSE;
        memcpy(r->words,actual,sizeof(actual));
        if(r->module==0x93){sub_base_gain=requested[1];sub_base_valid=true;r->words[1]=sub_base_gain;}
    }else{
        err=read_words(r->module,r->words);if(err!=ESP_OK)return err;
        if(r->module==0x93&&sub_base_valid){
            if(r->words[1]==pregain_attenuated(sub_base_gain,sub_attenuation_q8))r->words[1]=sub_base_gain;
            else ESP_LOGW(TAG,"Sub gain differs from last confirmed target; returning physical value until next write");
        }
    }
    cache_update(r->module,r->words);
    ESP_LOGI(TAG,"EQ RX module=%02x enabled=%u user_pregain=%.3f dB",r->module,r->words[0],(int16_t)r->words[1]/256.);
    return ESP_OK;
}
static bool apply_boot_preset(void) {
    uint16_t sub[53],stereo[53];
    esp_err_t err=presets_read(1,sub,stereo);
    if(err!=ESP_OK){ESP_LOGW(TAG,"Preset 1: %s",esp_err_to_name(err));return false;}
    uint16_t *banks[]={stereo,sub};const uint8_t modules[]={0x91,0x93};
    for(int i=0;i<2;i++) {
        request_t r={.op=0,.module=modules[i]};
        err=execute(&r);
        if(err!=ESP_OK){ESP_LOGW(TAG,"Read module %02x: %s",modules[i],esp_err_to_name(err));return false;}
        banks[i][2]=r.words[2];memcpy(r.words,banks[i],106);r.op=1;
        err=execute(&r);
        if(err!=ESP_OK){ESP_LOGW(TAG,"Apply module %02x: %s",modules[i],esp_err_to_name(err));return false;}
        for(int j=0;j<53;j++)if(j!=2 && r.words[j]!=banks[i][j]){ESP_LOGW(TAG,"Module %02x mismatch word %d",modules[i],j);return false;}
    }
    ESP_LOGI(TAG,"Preset 1 applied to stereo + sub; no DSP flash save");return true;
}
static void library_task(void *arg) {
    (void)arg;while(1){uint32_t flags;usb_host_lib_handle_events(portMAX_DELAY,&flags);}
}
static void client_task(void *arg) {
    (void)arg;
    usb_host_client_config_t cfg={.max_num_event_msg=5,.async={.client_event_callback=event}};
    ESP_ERROR_CHECK(usb_host_client_register(&cfg,&client));
    ESP_ERROR_CHECK(usb_host_transfer_alloc(520,0,&transfer));
    uint8_t addresses[8];int count=0;
    if(usb_host_device_addr_list_fill(8,addresses,&count)==ESP_OK && count>0)address=addresses[0];
    while(1) {
        usb_host_client_handle_events(client,pdMS_TO_TICKS(10));
        if(gone)close_device();
        if(address && !dev)open_device();
        source_control_poll();
        if(dev&&!gone&&iface>=0&&source_control_ready()&&!boot_ready&&esp_timer_get_time()>=boot_retry) {
            boot_ready=apply_boot_preset();boot_retry=esp_timer_get_time()+2000000;
            status_led_bootstrap(boot_ready);
        }
        int token;
        if(xQueueReceive(queue,&token,0)==pdTRUE){request.result=boot_ready?execute(&request):ESP_ERR_INVALID_STATE;xSemaphoreGive(done);}
    }
}
void dsp_usb_start(void) {
    lock=xSemaphoreCreateMutex();done=xSemaphoreCreateBinary();queue=xQueueCreate(1,sizeof(int));
    configASSERT(lock && done && queue);
    usb_host_config_t cfg={.intr_flags=ESP_INTR_FLAG_LEVEL1};
    ESP_ERROR_CHECK(usb_host_install(&cfg));
    ESP_LOGI(TAG,"USB host installed; native D- GPIO19, D+ GPIO20");
    xTaskCreate(library_task,"usb_lib",4096,NULL,5,NULL);
    xTaskCreate(client_task,"dsp_usb",6144,NULL,5,NULL);
}
static esp_err_t call(int op,uint8_t module,const uint16_t *in,uint16_t *out) {
    if(module!=0x91 && module!=0x93)return ESP_ERR_INVALID_ARG;
    if(xSemaphoreTake(lock,pdMS_TO_TICKS(100))!=pdTRUE)return ESP_ERR_TIMEOUT;
    request.op=op;request.module=module;if(in)memcpy(request.words,in,106);
    int token=1;xQueueSend(queue,&token,portMAX_DELAY);
    xSemaphoreTake(done,portMAX_DELAY);
    esp_err_t err=request.result;
    if(err==ESP_OK && out)memcpy(out,request.words,106);
    xSemaphoreGive(lock);return err;
}
esp_err_t dsp_read_eq(uint8_t module,uint16_t w[DSP_EQ_WORDS]){return call(0,module,NULL,w);}
esp_err_t dsp_apply_eq(uint8_t module,const uint16_t w[DSP_EQ_WORDS],uint16_t actual[DSP_EQ_WORDS]){return call(1,module,w,actual);}

esp_err_t dsp_set_sub_attenuation(unsigned q8){
    if(q8>24*256)return ESP_ERR_INVALID_ARG;
    uint16_t words[DSP_EQ_WORDS]={0};words[0]=(uint16_t)q8;return call(2,0x93,words,NULL);
}

esp_err_t dsp_set_gain(uint8_t module,uint8_t selector,int16_t gain){
    uint16_t words[DSP_EQ_WORDS]={selector,(uint16_t)gain};return call(3,module,words,NULL);
}
