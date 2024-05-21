#include "console.h"
#include "uart.h"
#include "config.h"
#include "stdio.h"
#include "string.h"
#include "assert.h"

#define BACKSPACE 0x100
#define C(x) ((x) - '@')  // Control-x

static struct {
    uint8_t buf[CONSOLE_BUF_SIZE];
    size_t rpos;
    size_t wpos;
    size_t epos;
} cons;

static char read_buf[CONSOLE_BUF_SIZE];

static void cons_putc(int c){
    if (c == BACKSPACE){
        uart_putc('\b');
        uart_putc(' ');
        uart_putc('\b');
    }
    else
        uart_putc(c);
}

static void cons_intr(int (*proc)(void)) {
    int c;
    while ((c = (*proc)()) != -1) { 
        if (c != 0){
            switch(c){
                case C('P'):
                //TODO :procdump
                break;
                case C('U'):
                    while(cons.epos != cons.wpos &&
                        cons.buf[(cons.epos - 1) % CONSOLE_BUF_SIZE] != '\n'){
                        cons.epos--;
                        cons_putc(BACKSPACE);
                    }
                    break;
                case C('H'):
                case '\x7f':
                    if(cons.epos != cons.wpos){
                        cons.epos--;
                        cons_putc(BACKSPACE);
                    }
                    break;
                default:
                    if(c != 0 && cons.epos - cons.rpos < CONSOLE_BUF_SIZE){
                        c = (c == '\r' || c == C('D')) ? '\n' : c;
                        cons_putc(c);
                        cons.buf[cons.epos++ % CONSOLE_BUF_SIZE] = c;
                        if(c == '\n' || cons.epos == cons.rpos + CONSOLE_BUF_SIZE){
                            cons.wpos = cons.epos;
                        }
                    }
                break;
            }
        }
    }
}

void uart_intr(void) { 
    cons_intr(uart_getc);
}

int cons_getc(void) { 
    int c;
    // uart_intr();
    if(cons.rpos != cons.wpos){
        c = cons.buf[cons.rpos++ % CONSOLE_BUF_SIZE];
        return c;
    }
    return 0; 
}

char *readline(const char *prompt){
    if(prompt != nullptr)
        cprintf("%s", prompt);
    memset(read_buf, 0, CONSOLE_BUF_SIZE);
    int i = 0, c;
    while(1){
        c = getchar();
        if( c == '\n'){
            read_buf[i] = '\0';
            return read_buf;
        }
        else if(i < CONSOLE_BUF_SIZE - 1)
            read_buf[i++] = c;
        else{
             warn("out of console buffer size");
             return nullptr;
        }
    }
}