/* AGENCY-1 kernel main entry point. */

#include "agency1.h"

volatile uint32_t uptime_ticks = 0;
const struct inode *cwd;

static char request_line[MAX_LINE_LEN];

static inline void debug_putc(char c) {
    uint8_t lsr;
    do {
        __asm__ __volatile__("inb %1, %0" : "=a"(lsr) : "Nd"(0x3FD));
    } while ((lsr & 0x20) == 0);
    __asm__ __volatile__("outb %0, %1" : : "a"((uint8_t)c), "Nd"(0x3F8));
}

static void debug_str(const char *s) {
    while (*s) debug_putc(*s++);
}

__attribute__((section(".text._start"), noinline, used))
void _start(void) {
    /* Very first thing: print "X" to prove we entered _start */
    debug_putc(0x58);  /* 'X' */

    /* Set up a clean stack (in case boot sector's stack is wonky) */
    __asm__ __volatile__("mov $0x90000, %esp");
    __asm__ __volatile__("mov $0x90000, %ebp");

    debug_putc(0x59);  /* 'Y' - stack set up */

    gdt_init();
    debug_putc(0x31);  /* '1' */
    idt_init();
    debug_putc(0x32);  /* '2' */
    pic_remap();
    debug_putc(0x33);  /* '3' */
    pit_init();
    debug_putc(0x34);  /* '4' */
    arena_init();
    debug_putc(0x35);  /* '5' */
    serial_init();
    debug_putc(0x36);  /* '6' */
    ramfs_init();
    debug_putc(0x37);  /* '7' */

    const struct inode *root;
    if (ramfs_resolve("/", &root) == 0) {
        cwd = root;
    }
    debug_putc(0x38);  /* '8' */

    serial_write(BANNER, sizeof(BANNER) - 1);
    debug_putc(0x41);  /* 'A' */

    for (;;) {
        if (!serial_readline(request_line, sizeof request_line, SERIAL_TIMEOUT_MS)) {
            reply_error(0, E_BUSY, "no complete line within 250 ms", NULL, NULL);
            continue;
        }

        unsigned len = 0;
        while (request_line[len] && len < MAX_LINE_LEN) len++;
        if (len >= MAX_LINE_LEN - 1) {
            reply_error(0, E_TOO_LONG, "request line exceeded 4096 bytes", NULL, NULL);
            continue;
        }

        struct jval root_val;
        if (!json_parse(request_line, &root_val)) {
            reply_error(0, E_PARSE, "request is not valid JSON", NULL, NULL);
            continue;
        }

        if (root_val.type != J_OBJ) {
            reply_error(0, E_PARSE, "request is not a JSON object", NULL, NULL);
            continue;
        }

        const struct jval *v_field = json_get(&root_val, "v");
        const struct jval *id_field = json_get(&root_val, "id");
        const struct jval *op_field = json_get(&root_val, "op");
        const struct jval *args_field = json_get(&root_val, "args");

        if (!v_field || !id_field || !op_field || !args_field) {
            reply_error(0, E_PARSE, "missing required field", NULL, NULL);
            continue;
        }

        struct request req;
        req.v = v_field->i;
        req.id = id_field->i;
        unsigned i = 0;
        const char *src = op_field->s;
        while (src[i] && i < 31) {
            req.op[i] = src[i];
            i++;
        }
        req.op[i] = '\0';
        req.args = *args_field;

        const struct op_entry *op = registry_find(req.op);
        if (!op) {
            reply_unknown_op(&req);
            continue;
        }

        struct resp resp;
        resp.us_start = uptime_ms();
        resp.ok = false;
        resp.data_json = NULL;
        resp.err_code = E_FAULT;
        resp.err_meaning = "internal error";
        resp.sug_op = NULL;
        resp.sug_args = NULL;

        op->fn(&req.args, &resp);
        resp.us_end = uptime_ms();
        reply_send(&req, &resp);
    }
}
