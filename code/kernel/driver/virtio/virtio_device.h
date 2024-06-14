#ifndef __VIRTIO_VIRTIO_DEVICE_H_
#define __VIRTIO_VIRTIO_DEVICE_H_
#include "types.h"

typedef enum device_type Device_type;
enum device_type{
    DEVICE_NONE = 0,
    DEVICE_NETWORK = 1,
    DEVICE_BLOCK = 2,
    DEVICE_CONSOLE = 3,
    DEVICE_ENTROPY = 4,
    DEVICE_GPU = 16,
    DEVICE_INPUT = 18,
    DEVICE_MEMORY = 24,
};

void virtio_device_init(void);
void virtio_device_intr_handler(int irq);

#endif