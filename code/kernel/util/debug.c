#include "debug.h"
#include "printf.h"
#include "string.h"
#include "stdio.h"

void dbg_print(int m_level, int s_level, const char* file, const char* func, int line, const char* fmt, ...) {
    static const char* title[] = {[DBG_LEVEL_ERROR] = "error",
                                  [DBG_LEVEL_WARNING] = "warning",
                                  [DBG_LEVEL_INFO] = "info",
                                  [DBG_LEVEL_NONE] = "none"};
  if (m_level >= s_level) {
      const char* end = file + strlen(file);
      while (end >= file) {
          if ((*end == '\\') || (*end == '/')) { break; }
          end--;
      }
      end++;
      char str_buf[512];
      va_list args;
      va_start(args, fmt);
      vsnprintf(str_buf, sizeof(str_buf), fmt, args);
      va_end(args);
      cprintf("%s(%s-%s-%d):%s\n", title[s_level], end, func, line, str_buf);
  }
}