#include "nettool.h"

static int is_little_endian(void) {
    uint16_t v = 0x1234;
    uint8_t* b = (uint8_t*)&v;
    return b[0] == 0x34;
}


int tools_init(void) {
    dbg_info(DBG_TOOLS, "init tools.");
    if (is_little_endian() != NET_ENDIAN_LITTLE) {
        dbg_error(DBG_TOOLS, "check endian faild.");
        return -E_NET_SYS;
    }
    dbg_info(DBG_TOOLS, "done.");
    return NET_OK;
}

uint16_t checksum16(void* buf, uint16_t len, uint32_t pre_sum, bool complement){
    uint16_t* curr_buf = (uint16_t*)buf;
    uint32_t checksum = pre_sum;
    while (len > 1) {
        checksum += *curr_buf++;
        len -= 2;
    }
    if (len > 0) { checksum += *(uint8_t*)curr_buf; }
    uint16_t high;
    while ((high = checksum >> 16) != 0) { checksum = high + (checksum & 0xffff); }
    return complement ? (uint16_t)~checksum : (uint16_t)checksum;
}