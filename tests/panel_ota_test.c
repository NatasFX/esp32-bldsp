#include "panel_ota_protocol.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 assert(sizeof(panel_ota_request_t)<=250&&sizeof(panel_ota_ack_t)<=250);
 panel_ota_request_t p={.magic=PANEL_OTA_MAGIC,.version=1,.type=PANEL_OTA_REQUEST,.command=PANEL_OTA_DATA,.session=123,.total=1000,.length=200};
 assert(panel_ota_request_valid(&p));assert(panel_ota_data_action(&p,123,1000,0,true)==1);
 assert(panel_ota_data_action(&p,123,1000,200,true)==0); // lost ACK
 assert(panel_ota_data_action(&p,124,1000,200,true)==-1); // wrong session
 assert(panel_ota_data_action(&p,123,999,200,true)==-1);
 p.offset=400;assert(panel_ota_data_action(&p,123,1000,200,true)==-1); // gap
 p.offset=100;assert(panel_ota_data_action(&p,123,1000,200,true)==-1); // overlap
 p.offset=200;assert(panel_ota_data_action(&p,123,1000,200,false)==-1); // expired writer
 p.offset=900;assert(!panel_ota_request_valid(&p));
 p.offset=UINT32_MAX;assert(!panel_ota_request_valid(&p));
 p.offset=0;p.length=201;assert(!panel_ota_request_valid(&p));
 p.length=200;p.session=0;assert(!panel_ota_request_valid(&p));
 p.session=123;p.total=PANEL_OTA_MAX_SIZE+1;assert(!panel_ota_request_valid(&p));
 p.total=1000;p.command=PANEL_OTA_BEGIN;assert(!panel_ota_request_valid(&p));
 p.length=0;assert(panel_ota_request_valid(&p));
 p.command=99;assert(!panel_ota_request_valid(&p));
 p.command=PANEL_OTA_END;p.version=2;assert(!panel_ota_request_valid(&p));
 puts("OTA: bounded ESP-NOW frames, sessions, retries, gaps, overlap, inactive writer and integer boundaries passed");return 0;
}
