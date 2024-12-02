
#include "string.h"
#include "printf.h"
#include "user.h"
#include "config.h"
#include "assert.h"

static void vprintfmt(void (*putch)(int, void *), void *putdat, const char *fmt, va_list ap);
int vsnprintf(char *str, size_t size, const char *fmt, va_list ap);

static void cputch(int c, int *cnt) {
    putc(2, c);
    (*cnt)++;
}

int vcprintf(const char *fmt, va_list ap) {
    int cnt = 0;
    vprintfmt((void (*)(int, void *))cputch, &cnt, fmt, ap);
    return cnt;
}

// int printf(const char *fmt, ...) {
//     va_list ap;
//     int cnt;
//     va_start(ap, fmt);
//     cnt = vcprintf(fmt, ap);
//     va_end(ap);
//     return cnt;
// }

static void printnum(void (*putch)(int, void *), void *putdat, uint64_t num, unsigned base, int width,
                     int padc) {
    uint64_t result = num / base;
    unsigned mod = num % base;
    if (num >= base)
        printnum(putch, putdat, result, base, width - 1, padc);
    else {
        while (--width > 0) putch(padc, putdat);
    }
    putch("0123456789abcdef"[mod], putdat);
}

static uint64_t getuint(va_list *ap, int lflag) {
    if (lflag >= 2)
        return va_arg(*ap, uint64_t);
    else if (lflag)
        return va_arg(*ap, unsigned long);
    else
        return va_arg(*ap, uint);
}

static int64_t getint(va_list *ap, int lflag) {
    if (lflag >= 2)
        return va_arg(*ap, int64_t);
    else if (lflag)
        return va_arg(*ap, long);
    else
        return va_arg(*ap, int);
}

static void printfmt(void (*putch)(int, void *), void *putdat, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintfmt(putch, putdat, fmt, ap);
    va_end(ap);
}

static void vprintfmt(void (*putch)(int, void *), void *putdat, const char *fmt, va_list ap) {
    const char *p;
    int ch, err;
    uint64_t num;
    int base, width, precision, lflag, altflag;
    while (1) {
        while ((ch = *(uint8_t *)fmt++) != '%') {
            if (ch == '\0') return;
            putch(ch, putdat);
        }
        char padc = ' ';
        width = precision = -1;
        lflag = altflag = 0;

    reswitch:
        switch (ch = *(uint8_t *)fmt++) {
            case '-': padc = '-'; goto reswitch;
            case '0': padc = '0'; goto reswitch;
            case '1' ... '9':
                for (precision = 0;; ++fmt) {
                    precision = precision * 10 + ch - '0';
                    ch = *fmt;
                    if (ch < '0' || ch > '9') break;
                }
                goto process_precision;
            case '*': precision = va_arg(ap, int); goto process_precision;
            case '.': width = va_arg(ap, int); goto process_precision;
            case '#':
                altflag = 1;
                goto reswitch;
            process_precision:
                if (width < 0) {
                    width = precision;
                    precision = -1;
                }
                goto reswitch;
            case 'l': lflag++; goto reswitch;
            case 'c': putch(va_arg(ap, int), putdat); break;
            case 's':
                if ((p = va_arg(ap, char *)) == nullptr) p = "(nullptr)";
                if (width > 0 && padc != '-') {
                    for (width -= strnlen(p, precision); width > 0; width--) putch(padc, putdat);
                }
                for (; (ch = *p++) != '\0' && (precision < 0 || --precision >= 0); width--) {
                    if (altflag && (ch < ' ' || ch > '~'))
                        putch('?', putdat);
                    else
                        putch(ch, putdat);
                }
                for (; width > 0; width--) putch(' ', putdat);
                break;
            case 'd':
                num = getint(&ap, lflag);
                if ((int64_t)num < 0) {
                    putch('-', putdat);
                    num = -(int64_t)num;
                }
                base = 10;
                goto number;
            case 'u':
                num = getuint(&ap, lflag);
                base = 10;
                goto number;

            // (unsigned) octal
            case 'o':
                num = getuint(&ap, lflag);
                base = 8;
                goto number;
            case 'p':
                putch('0', putdat);
                putch('x', putdat);
                num = (unsigned long long)(uintptr_t)va_arg(ap, void *);
                base = 16;
                goto number;

            // (unsigned) hexadecimal
            case 'x':
                num = getuint(&ap, lflag);
                base = 16;
            number:
                printnum(putch, putdat, num, base, width, padc);
                break;
            case '%': putch(ch, putdat); break;
            default:
                putch('%', putdat);
                for (fmt--; fmt[-1] != '%'; fmt--) /* do nothing */
                    ;
                break;
        }
    }
}

static void sprintputch(int ch, Sprintbuf *b) {
    b->cnt++;
    if (b->buf < b->ebuf) *b->buf++ = ch;
    else panic("too long string");
}

int snprintf(char *str, size_t size, const char *fmt, ...) {
    va_list ap;
    int cnt;
    va_start(ap, fmt);
    cnt = vsnprintf(str, size, fmt, ap);
    va_end(ap);
    return cnt;
}

int vsnprintf(char *str, size_t size, const char *fmt, va_list ap) {
    Sprintbuf b = {str, str + size - 1, 0};
    if (str == nullptr || b.buf > b.ebuf) return -1;
    vprintfmt((void (*)(int, void *))sprintputch, &b, fmt, ap);
    *b.buf = '\0';
    return b.cnt;
}

int printf(const char *fmt, ...) {
    char str[MAXPATH];
    va_list ap;
    int cnt;
    va_start(ap, fmt);
    cnt = vsnprintf(str, MAXPATH, fmt, ap);
    va_end(ap);
    if(cnt != -1)
        puts(1, str);
    return cnt;
}

int fprintf(int fd, const char *fmt, ...) {
    char str[MAXPATH];
    va_list ap;
    int cnt;
    va_start(ap, fmt);
    cnt = vsnprintf(str, MAXPATH, fmt, ap);
    va_end(ap);
    if (cnt != -1) puts(fd, str);
    return cnt;
}
