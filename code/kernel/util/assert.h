#ifndef __UTIL_ASSERT_H__
#define __UTIL_ASSERT_H__

void __panic(const char *file, int line, const char *fmt, ...);
void __warn(const char *file, int line, const char *fmt, ...);

#define warn(...) __warn(__FILE__, __LINE__, __VA_ARGS__)
#define panic(...) __panic(__FILE__, __LINE__, __VA_ARGS__)

#define assert(x)                                        \
    do {                                                 \
        if (!(x)) { panic("assertion failed: %s", #x); } \
    } while (0)

#define static_assert(x) \
    switch (x) {         \
        case 0:          \
        case (x):;       \
    }
#endif