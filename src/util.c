#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void err_vset(Err *e, const char *fmt, va_list ap) {
    if (!e) return;
    vsnprintf(e->msg, sizeof(e->msg), fmt, ap);
    e->set = true;
}

void err_set(Err *e, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    err_vset(e, fmt, ap);
    va_end(ap);
}

/* ------------------------------------------------------------------ */
/* arena                                                               */
/* ------------------------------------------------------------------ */
#define ARENA_BLOCK_MAX (4u * 1024u * 1024u)
#define ARENA_ALIGN 16u

static size_t align_up(size_t n) { return (n + (ARENA_ALIGN - 1)) & ~(size_t)(ARENA_ALIGN - 1); }

static ArenaBlock *block_new(size_t cap) {
    ArenaBlock *b = (ArenaBlock *)malloc(sizeof(ArenaBlock) + cap);
    if (!b) return NULL;
    b->next = NULL;
    b->cap = cap;
    b->used = 0;
    return b;
}

static char *block_data(ArenaBlock *b) { return (char *)(b + 1); }

void arena_init(Arena *a) { a->head = NULL; }

void arena_free(Arena *a) {
    ArenaBlock *b = a->head;
    while (b) {
        ArenaBlock *nx = b->next;
        free(b);
        b = nx;
    }
    a->head = NULL;
}

void *arena_alloc(Arena *a, size_t n) {
    if (n == 0) n = 1;
    n = align_up(n);
    ArenaBlock *b = a->head;
    if (!b || b->cap - b->used < n) {
        size_t cap = n > ARENA_BLOCK_MAX ? n : ARENA_BLOCK_MAX;
        ArenaBlock *nb = block_new(cap);
        if (!nb) return NULL;
        nb->next = a->head;
        a->head = nb;
        b = nb;
    }
    void *p = block_data(b) + b->used;
    b->used += n;
    return p;
}

void *arena_grow(Arena *a, void *old, size_t old_n, size_t new_n) {
    ArenaBlock *b = a->head;
    if (old && b) {
        char *d = block_data(b);
        char *end = d + b->used;
        if ((char *)old >= d && (char *)old < end && (char *)old + old_n == end) {
            size_t extra = align_up(new_n) - old_n;
            if (b->cap - b->used >= extra) {
                b->used += extra;
                return old;
            }
        }
    }
    void *p = arena_alloc(a, new_n);
    if (p && old && old_n) memcpy(p, old, old_n);
    return p;
}

char *arena_strndup(Arena *a, const char *s, size_t n) {
    char *p = (char *)arena_alloc(a, n + 1);
    if (!p) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

char *arena_strdup(Arena *a, const char *s) { return s ? arena_strndup(a, s, strlen(s)) : NULL; }

char *arena_printf(Arena *a, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) {
        va_end(ap2);
        return NULL;
    }
    char *p = (char *)arena_alloc(a, (size_t)n + 1);
    if (p) vsnprintf(p, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return p;
}

/* ------------------------------------------------------------------ */
/* string builder                                                      */
/* ------------------------------------------------------------------ */
void sb_init(SB *b) {
    b->p = NULL;
    b->n = 0;
    b->cap = 0;
}

void sb_free(SB *b) {
    free(b->p);
    sb_init(b);
}

void sb_reserve(SB *b, size_t extra) {
    if (b->n + extra + 1 <= b->cap) return;
    size_t cap = b->cap ? b->cap : 256;
    while (cap < b->n + extra + 1) cap += cap / 2 + 64;
    char *p = (char *)realloc(b->p, cap);
    if (!p) abort();
    b->p = p;
    b->cap = cap;
}

void sb_putc(SB *b, char c) {
    sb_reserve(b, 1);
    b->p[b->n++] = c;
    b->p[b->n] = '\0';
}

void sb_write(SB *b, const void *data, size_t n) {
    if (!n) return;
    sb_reserve(b, n);
    memcpy(b->p + b->n, data, n);
    b->n += n;
    b->p[b->n] = '\0';
}

void sb_puts(SB *b, const char *s) {
    if (s) sb_write(b, s, strlen(s));
}

void sb_printf(SB *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n > 0) {
        sb_reserve(b, (size_t)n);
        vsnprintf(b->p + b->n, (size_t)n + 1, fmt, ap2);
        b->n += (size_t)n;
    }
    va_end(ap2);
}

void sb_repeat(SB *b, char c, size_t n) {
    sb_reserve(b, n);
    memset(b->p + b->n, c, n);
    b->n += n;
    b->p[b->n] = '\0';
}

char *sb_take(SB *b) {
    if (!b->p) {
        char *p = (char *)malloc(1);
        if (!p) abort();
        p[0] = '\0';
        return p;
    }
    char *p = b->p;
    b->p = NULL;
    b->n = b->cap = 0;
    return p;
}

const char *sb_cstr(SB *b) {
    if (!b->p) {
        sb_reserve(b, 1);
        b->p[0] = '\0';
    }
    return b->p;
}

/* ------------------------------------------------------------------ */
/* strings                                                             */
/* ------------------------------------------------------------------ */
size_t cstr_len(const char *s) { return s ? strlen(s) : 0; }

bool str_eq(const char *a, const char *b) { return strcmp(a ? a : "", b ? b : "") == 0; }

static char lower_ascii(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; }

bool str_eq_ci(const char *a, const char *b) {
    if (!a || !b) return a == b;
    while (*a && *b) {
        if (lower_ascii(*a) != lower_ascii(*b)) return false;
        a++;
        b++;
    }
    return *a == *b;
}

bool str_starts_ci(const char *s, const char *prefix) {
    if (!s || !prefix) return false;
    while (*prefix) {
        if (!*s || lower_ascii(*s) != lower_ascii(*prefix)) return false;
        s++;
        prefix++;
    }
    return true;
}

bool str_contains_ci(const char *hay, const char *needle) {
    if (!hay || !needle) return false;
    if (!*needle) return true;
    for (const char *h = hay; *h; h++) {
        const char *a = h;
        const char *b = needle;
        while (*a && *b && lower_ascii(*a) == lower_ascii(*b)) {
            a++;
            b++;
        }
        if (!*b) return true;
    }
    return false;
}

char *str_lower_dup(Arena *a, const char *s) {
    size_t n = cstr_len(s);
    char *p = arena_strndup(a, s ? s : "", n);
    if (!p) return NULL;
    for (size_t i = 0; i < n; i++) p[i] = lower_ascii(p[i]);
    return p;
}

void i64_to_str(int64_t v, char out[24]) { snprintf(out, 24, "%lld", (long long)v); }

void pvec_push(Arena *a, PVec *v, void *item) {
    if (v->n == v->cap) {
        size_t ncap = v->cap ? v->cap * 2 : 8;
        void **ni = (void **)arena_grow(a, v->items, v->cap * sizeof(void *), ncap * sizeof(void *));
        if (!ni) return;
        v->items = ni;
        v->cap = ncap;
    }
    v->items[v->n++] = item;
}
