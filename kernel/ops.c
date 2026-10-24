/* AGENCY-1 Op Handlers
 * Each op function takes args + resp, returns 0 on success.
 * The dispatcher calls reply_send() after this returns.
 */

#include "agency1.h"

/* cwd defined in main.c */


static unsigned str_copy(char *dst, const char *src) {
    unsigned n = 0;
    while (src[n]) { dst[n] = src[n]; n++; }
    dst[n] = '\0';
    return n;
}

static int int_to_str(int val, char *buf) {
    int i = 0;
    if (val < 0) { buf[i++] = '-'; val = -val; }
    char tmp[12];
    int j = 0;
    if (val == 0) tmp[j++] = '0';
    while (val > 0) { tmp[j++] = '0' + (val % 10); val /= 10; }
    while (j > 0) buf[i++] = tmp[--j];
    return i;
}

/* ----- ping ----- */
int op_ping(const struct jval *args, struct resp *out) {
    (void)args;
    out->ok = true;
    out->data_json = "{\"pong\":true}";
    return 0;
}

/* ----- help ----- */
int op_help(const struct jval *args, struct resp *out) {
    const struct jval *op_arg = json_get(args, "op");
    if (op_arg && op_arg->type == J_STR) {
        const struct op_entry *e = registry_find(op_arg->s);
        if (!e) {
            out->ok = false;
            out->err_code = E_UNKNOWN_OP;
            out->err_meaning = "op name not in registry";
            out->sug_op = "help";
            out->sug_args = "{}";
            return 0;
        }
        char buf[256];
        unsigned n = 0;
        n += str_copy(buf + n, "{\"name\":\"");
        n += str_copy(buf + n, e->name);
        n += str_copy(buf + n, "\",\"schema\":\"");
        n += str_copy(buf + n, e->schema);
        n += str_copy(buf + n, "\"}");
        buf[n] = '\0';
        out->ok = true;
        out->data_json = arena_alloc(n + 1);
        str_copy(out->data_json, buf);
        return 0;
    }
    /* List all ops */
    char buf[2048];
    unsigned n = 0;
    n += str_copy(buf + n, "{\"ops\":[");
    bool first = true;
    for (unsigned i = 0; OPS[i].name; i++) {
        if (!first) buf[n++] = ',';
        first = false;
        n += str_copy(buf + n, "{\"name\":\"");
        n += str_copy(buf + n, OPS[i].name);
        n += str_copy(buf + n, "\",\"schema\":\"");
        n += str_copy(buf + n, OPS[i].schema);
        n += str_copy(buf + n, "\"}");
    }
    buf[n++] = ']';
    buf[n++] = '}';
    buf[n] = '\0';
    out->ok = true;
    out->data_json = arena_alloc(n + 1);
    str_copy(out->data_json, buf);
    return 0;
}

/* ----- sys.info ----- */
int op_sysinfo(const struct jval *args, struct resp *out) {
    (void)args;
    char buf[1024];
    unsigned n = 0;
    n += str_copy(buf + n, "{\"version\":\"1.0\",\"uptime_ms\":");
    n += int_to_str((int)uptime_ms(), buf + n);
    n += str_copy(buf + n, ",\"mem\":{\"ramfs_used\":");
    n += int_to_str(0, buf + n);  /* TODO: track */
    n += str_copy(buf + n, ",\"arena_used\":");
    n += int_to_str(0, buf + n);  /* TODO: track */
    n += str_copy(buf + n, "},\"cwd\":\"");
    if (cwd) {
        /* Build cwd path - simplified: just print root or /logs etc. */
        n += str_copy(buf + n, "/");
        n += str_copy(buf + n, cwd->name);
    } else {
        n += str_copy(buf + n, "/");
    }
    n += str_copy(buf + n, "\",\"ops\":[");
    bool first = true;
    for (unsigned i = 0; OPS[i].name; i++) {
        if (!first) buf[n++] = ',';
        first = false;
        buf[n++] = '"';
        n += str_copy(buf + n, OPS[i].name);
        buf[n++] = '"';
    }
    n += str_copy(buf + n, "]}");
    buf[n] = '\0';
    out->ok = true;
    out->data_json = arena_alloc(n + 1);
    str_copy(out->data_json, buf);
    return 0;
}

