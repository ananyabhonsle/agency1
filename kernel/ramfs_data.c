/* AGENCY-1 Runtime Ramfs Builder
 * Builds the filesystem in memory at 0x200000 instead of embedding a 256 KiB blob.
 * This keeps kernel.bin small (~30 KiB) so the boot sector can load it.
 */

#include "agency1.h"

#define RAMFS_PTR ((unsigned char *)0x200000)

/* File contents as C strings */
static const char *file_contents[] = {
    "AGENCY-1 v1.0\n",                                                    /* /bin/agency1.txt */
    "AGENCY-1 - Agent-Native Operating System\n\nA bare-metal kernel whose syscalls are JSON tool calls over serial.\n",  /* /docs/readme.txt */
    "AGENCY-1 ABI Reference\n\nRequest: {\"v\":1,\"id\":int,\"op\":\"string\",\"args\":{...}}\n",  /* /docs/abi.txt */
    "[ 0.000] AGENCY-1 v1.0 booting\n[ 0.412] kernel panic in ahci_poll: timeout\n[ 0.907] kernel panic resumed: retrying\n[ 1.334] kernel panic cleared after reset\n",  /* /logs/boot.log */
    "[ 1.001] RX: ping\n[ 1.002] TX: pong\n",                            /* /logs/serial.log */
    "// AGENCY-1 kernel main - dispatch loop, _start entry, banner\n",   /* /src/main.c */
    "// AGENCY-1 16550 UART driver - polled, no interrupts\n",           /* /src/serial.c */
    "// AGENCY-1 in-memory filesystem - inode table + content blocks\n", /* /src/ramfs.c */
};

struct ramfs_spec {
    unsigned id;
    unsigned type;     /* 0=dir, 1=file */
    const char *name;
    unsigned parent;
    unsigned content_idx;  /* for files: index into file_contents */
};

static const struct ramfs_spec RAMFS_SPEC[] = {
    /* root */
    {0, 0, "/", 0, 0},
    /* directories */
    {1, 0, "bin", 0, 0},
    {2, 0, "docs", 0, 0},
    {3, 0, "logs", 0, 0},
    {4, 0, "src", 0, 0},
    /* files */
    {5, 1, "agency1.txt", 1, 0},
    {6, 1, "readme.txt", 2, 1},
    {7, 1, "abi.txt", 2, 2},
    {8, 1, "boot.log", 3, 3},
    {9, 1, "serial.log", 3, 4},
    {10, 1, "main.c", 4, 5},
    {11, 1, "serial.c", 4, 6},
    {12, 1, "ramfs.c", 4, 7},
};

static unsigned my_strlen(const char *s) {
    unsigned n = 0;
    while (s[n]) n++;
    return n;
}

static void my_memcpy(unsigned char *dst, const unsigned char *src, unsigned n) {
    for (unsigned i = 0; i < n; i++) dst[i] = src[i];
}

static void my_memzero(unsigned char *dst, unsigned n) {
    for (unsigned i = 0; i < n; i++) dst[i] = 0;
}

void ramfs_load(void) {
    /* Build inode table at start of RAMFS_PTR */
    struct inode *ino_table = (struct inode *)RAMFS_PTR;
    unsigned char *content_area = RAMFS_PTR + 64 * 48;  /* after inode table */
    unsigned content_offset = 0;

    /* Zero the inode table */
    my_memzero((unsigned char *)ino_table, 64 * 48);

    /* Build inodes */
    unsigned num_specs = sizeof(RAMFS_SPEC) / sizeof(RAMFS_SPEC[0]);
    for (unsigned i = 0; i < num_specs; i++) {
        const struct ramfs_spec *s = &RAMFS_SPEC[i];
        struct inode *ino = &ino_table[i];

        ino->id = s->id;
        ino->type = (enum node_type)s->type;

        /* Copy name */
        unsigned name_len = my_strlen(s->name);
        for (unsigned j = 0; j < 31 && j < name_len; j++) {
            ino->name[j] = s->name[j];
        }
        ino->name[31] = 0;

        ino->parent_id = s->parent;

        if (s->type == 1) {
            /* File: copy content */
            const char *content = file_contents[s->content_idx];
            unsigned content_len = my_strlen(content);

            /* Align content to 512-byte boundary */
            unsigned aligned_offset = (content_offset + 511) & ~511u;

            my_memcpy(content_area + aligned_offset,
                      (const unsigned char *)content, content_len);

            ino->offset = aligned_offset;
            ino->bytes = content_len;

            content_offset = aligned_offset + content_len;
        } else {
            ino->offset = 0;
            ino->bytes = 0;
        }
    }
}
