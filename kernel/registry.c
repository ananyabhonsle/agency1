/* AGENCY-1 Op Registry
 * The single source of truth — kernel dispatch + model tool schemas
 * are both generated from this table.
 */

#include "agency1.h"

const struct op_entry OPS[] = {
    { "ping",     "",                                         op_ping     },
    { "help",     "op:str?",                                  op_help     },
    { "sys.info", "",                                         op_sysinfo  },
    { "pwd",      "",                                         op_pwd      },
    { "cd",       "path:str",                                 op_cd       },
    { "ls",       "path:str?",                                op_ls       },
    { "cat",      "path:str",                                 op_cat      },
    { "grep",     "pattern:str path:str? ignore_case:bool?",  op_grep     },
    { "stat",     "path:str?",                                op_stat     },
    { "write",    "path:str text:str",                        op_write    },
    { "pipe",     "steps:arr",                                op_pipe     },
    { NULL, NULL, NULL }
};

const struct op_entry *registry_find(const char *name) {
    for (unsigned i = 0; OPS[i].name; i++) {
        const char *a = OPS[i].name;
        const char *b = name;
        bool match = true;
        while (*a && *b) {
            if (*a != *b) { match = false; break; }
            a++; b++;
        }
        if (match && *a == '\0' && *b == '\0') {
            return &OPS[i];
        }
    }
    return NULL;
}
