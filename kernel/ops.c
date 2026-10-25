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

static bool str_eq(const char *a, const char *b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

static unsigned get_node_path(const struct inode *node, char *buf) {
    if (!node || node->id == 0 || (node->name[0] == '/' && node->name[1] == '\0')) {
        buf[0] = '/';
        buf[1] = '\0';
        return 1;
    }
    if (node->parent_id != 0 && node->parent_id < inode_count) {
        const struct inode *parent = &inodes[node->parent_id];
        unsigned n = 0;
        buf[n++] = '/';
        n += str_copy(buf + n, parent->name);
        buf[n++] = '/';
        n += str_copy(buf + n, node->name);
        return n;
    }
    unsigned n = 0;
    buf[n++] = '/';
    n += str_copy(buf + n, node->name);
    return n;
}

static unsigned get_cwd_path(char *buf) {
    return get_node_path(cwd, buf);
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
    n += int_to_str((int)ramfs_used_bytes(), buf + n);
    n += str_copy(buf + n, ",\"arena_used\":");
    n += int_to_str((int)arena_used_bytes(), buf + n);
    n += str_copy(buf + n, "},\"cwd\":\"");
    char cwd_str[64];
    get_cwd_path(cwd_str);
    n += str_copy(buf + n, cwd_str);
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
    char cwd_str[64];
    get_cwd_path(cwd_str);
    char buf[128];
    unsigned n = 0;
    n += str_copy(buf + n, "{\"cwd\":\"");
    n += str_copy(buf + n, cwd_str);
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
    char cwd_str[64];
    get_cwd_path(cwd_str);
    char buf[128];
    unsigned n = 0;
    n += str_copy(buf + n, "{\"cwd\":\"");
    n += str_copy(buf + n, cwd_str);
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
        if (c == '"' || c == '\\') {
            buf[n++] = '\\';
            buf[n++] = c;
        } else if (c == '\n') {
            buf[n++] = '\\';
            buf[n++] = 'n';
        } else if (c == '\r') {
            buf[n++] = '\\';
            buf[n++] = 'r';
        } else if (c == '\t') {
            buf[n++] = '\\';
            buf[n++] = 't';
        } else if ((unsigned char)c >= 0x20) {
            buf[n++] = c;
        }
    }
    n += str_copy(buf + n, "\",\"truncated\":false}");
    buf[n] = '\0';
    out->ok = true;
    out->data_json = arena_alloc(n + 1);
    str_copy(out->data_json, buf);
    return 0;
}

