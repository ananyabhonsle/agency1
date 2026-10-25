/* AGENCY-1 16550 UART driver
 * Polled, deterministic. No interrupts.
 * Port: 0x3F8, Config: 115200 baud, 8N1, FIFO enabled
 */

#include "agency1.h"

#define UART_DATA        0x3F8
#define UART_IER         0x3F9
#define UART_FCR         0x3FA
#define UART_LCR         0x3FB
#define UART_MCR         0x3FC
#define UART_LSR         0x3FD

#define LSR_DATA_READY   0x01
#define LSR_THR_EMPTY    0x20

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void serial_init(void) {
    outb(UART_IER, 0x00);
    outb(UART_LCR, 0x80);
    outb(UART_DATA, 0x01);
    outb(UART_IER, 0x00);
    outb(UART_LCR, 0x03);
    outb(UART_FCR, 0xC7);
    outb(UART_MCR, 0x0B);
}

void serial_putc(char c) {
    while ((inb(UART_LSR) & LSR_THR_EMPTY) == 0) { }
    outb(UART_DATA, (uint8_t)c);
}

void serial_write(const char *s, unsigned len) {
    for (unsigned i = 0; i < len; i++) {
        serial_putc(s[i]);
    }
}

int serial_readline(char *buf, unsigned cap, unsigned timeout_ms) {
    unsigned len = 0;
    uint32_t start = uptime_ms();

    while (len < cap - 1) {
        if (timeout_ms > 0 && len > 0 && uptime_ms() - start >= timeout_ms) {
            return 0;
        }
        if (inb(UART_LSR) & LSR_DATA_READY) {
            char c = (char)inb(UART_DATA);
            if (c == '\n') {
                buf[len] = '\0';
                return 1;
            }
            if (c != '\r') {
                buf[len++] = c;
            }
            start = uptime_ms();
        }
    }
    buf[cap - 1] = '\0';
    return 1;
}
