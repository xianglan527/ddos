#include "config.h"
#include "types.h"
#include "uart.h"
#include "memlayout.h"

#define RHR 0  // Receive Holding Register (read mode)
#define THR 0  // Transmit Holding Register (write mode)
#define DLL 0  // LSB of Divisor Latch (write mode)
#define IER 1  // Interrupt Enable Register (write mode)
#define DLM 1  // MSB of Divisor Latch (write mode)
#define FCR 2  // FIFO Control Register (write mode)
#define ISR 2  // Interrupt Status Register (read mode)
#define LCR 3  // Line Control Register
#define MCR 4  // Modem Control Register
#define LSR 5  // Line Status Register
#define MSR 6  // Modem Status Register
#define SPR 7  // ScratchPad Register

#define UART_REG(reg) ((volatile uint8_t *)(UART0 + reg))

#define LSR_RX_READY (1 << 0)
#define LSR_TX_IDLE (1 << 5)

#define IER_RX_ENABLE (1 << 0)
#define IER_TX_ENABLE (1 << 1)

#define FCR_FIFO_ENABLE (1 << 0)
#define FCR_FIFO_CLEAR (3 << 1)  // clear the content of the two FIFOs

#define uart_read_reg(reg) (*(UART_REG(reg)))
#define uart_write_reg(reg, v) (*(UART_REG(reg)) = (v))

extern volatile bool is_panic;

void uart_init() {
    /* disable interrupts. */
    uart_write_reg(IER, 0x00);
    uint8_t lcr = uart_read_reg(LCR);
    uart_write_reg(LCR, lcr | (1 << 7));
    uart_write_reg(DLL, 0x03);
    uart_write_reg(DLM, 0x00);

    /*
     * Continue setting the asynchronous data communication format.
     * - number of the word length: 8 bits
     * - number of stop bits：1 bit when word length is 8 bits
     * - no parity
     * - no break control
     * - disabled baud latch
     */
    lcr = 0;
    uart_write_reg(LCR, lcr | (3 << 0));
    // reset and enable FIFOs.
    uart_write_reg(FCR, FCR_FIFO_ENABLE | FCR_FIFO_CLEAR);

    // // enable transmit and receive interrupts.
    // uart_write_reg(IER, IER_TX_ENABLE | IER_RX_ENABLE);

    uint8_t ier = uart_read_reg(IER);
    uart_write_reg(IER, ier | IER_RX_ENABLE);
}

int uart_putc(char ch) {
    // if(is_panic){
    //     while(1);
    // }
    while ((uart_read_reg(LSR) & LSR_TX_IDLE) == 0);
    return uart_write_reg(THR, ch);
}

void uart_puts(char *s) {
    while (*s) { uart_putc(*s++); }
}

int uart_getc(void){
    if(uart_read_reg(LSR) & 0x01)
        return uart_read_reg(RHR);
    else 
        return -1;
}
