#include "plic.h"

#include "memlayout.h"
#include "proc.h"
void plic_init(void) {
    // set desired IRQ priorities non-zero (otherwise disabled).
    *(uint32_t*)(PLIC + UART0_IRQ * 4) = 1;
    for(int i = 1; i <= VIRTIO_DEVICE_NUM; i++){
        *(uint32_t*)(PLIC + i* 4) = 1;
    }
    // *(uint32_t*)(PLIC + VIRTIO0_IRQ * 4) = 1;
    // *(uint32_t*)(PLIC + VIRTIO1_IRQ * 4) = 1;
}

void plic_init_hart(void) {
    int hart = cpuid();

    // set uart's enable bit for this hart's S-mode.
    *(uint32_t*)PLIC_SENABLE(hart) = (1 << UART0_IRQ);

    for(int i = 1; i <= VIRTIO_DEVICE_NUM; i++){
        *(uint32_t*)PLIC_SENABLE(hart) |=  (1 << i);
    }

    // set this hart's S-mode priority threshold to 0.
    *(uint32_t*)PLIC_SPRIORITY(hart) = 0;
}

// ask the PLIC what interrupt we should serve.
int plic_claim(void) {
    int hart = cpuid();
    int irq = *(uint32_t*)PLIC_SCLAIM(hart);
    return irq;
}

// tell the PLIC we've served this IRQ.
void plic_complete(int irq) {
    int hart = cpuid();
    *(uint32_t*)PLIC_SCLAIM(hart) = irq;
}