#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#define PANEL_OTA_MAGIC 0x4f424c44u
#define PANEL_OTA_VERSION 1
#define PANEL_OTA_REQUEST 3
#define PANEL_OTA_ACK 4
#define PANEL_OTA_INFO 5
#define PANEL_OTA_DATA_SIZE 200
#define PANEL_OTA_MAX_SIZE 0x1f0000u
#define PANEL_OTA_BEGIN 1
#define PANEL_OTA_DATA 2
#define PANEL_OTA_END 3
#define PANEL_OTA_ABORT 4
#pragma pack(push,1)
typedef struct {
 uint32_t magic,session,offset,total;
 uint8_t version,type,command,reserved;
 uint16_t length;
 uint8_t data[PANEL_OTA_DATA_SIZE];
} panel_ota_request_t;
typedef struct {
 uint32_t magic,session,offset;
 int32_t error;
 uint8_t version,type,command,reserved;
} panel_ota_ack_t;
typedef struct {
 uint32_t magic,boot,address;
 uint8_t version,type,pending,reserved;
 uint8_t sha256[32];
} panel_ota_info_t;
#pragma pack(pop)
static inline bool panel_ota_request_valid(const panel_ota_request_t *p){
 return p->magic==PANEL_OTA_MAGIC&&p->version==PANEL_OTA_VERSION&&p->type==PANEL_OTA_REQUEST&&p->session&&
 p->command>=PANEL_OTA_BEGIN&&p->command<=PANEL_OTA_ABORT&&p->length<=PANEL_OTA_DATA_SIZE&&
 p->total>=288&&p->total<=PANEL_OTA_MAX_SIZE&&p->offset<=p->total&&p->length<=p->total-p->offset&&
 (p->command==PANEL_OTA_DATA?p->length>0:p->length==0);
}

// 1=new contiguous data, 0=duplicate already written, -1=reject.
static inline int panel_ota_data_action(const panel_ota_request_t *p,uint32_t session,uint32_t total,uint32_t written,bool open){
 if(!panel_ota_request_valid(p)||p->command!=PANEL_OTA_DATA||p->session!=session||p->total!=total)return -1;
 if(p->offset<written&&p->offset+p->length<=written)return 0;
 return open&&p->offset==written?1:-1;
}
