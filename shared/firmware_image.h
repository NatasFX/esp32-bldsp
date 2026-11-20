#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
// ESP image + app descriptor, SHA appended. Header is checked before flash writes.
static inline int firmware_image_valid(const uint8_t *p,size_t n,unsigned chip,const char *project){
 return n>=288&&p[0]==0xe9&&p[12]==chip&&p[13]==0&&p[23]==1&&p[32]==0x32&&p[33]==0x54&&p[34]==0xcd&&p[35]==0xab&&strlen(project)<32&&!memcmp(p+80,project,strlen(project)+1);
}
