/* AGENCY-1 Reply module
 * Constructs and sends response envelopes.
 * Format: {"v":1,"id":ID,"ok":BOOL,[DATA|ERROR],"stats":{"us":U,"bytes_out":B,"tokens_saved":T}}\n
 */

#include "agency1.h"

static const struct {
    const char *op;
    unsigned human_equiv_bytes;
} TOKEN_TABLE[] = {
    { "ping",     16    },
    { "help",     8200  },
    { "sys.info", 80    },
    { "pwd",      8     },
    { "cd",       12    },
    { "ls",       200   },
    { "cat",      0     },
    { "grep",     0     },
    { "stat",     80    },
    { "write",    30    },
    { "pipe",     0     },
    { NULL,       0     }
};

static bool str_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return false;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

unsigned tokens_saved_for(const char *op_name, unsigned bytes_out) {
    for (unsigned i = 0; TOKEN_TABLE[i].op; i++) {
        if (str_eq(TOKEN_TABLE[i].op, op_name)) {
            unsigned human = TOKEN_TABLE[i].human_equiv_bytes;
            if (human == 0) return 0;
            int saved = (int)human - (int)bytes_out;
            if (saved < 0) saved = 0;
            return (unsigned)saved / 4;
        }
    }
    return 0;
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

static unsigned str_ncopy(const char *s, char *out, unsigned cap) {
    unsigned n = 0;
    while (*s && n < cap) out[n++] = *s++;
    return n;
}

void reply_send(const struct request *req, const struct resp *resp) {
    char buf[MAX_LINE_LEN + 256];
    unsigned n = 0;

    n += str_ncopy("{\"v\":1,\"id\":", buf + n, MAX_LINE_LEN - n);
    n += int_to_str(req->id, buf + n);
    buf[n++] = ',';
    if (resp->ok) {
        n += str_ncopy("\"ok\":true,\"data\":", buf + n, MAX_LINE_LEN - n);
        if (resp->data_json) {
            while (*resp->data_json && n < MAX_LINE_LEN - 1) {
                { const char *d = resp->data_json; while (*d && n < MAX_LINE_LEN - 1) { buf[n++] = *d++; } }
            }
        } else {
            buf[n++] = '{';
            buf[n++] = '}';
        }
    } else {
        n += str_ncopy("\"ok\":false,\"error\":{\"code\":\"", buf + n, MAX_LINE_LEN - n);
        n += str_ncopy(resp->err_code, buf + n, MAX_LINE_LEN - n);
        n += str_ncopy("\",\"meaning\":\"", buf + n, MAX_LINE_LEN - n);
        n += str_ncopy(resp->err_meaning, buf + n, MAX_LINE_LEN - n);
        buf[n++] = '"';
        if (resp->sug_op) {
            n += str_ncopy(",\"suggested_next_call\":{\"op\":\"", buf + n, MAX_LINE_LEN - n);
            n += str_ncopy(resp->sug_op, buf + n, MAX_LINE_LEN - n);
            n += str_ncopy("\",\"args\":", buf + n, MAX_LINE_LEN - n);
            if (resp->sug_args) {
                n += str_ncopy(resp->sug_args, buf + n, MAX_LINE_LEN - n);
            } else {
                buf[n++] = '{';
                buf[n++] = '}';
            }
            buf[n++] = '}';
        }
        buf[n++] = '}';
    }

    /* Stats */
    n += str_ncopy(",\"stats\":{\"us\":", buf + n, MAX_LINE_LEN - n);
    uint32_t us = resp->us_end - resp->us_start;
    n += int_to_str((int)us, buf + n);
    n += str_ncopy(",\"bytes_out\":", buf + n, MAX_LINE_LEN - n);
    n += int_to_str((int)n, buf + n);
    n += str_ncopy(",\"tokens_saved\":", buf + n, MAX_LINE_LEN - n);
    unsigned ts = tokens_saved_for(req->op, n);
    n += int_to_str((int)ts, buf + n);
    buf[n++] = '}';
    buf[n++] = '}';
    buf[n++] = '\n';

    serial_write(buf, n);
}

void reply_error(int id, const char *code, const char *meaning,
                 const char *sug_op, const char *sug_args) {
    struct request req;
    req.id = id;
    struct resp resp;
    resp.ok = false;
    resp.err_code = code;
    resp.err_meaning = meaning;
    resp.sug_op = sug_op;
    resp.sug_args = sug_args;
    resp.us_start = 0;
    resp.us_end = 0;
    reply_send(&req, &resp);
}

void reply_unknown_op(const struct request *req) {
    reply_error(req->id, E_UNKNOWN_OP, "op name not in registry",
                "help", "{}");
}
