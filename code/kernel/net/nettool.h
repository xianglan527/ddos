#ifndef __NET_NETTOOL_H__
#define __NET_NETTOOL_H__

#include "error.h"
#include "ipaddr.h"
#include "net_config.h"
#include "types.h"

static inline uint16_t swap_u16(uint16_t v) {
    uint16_t r = ((v & 0xFF) << 8) | ((v >> 8) & 0xFF);
    return r;
}

static inline uint32_t swap_u32(uint32_t v) {
    uint32_t r = (((v >> 0) & 0xFF) << 24)  
                 | (((v >> 8) & 0xFF) << 16)   
                 | (((v >> 16) & 0xFF) << 8)  
                 | (((v >> 24) & 0xFF) << 0);
    return r;
}

#if NET_ENDIAN_LITTLE  // 小端模式，需要转换
uint16_t swap_u16(uint16_t v);
uint32_t swap_u32(uint32_t v);
#define x_htons(v) swap_u16(v)
#define x_ntohs(v) swap_u16(v)
#define x_htonl(v) swap_u32(v)
#define x_ntohl(v) swap_u32(v)

#else
// 大端模式，保持不变
#define x_htons(v) (v)
#define x_ntohs(v) (v)
#define x_htonl(v) (v)
#define x_ntohl(v) (v)
#endif
int tools_init(void);
#endif