#ifndef __UTIL_DEBUG_H__
#define __UTIL_DEBUG_H__

#include "stdarg.h"
#include "types.h"

#define DBG_LEVEL_NONE 0
#define DBG_LEVEL_ERROR 1
#define DBG_LEVEL_WARNING 2
#define DBG_LEVEL_INFO 3

void dbg_print(int m_level, int s_level, const char* file, const char* func, int line, const char* fmt, ...);

#define dbg_info(module, fmt, ...) \
    dbg_print(module, DBG_LEVEL_INFO, __FILE__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define dbg_error(module, fmt, ...) \
    dbg_print(module, DBG_LEVEL_ERROR, __FILE__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define dbg_warning(module, fmt, ...) \
    dbg_print(module, DBG_LEVEL_WARNING, __FILE__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)


#define DBG_DISP_ENABLED(module) (module >= DBG_LEVEL_INFO)
#endif
