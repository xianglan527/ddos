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