#ifndef __NET_NET_H__
#define __NET_NET_H__

#include "types.h"
#include "net_config.h"
#include "error.h"

#define NET_OK 0;

int net_init(void);
int net_start(void);
#endif