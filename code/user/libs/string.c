#include "string.h"

void *memset(void *dst, int c, size_t n) {
    char *cdst = (char *)dst;
    for (size_t i = 0; i < n; i++) cdst[i] = c;
    return dst;
}

int memcmp(const void *v1, const void *v2, size_t n) {
    const uchar *s1, *s2;
    s1 = v1;
    s2 = v2;
    while (n-- > 0) {
        if (*s1 != *s2) return *s1 - *s2;
        s1++;
        s2++;
    }
    return 0;
}

void *memmove(void *dst, const void *src, size_t n) {
    const char *s;
    char *d;
    s = src;
    d = dst;
    if (s < d && s + n > d) {
        s += n;
        d += n;
        while (n-- > 0) *--d = *--s;
    } else {
        while (n-- > 0) *d++ = *s++;
    }
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n) { return memmove(dst, src, n); }

int strcmp(const char *p, const char *q) {
    while (*p && *p == *q) {
        p++;
        q++;
    }
    return (uchar)*p - (uchar)*q;
}

int strncmp(const char *p, const char *q, size_t n) {
    while (n > 0 && *p && *p == *q) {
        n--;
        p++;
        q++;
    }
    if (n == 0) return 0;
    return (uchar)*p - (uchar)*q;
}

char *strcpy(char *dst, const char *src) {
    char *p = dst;
    while ((*p++ = *src++) != '\0');
    return dst;
}

char *strncpy(char *s, const char *t, size_t n) {
    char *os;
    os = s;
    while (n-- > 0 && (*s++ = *t++) != 0);
    while (n-- > 0) *s++ = 0;
    return os;
}

char *safestrcpy(char *s, const char *t, size_t n) {
    char *os;
    os = s;
    if (n <= 0) return os;
    while (--n > 0 && (*s++ = *t++) != 0);
    *s = 0;
    return os;
}

size_t strlen(const char *s) {
    size_t n;
    for (n = 0; s[n]; n++);
    return n;
}

size_t strnlen(const char *s, size_t len) {
    size_t cnt = 0;
    while (cnt < len && *s++ != '\0') cnt++;
    return cnt;
}

char *strchr(const char *s, int c) {
    while (*s != '\0') {
        if (*s == c) return (char *)s;
        s++;
    }
    return nullptr;
}

char *strfind(const char *s, int c) {
    while (*s != '\0') {
        if (*s == c) break;
        s++;
    }
    return (char *)s;
}

long strtol(const char *s, char **endptr, int base) {
    int neg = 0;
    long val = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '+')
        s++;
    else if (*s == '-') {
        s++;
        neg = 1;
    }
    if ((base == 0 || base == 16) && (s[0] == '0' && s[1] == 'x')) {
        s += 2;
        base = 16;
    } else if ((base == 0 || base == 8) && s[0] == '0') {
        s++;
        base = 8;
    } else if (base == 0 || base == 10)
        base = 10;
    while (1) {
        int dig;
        if (*s >= '0' && *s <= '9') {
            dig = *s - '0';
        } else if (*s >= 'a' && *s <= 'z') {
            dig = *s - 'a' + 10;
        } else if (*s >= 'A' && *s <= 'Z') {
            dig = *s - 'A' + 10;
        } else {
            break;
        }
        if (dig >= base) { break; }
        s++;
        val = (val * base) + dig;
    }
    if (endptr) *endptr = (char *)s;
    return (neg ? -val : val);
}