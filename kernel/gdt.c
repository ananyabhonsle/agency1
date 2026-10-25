/* AGENCY-1 GDT/IDT/PIC/PIT initialization
 * Standard OSDev recipe — boring substrate.
 */

#include "agency1.h"

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct gdt_entry gdt[3];
static struct gdt_ptr   gdtp;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void gdt_set_entry(int i, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t gran) {
    gdt[i].limit_low   = limit & 0xFFFF;
    gdt[i].base_low    = base & 0xFFFF;
    gdt[i].base_mid    = (base >> 16) & 0xFF;
    gdt[i].access      = access;
    gdt[i].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[i].base_high   = (base >> 24) & 0xFF;
}

void gdt_init(void) {
    gdt_set_entry(0, 0, 0, 0, 0);
    gdt_set_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    gdt_set_entry(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    gdtp.limit = sizeof(gdt) - 1;
    gdtp.base  = (uint32_t)&gdt;

    __asm__ __volatile__("lgdt %0" : : "m"(gdtp));
    __asm__ __volatile__(
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        : : : "eax"
    );
}

struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr   idtp;

static void idt_set_entry(int i, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[i].base_low  = base & 0xFFFF;
    idt[i].selector  = sel;
    idt[i].zero      = 0;
    idt[i].flags     = flags;
    idt[i].base_high = (base >> 16) & 0xFFFF;
}

__attribute__((naked)) static void timer_isr(void) {
    __asm__ __volatile__(
        "pushal\n"
        "incl uptime_ticks\n"
        "movb $0x20, %al\n"
        "outb %al, $0x20\n"
        "popal\n"
        "iretl\n"
    );
}

void idt_init(void) {
    for (int i = 0; i < 256; i++) {
        idt_set_entry(i, 0, 0x08, 0);
    }
    idt_set_entry(0x20, (uint32_t)timer_isr, 0x08, 0x8E);
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint32_t)&idt;
    __asm__ __volatile__("lidt %0" : : "m"(idtp));
}

void pic_remap(void) {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
}

/* uptime_ticks defined in main.c */

void pit_init(void) {
    {
        uint8_t lsr;
        do { __asm__ __volatile__("inb %1, %0" : "=a"(lsr) : "Nd"(0x3FD)); } while ((lsr & 0x20) == 0);
        __asm__ __volatile__("outb %0, %1" : : "a"((uint8_t)'a'), "Nd"(0x3F8));
    }
    uint16_t divisor = 11932;
    outb(0x43, 0x36);
    {
        uint8_t lsr;
        do { __asm__ __volatile__("inb %1, %0" : "=a"(lsr) : "Nd"(0x3FD)); } while ((lsr & 0x20) == 0);
        __asm__ __volatile__("outb %0, %1" : : "a"((uint8_t)'b'), "Nd"(0x3F8));
    }
    outb(0x40, divisor & 0xFF);
    {
        uint8_t lsr;
        do { __asm__ __volatile__("inb %1, %0" : "=a"(lsr) : "Nd"(0x3FD)); } while ((lsr & 0x20) == 0);
        __asm__ __volatile__("outb %0, %1" : : "a"((uint8_t)'c'), "Nd"(0x3F8));
    }
    outb(0x40, (divisor >> 8) & 0xFF);
    {
        uint8_t lsr;
        do { __asm__ __volatile__("inb %1, %0" : "=a"(lsr) : "Nd"(0x3FD)); } while ((lsr & 0x20) == 0);
        __asm__ __volatile__("outb %0, %1" : : "a"((uint8_t)'d'), "Nd"(0x3F8));
    }
    outb(0x21, 0xFE); /* unmask IRQ0 */
    __asm__ __volatile__("sti");
}

uint32_t uptime_ms(void) {
    return uptime_ticks * 10;
}