static char to_lower(char c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
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
    const struct jval *ic_arg = json_get(args, "ignore_case");
    bool ic = ic_arg && ic_arg->type == J_BOOL && ic_arg->b;

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

    for (uint32_t i = 0; i < inode_count; i++) {
        if (inodes[i].type != N_FILE) continue;
        if (start_dir) {
            if (start_dir->type == N_FILE && inodes[i].id != start_dir->id) continue;
            if (start_dir->type == N_DIR && start_dir->id != 0 && inodes[i].parent_id != start_dir->id) continue;
        }
        files_searched++;
        const char *content = ramfs_content + inodes[i].offset;
        unsigned line = 1, col = 1;
        for (uint32_t j = 0; j < inodes[i].bytes; j++) {
            if (content[j] == '\n') { line++; col = 1; continue; }
            bool match = true;
            for (unsigned k = 0; pattern[k]; k++) {
                if (j + k >= inodes[i].bytes) { match = false; break; }
                char c1 = content[j + k];
                char c2 = pattern[k];
                if (ic) { c1 = to_lower(c1); c2 = to_lower(c2); }
                if (c1 != c2) { match = false; break; }
            }
            if (match && match_count < 20) {
                if (match_count > 0) buf[n++] = ',';
                char file_path[64];
                get_node_path(&inodes[i], file_path);
                n += str_copy(buf + n, "{\"file\":\"");
                n += str_copy(buf + n, file_path);
                n += str_copy(buf + n, "\",\"line\":");
                n += int_to_str((int)line, buf + n);
                n += str_copy(buf + n, ",\"col\":");
                n += int_to_str((int)col, buf + n);
                n += str_copy(buf + n, ",\"excerpt\":\"");
                unsigned excerpt_len = 0;
                unsigned start = j;
                while (start > 0 && content[start-1] != '\n' && start > j - 40) start--;
                for (unsigned k = start; k < inodes[i].bytes && content[k] != '\n' && excerpt_len < 80; k++) {
                    if (content[k] == '\r') continue;
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
        out->sug_op = "help";
        out->sug_args = "{\"op\":\"write\"}";
        return 0;
    }
    if (!text_arg || text_arg->type != J_STR) {
        out->ok = false;
        out->err_code = E_ARG_MISSING;
        out->err_meaning = "required argument text absent";
        out->sug_op = "help";
        out->sug_args = "{\"op\":\"write\"}";
        return 0;
    }

    const char *path = path_arg->s;
    const char *text = text_arg->s;
    unsigned text_len = 0;
    while (text[text_len]) text_len++;

    const struct inode *existing;
    if (path_resolve(path, &existing) == 0) {
        if (existing->type == N_DIR) {
            out->ok = false;
            out->err_code = E_ISDIR;
            out->err_meaning = "cannot write to directory";
            out->sug_op = "ls";
            out->sug_args = "{}";
            return 0;
        }
        /* Overwrite existing file */
        struct inode *target = (struct inode *)existing;
        for (unsigned i = 0; i < text_len; i++) {
            ramfs_content[target->offset + i] = text[i];
        }
        target->bytes = text_len;

        char buf[256];
        unsigned n = 0;
        n += str_copy(buf + n, "{\"path\":\"");
        n += str_copy(buf + n, path);
        n += str_copy(buf + n, "\",\"bytes\":");
        n += int_to_str((int)text_len, buf + n);
        n += str_copy(buf + n, ",\"created\":false,\"overwritten\":true}");
        buf[n] = '\0';
        out->ok = true;
        out->data_json = arena_alloc(n + 1);
        str_copy(out->data_json, buf);
        return 0;
    }

    /* Create new file: parse parent directory and filename */
    char parent_path[128];
    char file_name[32];
    int last_slash = -1;
    for (int i = 0; path[i]; i++) {
        if (path[i] == '/') last_slash = i;
    }

    const struct inode *parent_dir;
    if (last_slash < 0) {
        parent_dir = cwd;
        unsigned i = 0;
        while (path[i] && i < 31) { file_name[i] = path[i]; i++; }
        file_name[i] = '\0';
    } else if (last_slash == 0) {
        parent_dir = &inodes[0];
        unsigned i = 0;
        const char *src = path + 1;
        while (src[i] && i < 31) { file_name[i] = src[i]; i++; }
        file_name[i] = '\0';
    } else {
        int i = 0;
        for (; i < last_slash && i < 127; i++) parent_path[i] = path[i];
        parent_path[i] = '\0';
        unsigned j = 0;
        const char *src = path + last_slash + 1;
        while (src[j] && j < 31) { file_name[j] = src[j]; j++; }
        file_name[j] = '\0';

        if (path_resolve(parent_path, &parent_dir) != 0) {
            out->ok = false;
            out->err_code = E_NOENT;
            out->err_meaning = "parent directory does not exist";
            out->sug_op = "ls";
            out->sug_args = "{\"path\":\"/\"}";
            return 0;
        }
    }

    if (parent_dir->type != N_DIR) {
        out->ok = false;
        out->err_code = E_NOTDIR;
        out->err_meaning = "parent path is not a directory";
        return 0;
    }

    if (inode_count >= MAX_INODES) {
        out->ok = false;
        out->err_code = E_FAULT;
        out->err_meaning = "ramfs inode table full";
        return 0;
    }

    /* Find next free offset in content area */
    uint32_t next_offset = 0;
    for (uint32_t i = 0; i < inode_count; i++) {
        if (inodes[i].type == N_FILE) {
            uint32_t end = inodes[i].offset + inodes[i].bytes;
            if (end > next_offset) next_offset = end;
        }
    }
    next_offset = (next_offset + 511) & ~511u;

    if (next_offset + text_len >= RAMFS_SIZE - MAX_INODES * sizeof(struct inode)) {
        out->ok = false;
        out->err_code = E_FAULT;
        out->err_meaning = "ramfs space full";
        return 0;
    }

    struct inode *new_ino = &inodes[inode_count];
    new_ino->id = inode_count;
    new_ino->type = N_FILE;
    unsigned name_i = 0;
    while (file_name[name_i] && name_i < 31) {
        new_ino->name[name_i] = file_name[name_i];
        name_i++;
    }
    new_ino->name[name_i] = '\0';
    new_ino->parent_id = parent_dir->id;
    new_ino->offset = next_offset;
    new_ino->bytes = text_len;

    for (unsigned i = 0; i < text_len; i++) {
        ramfs_content[next_offset + i] = text[i];
    }
    inode_count++;

    char buf[256];
    unsigned n = 0;
    n += str_copy(buf + n, "{\"path\":\"");
    n += str_copy(buf + n, path);
    n += str_copy(buf + n, "\",\"bytes\":");
    n += int_to_str((int)text_len, buf + n);
    n += str_copy(buf + n, ",\"created\":true,\"overwritten\":false}");
    buf[n] = '\0';
    out->ok = true;
    out->data_json = arena_alloc(n + 1);
    str_copy(out->data_json, buf);
    return 0;
}

/* ----- pipe (M5) ----- */
int op_pipe(const struct jval *args, struct resp *out) {
    const struct jval *steps_arg = json_get(args, "steps");
    if (!steps_arg || steps_arg->type != J_ARR) {
        out->ok = false;
        out->err_code = E_ARG_MISSING;
        out->err_meaning = "required argument steps absent or not an array";
        out->sug_op = "help";
        out->sug_args = "{\"op\":\"pipe\"}";
        return 0;
    }

    if (steps_arg->arr.count == 0) {
        out->ok = false;
        out->err_code = E_ARG_MISSING;
        out->err_meaning = "steps array cannot be empty";
        return 0;
    }

    if (steps_arg->arr.count > MAX_STEPS_PER_PIPE) {
        out->ok = false;
        out->err_code = E_TOO_LONG;
        out->err_meaning = "steps count exceeds MAX_STEPS_PER_PIPE (8)";
        return 0;
    }

    char *buf = arena_alloc(4096);
    unsigned n = 0;
    n += str_copy(buf + n, "{\"steps_completed\":");
    unsigned count_pos = n;
    n += str_copy(buf + n, "0,\"results\":[");

    const char *prev_data = NULL;
    unsigned completed = 0;
    bool pipeline_ok = true;

    for (unsigned i = 0; i < steps_arg->arr.count; i++) {
        const struct jval *step = &steps_arg->arr.items[i];
        if (step->type != J_OBJ) {
            pipeline_ok = false;
            break;
        }

        const struct jval *op_field = json_get(step, "op");
        const struct jval *step_args = json_get(step, "args");
        if (!op_field || op_field->type != J_STR) {
            pipeline_ok = false;
            break;
        }

        if (str_eq(op_field->s, "pipe")) {
            pipeline_ok = false;
            break;
        }

        const struct op_entry *entry = registry_find(op_field->s);
        if (!entry) {
            if (i > 0) buf[n++] = ',';
            n += str_copy(buf + n, "{\"op\":\"");
            n += str_copy(buf + n, op_field->s);
            n += str_copy(buf + n, "\",\"ok\":false,\"error\":\"unknown op\"}");
            pipeline_ok = false;
            break;
        }

        struct jval empty_args;
        empty_args.type = J_OBJ;
        empty_args.obj.count = 0;
        empty_args.obj.keys = NULL;
        empty_args.obj.vals = NULL;
        const struct jval *call_args = step_args ? step_args : &empty_args;

        /* $prev substitution in string arguments */
        if (prev_data && call_args->type == J_OBJ) {
            for (unsigned k = 0; k < call_args->obj.count; k++) {
                if (call_args->obj.vals[k].type == J_STR) {
                    if (str_eq(call_args->obj.vals[k].s, "$prev")) {
                        call_args->obj.vals[k].s = prev_data;
                    }
                }
            }
        }

        struct resp step_resp;
        step_resp.ok = false;
        step_resp.data_json = NULL;
        step_resp.err_code = E_FAULT;
        step_resp.err_meaning = "step failed";
        step_resp.sug_op = NULL;
        step_resp.sug_args = NULL;

        entry->fn(call_args, &step_resp);

        if (i > 0) buf[n++] = ',';
        n += str_copy(buf + n, "{\"op\":\"");
        n += str_copy(buf + n, op_field->s);
        n += str_copy(buf + n, "\",\"ok\":");
        n += str_copy(buf + n, step_resp.ok ? "true" : "false");

        if (step_resp.ok) {
            n += str_copy(buf + n, ",\"data\":");
            if (step_resp.data_json) {
                n += str_copy(buf + n, step_resp.data_json);
                prev_data = step_resp.data_json;
            } else {
                n += str_copy(buf + n, "{}");
            }
            buf[n++] = '}';
            completed++;
        } else {
            n += str_copy(buf + n, ",\"error\":{\"code\":\"");
            n += str_copy(buf + n, step_resp.err_code);
            n += str_copy(buf + n, "\",\"meaning\":\"");
            n += str_copy(buf + n, step_resp.err_meaning);
            n += str_copy(buf + n, "\"}}");
            completed++;
            pipeline_ok = false;
            break;
        }
        buf[n] = '\0';
    }

    buf[count_pos] = '0' + (completed % 10);
    n += str_copy(buf + n, "]}");
    buf[n] = '\0';

    out->ok = pipeline_ok;
    out->data_json = buf;
    return 0;
}