/* ----- pwd ----- */
int op_pwd(const struct jval *args, struct resp *out) {
    (void)args;
    char buf[64];
    unsigned n = 0;
    n += str_copy(buf + n, "{\"cwd\":\"");
    if (cwd) {
        n += str_copy(buf + n, "/");
        n += str_copy(buf + n, cwd->name);
    } else {
        n += str_copy(buf + n, "/");
    }
    n += str_copy(buf + n, "\"}");
    buf[n] = '\0';
    out->ok = true;
    out->data_json = arena_alloc(n + 1);
    str_copy(out->data_json, buf);
    return 0;
}

/* ----- cd ----- */
int op_cd(const struct jval *args, struct resp *out) {
    const struct jval *path_arg = json_get(args, "path");
    if (!path_arg || path_arg->type != J_STR) {
        out->ok = false;
        out->err_code = E_ARG_MISSING;
        out->err_meaning = "required argument path absent";
        out->sug_op = "help";
        out->sug_args = "{\"op\":\"cd\"}";
        return 0;
    }
    const struct inode *target;
    if (path_resolve(path_arg->s, &target) != 0) {
        out->ok = false;
        out->err_code = E_NOENT;
        out->err_meaning = "named path does not exist";
        out->sug_op = "ls";
        out->sug_args = "{\"path\":\"/\"}";
        return 0;
    }
    if (target->type != N_DIR) {
        out->ok = false;
        out->err_code = E_NOTDIR;
        out->err_meaning = "path is not a directory";
        out->sug_op = "stat";
        out->sug_args = "{}";
        return 0;
    }
    cwd = target;
    char buf[64];
    unsigned n = 0;
    n += str_copy(buf + n, "{\"cwd\":\"/");
    n += str_copy(buf + n, target->name);
    n += str_copy(buf + n, "\"}");
    buf[n] = '\0';
    out->ok = true;
    out->data_json = arena_alloc(n + 1);
    str_copy(out->data_json, buf);
    return 0;
}

/* ----- ls ----- */
int op_ls(const struct jval *args, struct resp *out) {
    const struct jval *path_arg = json_get(args, "path");
    const struct inode *dir;
    if (path_arg && path_arg->type == J_STR) {
        if (path_resolve(path_arg->s, &dir) != 0) {
            out->ok = false;
            out->err_code = E_NOENT;
            out->err_meaning = "named path does not exist";
            out->sug_op = "ls";
            out->sug_args = "{\"path\":\"/\"}";
            return 0;
        }
    } else {
        dir = cwd;
    }
    if (dir->type != N_DIR) {
        out->ok = false;
        out->err_code = E_NOTDIR;
        out->err_meaning = "path is not a directory";
        out->sug_op = "cat";
        out->sug_args = "{}";
        return 0;
    }

    char buf[2048];
    unsigned n = 0;
    buf[n++] = '[';
    bool first = true;
    /* Directories first, then files (alphabetical within each) */
    for (int pass = 0; pass < 2; pass++) {
        enum node_type want = pass == 0 ? N_DIR : N_FILE;
        for (uint32_t i = 1; i < inode_count; i++) {
            if (inodes[i].parent_id != dir->id) continue;
            if (inodes[i].type != want) continue;
            if (!first) buf[n++] = ',';
            first = false;
            n += str_copy(buf + n, "{\"name\":\"");
            n += str_copy(buf + n, inodes[i].name);
            n += str_copy(buf + n, "\",\"type\":\"");
            n += str_copy(buf + n, inodes[i].type == N_DIR ? "dir" : "file");
            n += str_copy(buf + n, "\",\"bytes\":");
            n += int_to_str((int)inodes[i].bytes, buf + n);
            buf[n++] = '}';
        }
    }
    buf[n++] = ']';
    buf[n] = '\0';
    out->ok = true;
    out->data_json = arena_alloc(n + 1);
    str_copy(out->data_json, buf);
    return 0;
}

