#include "memlayout.h"
#include "clint.h"
#include "proc.h"
#define CLINT_MSIP 0x0000
#define CLINT_MTIMECMP 0x4000
#define CLINT_MTIME 0xBFF8

#define CLINT_ADDR(reg) ((CLINT) + (reg))
#define CLINT_REG32_PTR(addr) (volatile uint32_t *)(CLINT_ADDR(addr))
#define CLINT_REG64_PTR(addr) (volatile uint64_t *)(CLINT_ADDR(addr))

#define CLINT_REG32(addr) (*CLINT_REG32_PTR(addr))
#define CLINT_REG64(addr) (*CLINT_REG64_PTR(addr))

void clint_set_msip(uchar val, uint64_t hart_id) {
    uint32_t reg = CLINT_MSIP + 4 * hart_id;
    CLINT_REG32(reg) = val ? 1 : 0;
}

void clint_set_ssip(bool val, uint64_t hart_id) {
    if (hart_id == cpuid()) {
        uint64_t sip = r_sip();
        if (val)
            sip |= SIP_SSIP;
        else
            sip &= ~SIP_SSIP;
        w_sip(sip);
    } else {
        clint_set_msip(val, hart_id);
    }
}

void clint_set_mtime(uint64_t val) { CLINT_REG64(CLINT_MTIME) = val; }

uint64_t clint_get_mtime(void) { return CLINT_REG64(CLINT_MTIME); }

void clint_set_mtimecmp(uint64_t val, uint64_t hart_id) {
    uint32_t reg = CLINT_MTIMECMP + 8 * hart_id;
    CLINT_REG64(reg) = val;
}

uint64_t clint_get_mtimecmp(uint64_t hart_id) {
    uint32_t reg = CLINT_MTIMECMP + 8 * hart_id;
    return CLINT_REG64(reg);
}

volatile uint64_t *clint_get_mtimecmp_ptr(uint64_t hart_id) {
    uint32_t reg = CLINT_MTIMECMP + 8 * hart_id;
    return CLINT_REG64_PTR(reg);
}

void clint_add_mtimecmp(uint64_t val, uint64_t hart_id) {
    uint32_t reg = CLINT_MTIMECMP + 8 * hart_id;
    CLINT_REG64(reg) += val;
}
