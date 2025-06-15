#ifndef __CONFIG_ERROR_H__
#define __CONFIG_ERROR_H__
typedef enum {
    ERR_BEGIN = 0,
#define ERROR(k, s) k,
#include "error_define.h"
#undef ERROR
    ERR_END
} ErrorCode;
// #define MAXERROR 48
#endif
