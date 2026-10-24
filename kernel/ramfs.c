/* AGENCY-1 In-Memory Filesystem (ramfs)
 * Layout: inode table (MAX_INODES * sizeof(inode)) + 512-byte content blocks
 * All in 256 KiB at RAMFS_BASE (0x200000).
 */

#include "agency1.h"

extern void ramfs_load(void);

struct inode *inodes = (struct inode *)RAMFS_BASE;
char *ramfs_content = (char *)(RAMFS_BASE + MAX_INODES * sizeof(struct inode));
uint32_t inode_count = 0;

static bool str_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return false;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

void ramfs_init(void) {
    ramfs_load();
    /* Count inodes by scanning until we hit an empty one */
    inode_count = 0;
    for (uint32_t i = 0; i < MAX_INODES; i++) {
        if (inodes[i].id == 0xFFFFFFFF || inodes[i].name[0] == '\0') {
            break;
        }
        inode_count++;
    }
}

const struct inode *ramfs_lookup_child(const struct inode *parent, const char *name) {
    uint32_t parent_id = parent ? parent->id : 0;
    for (uint32_t i = 0; i < inode_count; i++) {
        if (inodes[i].parent_id == parent_id && str_eq(inodes[i].name, name)) {
            return &inodes[i];
        }
    }
    return NULL;
}

int ramfs_resolve(const char *path, const struct inode **out) {
    if (path[0] != '/' && path[0] != '\0') {
        /* relative path - use path_resolve instead */
        return path_resolve(path, out);
    }
    if (path[0] == '/' && path[1] == '\0') {
        *out = &inodes[0];  /* root */
        return 0;
    }

    const struct inode *cur = &inodes[0];  /* root */
    const char *p = path + 1;  /* skip leading / */
    while (*p) {
        char name[32];
        int i = 0;
        while (*p && *p != '/' && i < 31) {
            name[i++] = *p++;
        }
        name[i] = '\0';
        if (*p == '/') p++;
        if (i == 0) continue;

        const struct inode *next = ramfs_lookup_child(cur, name);
        if (!next) return -1;
        cur = next;
    }
    *out = cur;
    return 0;
}

int path_resolve(const char *path, const struct inode **out) {
    if (path[0] == '/') {
        return ramfs_resolve(path, out);
    }
    if (path[0] == '\0') {
        *out = cwd;
        return 0;
    }
    /* relative path - resolve against cwd */
    /* Build absolute path from cwd */
    /* For simplicity, walk path components relative to cwd */
    const struct inode *cur = cwd;
    const char *p = path;
    while (*p) {
        char name[32];
        int i = 0;
        while (*p && *p != '/' && i < 31) {
            name[i++] = *p++;
        }
        name[i] = '\0';
        if (*p == '/') p++;
        if (i == 0) continue;

        if (str_eq(name, ".")) continue;
        if (str_eq(name, "..")) {
            if (cur->parent_id < inode_count) {
                cur = &inodes[cur->parent_id];
            }
            continue;
        }

        const struct inode *next = ramfs_lookup_child(cur, name);
        if (!next) return -1;
        cur = next;
    }
    *out = cur;
    return 0;
}
