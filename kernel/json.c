/* AGENCY-1 JSON parser
 * Hand-rolled, no libc. Parses one JSON line into a jval tree.
 * All allocations come from the arena.
 */

#include "agency1.h"

static const char *parse_value(const char *p, struct jval *out);

static const char *skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

static const char *parse_string(const char *p, const char **out_str) {
    if (*p != '"') return NULL;
    p++;
    *out_str = p;
    while (*p && *p != '"') {
        if (*p == '\\') p++;
        p++;
    }
    if (*p != '"') return NULL;
    return p + 1;
}

static const char *parse_object(const char *p, struct jval *out) {
    if (*p != '{') return NULL;
    p++;
    p = skip_ws(p);

    out->type = J_OBJ;
    out->obj.keys = NULL;
    out->obj.vals = NULL;
    out->obj.count = 0;

    if (*p == '}') return p + 1;

    /* First pass: count keys */
    const char *q = p;
    unsigned count = 0;
    while (*q && *q != '}') {
        count++;
        while (*q && *q != ':' && *q != '}') q++;
        if (*q == ':') {
            q++;
            int depth = 0;
            while (*q) {
                if (*q == '{' || *q == '[') depth++;
                else if (*q == '}' || *q == ']') {
                    if (depth == 0) break;
                    depth--;
                }
                q++;
            }
            if (*q == ',') q++;
            q = skip_ws(q);
        }
    }

    if (count == 0) return p + 1;

    out->obj.keys = arena_alloc(count * sizeof(char *));
    out->obj.vals = arena_alloc(count * sizeof(struct jval));
    out->obj.count = count;

    unsigned idx = 0;
    while (*p && *p != '}' && idx < count) {
        p = skip_ws(p);
        const char *key;
        p = parse_string(p, &key);
        if (!p) return NULL;
        ((char *)key)[p - key - 1] = '\0';
        out->obj.keys[idx] = (char *)key;
        p = skip_ws(p);
        if (*p != ':') return NULL;
        p++;
        p = skip_ws(p);
        p = parse_value(p, &out->obj.vals[idx]);
        if (!p) return NULL;
        idx++;
        p = skip_ws(p);
        if (*p == ',') { p++; p = skip_ws(p); }
    }
    if (*p == '}') p++;
    return p;
}

static const char *parse_array(const char *p, struct jval *out) {
    if (*p != '[') return NULL;
    p++;
    p = skip_ws(p);

    out->type = J_ARR;
    out->arr.items = NULL;
    out->arr.count = 0;

    if (*p == ']') return p + 1;

    const char *q = p;
    unsigned count = 0;
    int depth = 0;
    while (*q) {
        if (*q == '{' || *q == '[') depth++;
        else if (*q == '}' || *q == ']') {
            if (depth == 0) break;
            depth--;
        }
        else if (*q == ',' && depth == 0) count++;
        q++;
    }
    count++;

    out->arr.items = arena_alloc(count * sizeof(struct jval));
    out->arr.count = count;

    unsigned idx = 0;
    while (*p && *p != ']' && idx < count) {
        p = skip_ws(p);
        p = parse_value(p, &out->arr.items[idx]);
        if (!p) return NULL;
        idx++;
        p = skip_ws(p);
        if (*p == ',') { p++; p = skip_ws(p); }
    }
    if (*p == ']') p++;
    return p;
}

static const char *parse_value(const char *p, struct jval *out) {
    p = skip_ws(p);
    if (*p == '"') {
        const char *s;
        const char *np = parse_string(p, &s);
        if (!np) return NULL;
        ((char *)s)[np - s - 1] = '\0';
        out->type = J_STR;
        out->s = s;
        return np;
    } else if (*p == '{') {
        return parse_object(p, out);
    } else if (*p == '[') {
        return parse_array(p, out);
    } else if (*p == 't') {
        if (p[1] == 'r' && p[2] == 'u' && p[3] == 'e') {
            out->type = J_BOOL;
            out->b = 1;
            return p + 4;
        }
        return NULL;
    } else if (*p == 'f') {
        if (p[1] == 'a' && p[2] == 'l' && p[3] == 's' && p[4] == 'e') {
            out->type = J_BOOL;
            out->b = 0;
            return p + 5;
        }
        return NULL;
    } else if (*p == 'n') {
        if (p[1] == 'u' && p[2] == 'l' && p[3] == 'l') {
            out->type = J_NULL;
            return p + 4;
        }
        return NULL;
    } else if (*p == '-' || (*p >= '0' && *p <= '9')) {
        int sign = 1;
        if (*p == '-') { sign = -1; p++; }
        int val = 0;
        while (*p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
            p++;
        }
        out->type = J_INT;
        out->i = sign * val;
        return p;
    }
    return NULL;
}

int json_parse(const char *line, struct jval *out) {
    const char *p = parse_value(line, out);
    return p != NULL;
}

const struct jval *json_get(const struct jval *obj, const char *key) {
    if (!obj || obj->type != J_OBJ) return NULL;
    for (unsigned i = 0; i < obj->obj.count; i++) {
        const char *a = obj->obj.keys[i];
        const char *b = key;
        bool match = true;
        while (*a && *b) {
            if (*a != *b) { match = false; break; }
            a++; b++;
        }
        if (match && *a == '\0' && *b == '\0') {
            return &obj->obj.vals[i];
        }
    }
    return NULL;
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

static unsigned str_copy(const char *s, char *out, unsigned cap) {
    unsigned n = 0;
    while (*s && n < cap) out[n++] = *s++;
    return n;
}

static unsigned serialize_string(const char *s, char *out, unsigned cap) {
    unsigned n = 0;
    if (n < cap) out[n++] = '"';
    while (*s && n < cap - 1) {
        if (*s == '"' || *s == '\\') {
            if (n < cap - 1) out[n++] = '\\';
        }
        out[n++] = *s++;
    }
    if (n < cap) out[n++] = '"';
    return n;
}

static unsigned serialize_value(const struct jval *v, char *out, unsigned cap);

static unsigned serialize_value(const struct jval *v, char *out, unsigned cap) {
    switch (v->type) {
        case J_NULL:
            return str_copy("null", out, cap);
        case J_BOOL:
            return str_copy(v->b ? "true" : "false", out, cap);
        case J_INT: {
            char num[12];
            int len = int_to_str(v->i, num);
            unsigned n = 0;
            { unsigned k = 0; while (k < (unsigned)len && n < cap) { out[n++] = num[k++]; } }
            return n;
        }
        case J_STR:
            return serialize_string(v->s, out, cap);
        case J_ARR: {
            unsigned n = 0;
            if (n < cap) out[n++] = '[';
            for (unsigned i = 0; i < v->arr.count; i++) {
                if (i > 0 && n < cap) out[n++] = ',';
                n += serialize_value(&v->arr.items[i], out + n, cap - n);
            }
            if (n < cap) out[n++] = ']';
            return n;
        }
        case J_OBJ: {
            unsigned n = 0;
            if (n < cap) out[n++] = '{';
            for (unsigned i = 0; i < v->obj.count; i++) {
                if (i > 0 && n < cap) out[n++] = ',';
                n += serialize_string(v->obj.keys[i], out + n, cap - n);
                if (n < cap) out[n++] = ':';
                n += serialize_value(&v->obj.vals[i], out + n, cap - n);
            }
            if (n < cap) out[n++] = '}';
            return n;
        }
    }
    return 0;
}

unsigned json_serialize(const struct jval *v, char *out, unsigned cap) {
    return serialize_value(v, out, cap);
}