/* ----- cat ----- */
int op_cat(const struct jval *args, struct resp *out) {
    const struct jval *path_arg = json_get(args, "path");
    if (!path_arg || path_arg->type != J_STR) {
        out->ok = false;
        out->err_code = E_ARG_MISSING;
        out->err_meaning = "required argument path absent";
        out->sug_op = "help";
        out->sug_args = "{\"op\":\"cat\"}";
        return 0;
    }
    const struct inode *file;
    if (path_resolve(path_arg->s, &file) != 0) {
        out->ok = false;
        out->err_code = E_NOENT;
        out->err_meaning = "named path does not exist";
        out->sug_op = "ls";
        out->sug_args = "{\"path\":\"/\"}";
        return 0;
    }
    if (file->type != N_FILE) {
        out->ok = false;
        out->err_code = E_ISDIR;
        out->err_meaning = "path is a directory not a file";
        out->sug_op = "ls";
        out->sug_args = "{}";
        return 0;
    }

    char buf[8192];
    unsigned n = 0;
    n += str_copy(buf + n, "{\"path\":\"");
    n += str_copy(buf + n, path_arg->s);
    n += str_copy(buf + n, "\",\"bytes\":");
    n += int_to_str((int)file->bytes, buf + n);
    n += str_copy(buf + n, ",\"text\":\"");
    /* Copy file content with basic escaping */
    const char *content = ramfs_content + file->offset;
    for (uint32_t i = 0; i < file->bytes && n < 7800; i++) {
        char c = content[i];
        if (c == '"' || c == '\\') buf[n++] = '\\';
        if (c == '\n') { buf[n++] = '\\'; buf[n++] = 'n'; }
        else buf[n++] = c;
    }
    n += str_copy(buf + n, "\",\"truncated\":false}");
    buf[n] = '\0';
    out->ok = true;
    out->data_json = arena_alloc(n + 1);
    str_copy(out->data_json, buf);
    return 0;
}

/* ----- grep ----- */
int op_grep(const struct jval *args, struct resp *out) {
    const struct jval *pat_arg = json_get(args, "pattern");
    if (!pat_arg || pat_arg->type != J_STR) {
        out->ok = false;
        out->err_code = E_ARG_MISSING;
        out->err_meaning = "required argument pattern absent";
        out->sug_op = "help";
        out->sug_args = "{\"op\":\"grep\"}";
        return 0;
    }
    const char *pattern = pat_arg->s;
    const struct jval *path_arg = json_get(args, "path");
    const struct inode *start_dir;
    if (path_arg && path_arg->type == J_STR) {
        if (path_resolve(path_arg->s, &start_dir) != 0) {
            out->ok = false;
            out->err_code = E_NOENT;
            out->err_meaning = "named path does not exist";
            out->sug_op = "ls";
            out->sug_args = "{\"path\":\"/\"}";
            return 0;
        }
    } else {
        start_dir = cwd;
    }

    char buf[4096];
    unsigned n = 0;
    n += str_copy(buf + n, "{\"matches\":[");
    unsigned match_count = 0;
    unsigned files_searched = 0;

    /* Search recursively (depth 4) */
    /* For simplicity, search all files */
    for (uint32_t i = 0; i < inode_count; i++) {
        if (inodes[i].type != N_FILE) continue;
        files_searched++;
        const char *content = ramfs_content + inodes[i].offset;
        /* Find matches */
        unsigned line = 1, col = 1;
        for (uint32_t j = 0; j < inodes[i].bytes; j++) {
            if (content[j] == '\n') { line++; col = 1; continue; }
            /* Check for pattern match at this position */
            bool match = true;
            for (unsigned k = 0; pattern[k]; k++) {
                if (j + k >= inodes[i].bytes || content[j + k] != pattern[k]) {
                    match = false;
                    break;
                }
            }
            if (match && match_count < 20) {
                if (match_count > 0) buf[n++] = ',';
                n += str_copy(buf + n, "{\"file\":\"/");
                /* Build file path - simplified */
                n += str_copy(buf + n, inodes[i].name);
                n += str_copy(buf + n, "\",\"line\":");
                n += int_to_str((int)line, buf + n);
                n += str_copy(buf + n, ",\"col\":");
                n += int_to_str((int)col, buf + n);
                n += str_copy(buf + n, ",\"excerpt\":\"");
                /* 80-char excerpt */
                unsigned excerpt_len = 0;
                unsigned start = j;
                while (start > 0 && content[start-1] != '\n' && start > j - 40) start--;
                for (unsigned k = start; k < inodes[i].bytes && content[k] != '\n' && excerpt_len < 80; k++) {
                    if (content[k] == '"' || content[k] == '\\') buf[n++] = '\\';
                    buf[n++] = content[k];
                    excerpt_len++;
                }
                n += str_copy(buf + n, "\"}");
                match_count++;
            }
            col++;
        }
    }
    n += str_copy(buf + n, "],\"match_count\":");
    n += int_to_str((int)match_count, buf + n);
    n += str_copy(buf + n, ",\"files_searched\":");
    n += int_to_str((int)files_searched, buf + n);
    buf[n++] = '}';
    buf[n] = '\0';
    out->ok = true;
    out->data_json = arena_alloc(n + 1);
    str_copy(out->data_json, buf);
    return 0;
}

