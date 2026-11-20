#include <assert.h>
#include <stdio.h>
#include "firmware_image.h"
int main(void){unsigned char p[288]={0};p[0]=0xe9;p[12]=9;p[23]=1;p[32]=0x32;p[33]=0x54;p[34]=0xcd;p[35]=0xab;memcpy(p+80,"bldsp_control",14);
 assert(firmware_image_valid(p,sizeof(p),9,"bldsp_control"));
 assert(!firmware_image_valid(p,287,9,"bldsp_control"));assert(!firmware_image_valid(p,sizeof(p),5,"bldsp_control"));assert(!firmware_image_valid(p,sizeof(p),9,"bldsp_panel_c3"));p[23]=0;assert(!firmware_image_valid(p,sizeof(p),9,"bldsp_control"));p[23]=1;p[32]=0;assert(!firmware_image_valid(p,sizeof(p),9,"bldsp_control"));
 puts("OTA image: chip/project/descriptor/hash-header and truncated prefix checks OK");return 0;}
