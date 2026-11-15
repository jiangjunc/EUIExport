/* util.h - arena/string-builder/error primitives shared by every module. */
#ifndef EUI_UTIL_H
#define EUI_UTIL_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* error channel                                                       */
/* ------------------------------------------------------------------ */
typedef struct Err {
    char msg[1024];
    bool set;
} Err;

void err_set(Err *e, const char *fmt, ...);
void err_vset(Err *e, const char *fmt, va_list ap);

/* ------------------------------------------------------------------ */
/* arena: bump allocator, freed in one shot                            */
/* ------------------------------------------------------------------ */
typedef struct ArenaBlock {
    struct ArenaBlock *next;
    size_t cap;
    size_t used;
    /* payload follows, 16-byte aligned */
} ArenaBlock;

typedef struct Arena {
    ArenaBlock *head;
} Arena;

void arena_init(Arena *a);
void arena_free(Arena *a);
void *arena_alloc(Arena *a, size_t n);
/* Grow an arena block: in place when it is the most recent allocation. */
void *arena_grow(Arena *a, void *old, size_t old_n, size_t new_n);
char *arena_strndup(Arena *a, const char *s, size_t n);
char *arena_strdup(Arena *a, const char *s);
char *arena_printf(Arena *a, const char *fmt, ...);

/* ------------------------------------------------------------------ */
/* string builder (heap, transferable)                                 */
/* ------------------------------------------------------------------ */
typedef struct SB {
    char *p;
    size_t n;
    size_t cap;
} SB;

void sb_init(SB *b);
void sb_free(SB *b);
void sb_reserve(SB *b, size_t extra);
void sb_putc(SB *b, char c);
void sb_write(SB *b, const void *data, size_t n);
void sb_puts(SB *b, const char *s);
void sb_printf(SB *b, const char *fmt, ...);
void sb_repeat(SB *b, char c, size_t n);
/* Returns heap buffer (NUL terminated) and resets the builder. */
char *sb_take(SB *b);
const char *sb_cstr(SB *b);

/* ------------------------------------------------------------------ */
/* ASCII string helpers (UTF-8 byte-safe for the ASCII subset)          */
/* ------------------------------------------------------------------ */
size_t cstr_len(const char *s);
bool str_eq(const char *a, const char *b);
bool str_eq_ci(const char *a, const char *b);
bool str_starts_ci(const char *s, const char *prefix);
bool str_contains_ci(const char *hay, const char *needle);
char *str_lower_dup(Arena *a, const char *s);
/* JS-style String(number) for integer values. */
void i64_to_str(int64_t v, char out[24]);

/* Growable pointer array living in an arena. */
typedef struct PVec {
    void **items;
    size_t n;
    size_t cap;
} PVec;

void pvec_push(Arena *a, PVec *v, void *item);

#endif /* EUI_UTIL_H */
