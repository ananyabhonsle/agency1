/* AGENCY-1 Kernel Headers
 * Central include file for all kernel modules.
 */

#ifndef AGENCY1_H
#define AGENCY1_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ----- Constants ----- */
#define SERIAL_TIMEOUT_MS   250
#define MAX_LINE_LEN        4096
#define MAX_STEPS_PER_PIPE  8

#define RAMFS_BASE          0x200000
#define RAMFS_SIZE          (256 * 1024)
#define ARENA_BASE          0x300000
#define ARENA_SIZE          (12 * 1024 * 1024)

#define BANNER "{\"v\":1,\"ok\":true,\"data\":{\"banner\":\"AGENCY-1\",\"version\":\"1.0\"}}\n"

/* ----- Serial driver (kernel/serial.c) ----- */
void serial_init(void);
void serial_putc(char c);
void serial_write(const char *s, unsigned len);
int  serial_readline(char *buf, unsigned cap, unsigned timeout_ms);

/* ----- GDT/IDT (kernel/gdt.c) ----- */
void gdt_init(void);
void idt_init(void);
void pic_remap(void);
void pit_init(void);
uint32_t uptime_ms(void);

extern volatile uint32_t uptime_ticks;

/* ----- Bump allocator (kernel/alloc.c) ----- */
void  arena_init(void);
void *arena_alloc(unsigned size);
uint32_t arena_used_bytes(void);

/* ----- JSON parser (kernel/json.c) ----- */
enum jval_type { J_NULL, J_BOOL, J_INT, J_STR, J_ARR, J_OBJ };

struct jval {
    enum jval_type type;
    union {
        int b;
        int i;
        const char *s;
        struct { struct jval *items; unsigned count; } arr;
        struct { char **keys; struct jval *vals; unsigned count; } obj;
    };
};

int json_parse(const char *line, struct jval *out);
const struct jval *json_get(const struct jval *obj, const char *key);
unsigned json_serialize(const struct jval *v, char *out, unsigned cap);

/* ----- Op registry (kernel/registry.c) ----- */
struct request {
    int      v;
    int      id;
    char     op[32];
    struct jval args;
};

struct resp {
    bool ok;
    char *data_json;
    const char *err_code;
    const char *err_meaning;
    const char *sug_op;
    const char *sug_args;
    uint32_t us_start;
    uint32_t us_end;
    unsigned bytes_out;
    unsigned tokens_saved;
};

struct op_entry {
    const char *name;
    const char *schema;
    int (*fn)(const struct jval *args, struct resp *out);
};

extern const struct op_entry OPS[];
const struct op_entry *registry_find(const char *name);

/* ----- Reply (kernel/reply.c) ----- */
void reply_send(const struct request *req, const struct resp *resp);
void reply_error(int id, const char *code, const char *meaning,
                 const char *sug_op, const char *sug_args);
void reply_unknown_op(const struct request *req);
unsigned tokens_saved_for(const char *op_name, unsigned bytes_out);

/* ----- Ramfs (kernel/ramfs.c) ----- */
enum node_type { N_DIR, N_FILE };

struct inode {
    uint32_t id;
    enum node_type type;
    char name[32];
    uint32_t parent_id;
    uint32_t offset;
    uint32_t bytes;
};

#define MAX_INODES 64

extern struct inode *inodes;
extern char *ramfs_content;
extern uint32_t inode_count;

void ramfs_init(void);
const struct inode *ramfs_lookup_child(const struct inode *parent, const char *name);
int ramfs_resolve(const char *path, const struct inode **out);
int path_resolve(const char *path, const struct inode **out);
uint32_t ramfs_used_bytes(void);

/* ----- Ops (kernel/ops.c) ----- */
int op_ping(const struct jval *args, struct resp *out);
int op_help(const struct jval *args, struct resp *out);
int op_sysinfo(const struct jval *args, struct resp *out);
int op_pwd(const struct jval *args, struct resp *out);
int op_cd(const struct jval *args, struct resp *out);
int op_ls(const struct jval *args, struct resp *out);
int op_cat(const struct jval *args, struct resp *out);
int op_grep(const struct jval *args, struct resp *out);
int op_stat(const struct jval *args, struct resp *out);
int op_write(const struct jval *args, struct resp *out);
int op_pipe(const struct jval *args, struct resp *out);

/* ----- Errors (kernel/errors.c) ----- */
#define E_NOENT       "E_NOENT"
#define E_NOTDIR      "E_NOTDIR"
#define E_ISDIR       "E_ISDIR"
#define E_ARG_MISSING "E_ARG_MISSING"
#define E_ARG_UNKNOWN "E_ARG_UNKNOWN"
#define E_ARG_TYPE    "E_ARG_TYPE"
#define E_UNKNOWN_OP  "E_UNKNOWN_OP"
#define E_TOO_LONG    "E_TOO_LONG"
#define E_BUSY        "E_BUSY"
#define E_PARSE       "E_PARSE"
#define E_FAULT       "E_FAULT"

/* ----- Working directory (kernel/main.c) ----- */
extern const struct inode *cwd;

#endif /* AGENCY1_H */
