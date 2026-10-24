/* AGENCY-1 Bump Allocator
 * Used for JSON parsing and pipeline runner allocations.
 * Reset only at shutdown (per spec R7), never per-request.
 */

#include "agency1.h"

static char *arena_ptr = (char *)ARENA_BASE;
static char *arena_end = (char *)(ARENA_BASE + ARENA_SIZE);

void arena_init(void) {
    arena_ptr = (char *)ARENA_BASE;
    /* Zero out first 4 KiB for safety */
    for (int i = 0; i < 4096; i++) {
        arena_ptr[i] = 0;
    }
}

void *arena_alloc(unsigned size) {
    /* 8-byte align */
    size = (size + 7) & ~7u;
    if (arena_ptr + size > arena_end) {
        return NULL;
    }
    void *p = arena_ptr;
    arena_ptr += size;
    return p;
}