/* ----- stat ----- */
int op_stat(const struct jval *args, struct resp *out) {
    const struct jval *path_arg = json_get(args, "path");
    const struct inode *node;
    if (path_arg && path_arg->type == J_STR) {
        if (path_resolve(path_arg->s, &node) != 0) {
            out->ok = false;
            out->err_code = E_NOENT;
            out->err_meaning = "named path does not exist";
            out->sug_op = "ls";
            out->sug_args = "{\"path\":\"/\"}";
            return 0;
        }
    } else {
        node = cwd;
    }

    char buf[256];
    unsigned n = 0;
    n += str_copy(buf + n, "{\"name\":\"");
    n += str_copy(buf + n, node->name);
    n += str_copy(buf + n, "\",\"type\":\"");
    n += str_copy(buf + n, node->type == N_DIR ? "dir" : "file");
    n += str_copy(buf + n, "\",\"bytes\":");
    n += int_to_str((int)node->bytes, buf + n);
    n += str_copy(buf + n, ",\"inode\":");
    n += int_to_str((int)node->id, buf + n);
    if (node->type == N_DIR) {
        n += str_copy(buf + n, ",\"children\":[");
        bool first = true;
        for (uint32_t i = 1; i < inode_count; i++) {
            if (inodes[i].parent_id != node->id) continue;
            if (!first) buf[n++] = ',';
            first = false;
            buf[n++] = '"';
            n += str_copy(buf + n, inodes[i].name);
            buf[n++] = '"';
        }
        buf[n++] = ']';
    }
    buf[n++] = '}';
    buf[n] = '\0';
    out->ok = true;
    out->data_json = arena_alloc(n + 1);
    str_copy(out->data_json, buf);
    return 0;
}

/* ----- write ----- */
int op_write(const struct jval *args, struct resp *out) {
    const struct jval *path_arg = json_get(args, "path");
    const struct jval *text_arg = json_get(args, "text");
    if (!path_arg || path_arg->type != J_STR) {
        out->ok = false;
        out->err_code = E_ARG_MISSING;
        out->err_meaning = "required argument path absent";
        return 0;
    }
    if (!text_arg || text_arg->type != J_STR) {
        out->ok = false;
        out->err_code = E_ARG_MISSING;
        out->err_meaning = "required argument text absent";
        return 0;
    }
    /* For v1.0, write is a stub - we can't actually allocate new inodes safely
     * Document as future work */
    out->ok = false;
    out->err_code = E_FAULT;
    out->err_meaning = "write not implemented in v1.0";
    out->sug_op = "help";
    out->sug_args = "{}";
    return 0;
}

/* ----- pipe (M5 stub) ----- */
int op_pipe(const struct jval *args, struct resp *out) {
    (void)args;
    out->ok = false;
    out->err_code = E_UNKNOWN_OP;
    out->err_meaning = "pipe op is stubbed for v1.1";
    out->sug_op = "help";
    out->sug_args = "{}";
    return 0;
}
