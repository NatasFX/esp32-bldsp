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

static const char *TAG="dsp_usb";
static usb_host_client_handle_t client;
static usb_device_handle_t dev;
static uint8_t address;
static int iface=-1;
static bool gone, in_transfer, finished, profile_ok;
static bool boot_ready;
static int64_t boot_retry;
static usb_transfer_t *transfer;
static QueueHandle_t queue;
static SemaphoreHandle_t lock, done;
typedef struct {int op; uint8_t module; uint16_t words[53]; esp_err_t result;} request_t;
static request_t request;

static void event(const usb_host_client_event_msg_t *msg, void *arg) {
    (void)arg;
    if(msg->event==USB_HOST_CLIENT_EVENT_NEW_DEV && !dev) address=msg->new_dev.address;
    if(msg->event==USB_HOST_CLIENT_EVENT_DEV_GONE && msg->dev_gone.dev_hdl==dev) {
        gone=true; profile_ok=false;
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
    if(iface>=0) usb_host_interface_release(client,dev,iface);
    usb_host_device_close(client,dev);
    dev=NULL;iface=-1;gone=false;profile_ok=false;boot_ready=false;boot_retry=0;
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
                    ESP_LOGI(TAG,"Vendor HID interface %d, reports 256 bytes",iface);return;
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
static esp_err_t execute(request_t *r) {
    if(!dev || gone || iface<0)return ESP_ERR_INVALID_STATE;
    esp_err_t err;uint8_t frame[256];
    if(!profile_ok && (err=check_profile())!=ESP_OK)return err;
    if(r->op==1) {
        if(!dsp_encode_eq_module(frame,r->module,r->words))return ESP_ERR_INVALID_ARG;
        err=send_frame(frame);if(err!=ESP_OK)return err;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    err=query(r->module,frame);if(err!=ESP_OK)return err;
    return dsp_decode_eq_module(frame,256,r->module,r->words)?ESP_OK:ESP_ERR_INVALID_RESPONSE;
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
        if(dev&&!gone&&iface>=0&&!boot_ready&&esp_timer_get_time()>=boot_retry) {
            boot_ready=apply_boot_preset();boot_retry=esp_timer_get_time()+2000000;
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
