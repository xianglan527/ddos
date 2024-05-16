#ifndef __DIRVER_UART_H__
#define __DIRVER_UART_H__

void uart_init();
void uart_puts(char *s);
int uart_putc(char ch);
#endif