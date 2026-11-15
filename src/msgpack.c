#include "msgpack.h"

#include <stdio.h>
#include <string.h>

#include "json.h"
#include "utf8.h"

#define MP_MAX_DEPTH 256
#define MP_SAFE_INT 9007199254740991LL /* 2^53 - 1 */

typedef struct Cur {
    const unsigned char *p;
    size_t n;
    size_t i;
    Arena *a;
    Err *e;
    bool failed;
} Cur;

static bool need(Cur *c, size_t k) {
    if (c->i + k > c->n) {
        err_set(c->e, "MessagePack 数据截断（偏移 %zu，需要 %zu 字节）", c->i, k);
        c->failed = true;
        return false;
    }
    return true;
}

static uint64_t be(Cur *c, size_t k) {
    uint64_t v = 0;
    for (size_t i = 0; i < k; i++) v = (v << 8) | (uint64_t)c->p[c->i + i];
    c->i += k;
    return v;
}

static MpVal *new_val(Cur *c, MpType t) {
    MpVal *v = (MpVal *)arena_alloc(c->a, sizeof(MpVal));
    if (!v) {
        c->failed = true;
        err_set(c->e, "内存不足");
        return NULL;
    }
    memset(v, 0, sizeof(*v));
    v->t = t;
    return v;
}

static MpVal *mk_str(Cur *c, const char *p, size_t n) {
    MpVal *v = new_val(c, MP_STR);
    if (!v) return NULL;
    v->u.s.p = p;
    v->u.s.n = (uint32_t)n;
    return v;
}

/* JavaScript object keys are strings; mirrors ToPropertyKey. */
static char *key_to_string(Cur *c, MpVal *k) {
    char buf[40];
    switch (k->t) {
        case MP_STR: return arena_strndup(c->a, k->u.s.p, k->u.s.n);
        case MP_INT:
            snprintf(buf, sizeof buf, "%lld", (long long)k->u.i);
            return arena_strdup(c->a, buf);
        case MP_DBL:
            js_number_to_str(k->u.d, buf);
            return arena_strdup(c->a, buf);
        case MP_BOOL: return arena_strdup(c->a, k->u.b ? "true" : "false");
        case MP_NULL: return arena_strdup(c->a, "null");
        default: return arena_strdup(c->a, "[object Object]");
    }
}

static MpVal *decode_val(Cur *c, int depth);

static MpVal *decode_array(Cur *c, uint32_t count, int depth) {
    if (count > c->n - c->i) {
        err_set(c->e, "MessagePack 数组长度异常（%u）", count);
        c->failed = true;
        return NULL;
    }
    MpVal *v = new_val(c, MP_ARR);
    if (!v) return NULL;
    MpVal **items = (MpVal **)arena_alloc(c->a, (size_t)count * sizeof(MpVal *) + sizeof(MpVal *));
    if (!items) {
        c->failed = true;
        return NULL;
    }
    for (uint32_t k = 0; k < count; k++) {
        MpVal *e = decode_val(c, depth + 1);
        if (!e) return NULL;
        items[k] = e;
    }
    v->u.arr.items = items;
    v->u.arr.n = count;
    return v;
}

static MpVal *decode_map(Cur *c, uint32_t count, int depth) {
    if ((size_t)count * 2 > (c->n - c->i) + 1 && count > 0) {
        err_set(c->e, "MessagePack 映射长度异常（%u）", count);
        c->failed = true;
        return NULL;
    }
    MpVal *v = new_val(c, MP_MAP);
    if (!v) return NULL;
    const char **keys = (const char **)arena_alloc(c->a, (size_t)count * sizeof(char *) + sizeof(char *));
    MpVal **vals = (MpVal **)arena_alloc(c->a, (size_t)count * sizeof(MpVal *) + sizeof(MpVal *));
    if (!keys || !vals) {
        c->failed = true;
        return NULL;
    }
    for (uint32_t k = 0; k < count; k++) {
        MpVal *kv = decode_val(c, depth + 1);
        if (!kv) return NULL;
        char *name = key_to_string(c, kv);
        if (!name) return NULL;
        MpVal *vv = decode_val(c, depth + 1);
        if (!vv) return NULL;
        keys[k] = name;
        vals[k] = vv;
    }
    v->u.map.keys = keys;
    v->u.map.vals = vals;
    v->u.map.n = count;
    return v;
}

static MpVal *str_ref(Cur *c, size_t len) {
    if (!need(c, len)) return NULL;
    MpVal *v = mk_str(c, (const char *)(c->p + c->i), len);
    c->i += len;
    return v;
}

static MpVal *decode_val(Cur *c, int depth) {
    if (c->failed) return NULL;
    if (depth > MP_MAX_DEPTH) {
        err_set(c->e, "MessagePack 嵌套过深");
        c->failed = true;
        return NULL;
    }
    if (!need(c, 1)) return NULL;
    unsigned b = c->p[c->i++];

    if (b <= 0x7f) {
        MpVal *v = new_val(c, MP_INT);
        if (v) v->u.i = (int64_t)b;
        return v;
    }
    if (b >= 0xe0) {
        MpVal *v = new_val(c, MP_INT);
        if (v) v->u.i = (int64_t)b - 256;
        return v;
    }
    if (b >= 0x80 && b <= 0x8f) return decode_map(c, b - 0x80u, depth);
    if (b >= 0x90 && b <= 0x9f) return decode_array(c, b - 0x90u, depth);
    if (b >= 0xa0 && b <= 0xbf) return str_ref(c, b - 0xa0u);

    switch (b) {
        case 0xc0: return new_val(c, MP_NULL);
        case 0xc2:
        case 0xc3: {
            MpVal *v = new_val(c, MP_BOOL);
            if (v) v->u.b = (b == 0xc3);
            return v;
        }
        case 0xc4: {
            if (!need(c, 1)) return NULL;
            uint32_t len = (uint32_t)be(c, 1);
            return str_ref(c, len);
        }
        case 0xc5: {
            if (!need(c, 2)) return NULL;
            uint32_t len = (uint32_t)be(c, 2);
            if (!need(c, len)) return NULL;
            char *b64 = base64_encode(c->a, c->p + c->i, len);
            c->i += len;
            return b64 ? mk_str(c, b64, strlen(b64)) : NULL;
        }
        case 0xc6: {
            if (!need(c, 4)) return NULL;
            uint32_t len = (uint32_t)be(c, 4);
            if (!need(c, len)) return NULL;
            char *b64 = base64_encode(c->a, c->p + c->i, len);
            c->i += len;
            return b64 ? mk_str(c, b64, strlen(b64)) : NULL;
        }
        case 0xca: {
            if (!need(c, 4)) return NULL;
            uint32_t bits = (uint32_t)be(c, 4);
            float f;
            memcpy(&f, &bits, sizeof f);
            MpVal *v = new_val(c, MP_DBL);
            if (v) v->u.d = (double)f;
            return v;
        }
        case 0xcb: {
            if (!need(c, 8)) return NULL;
            uint64_t bits = be(c, 8);
            double d;
            memcpy(&d, &bits, sizeof d);
            MpVal *v = new_val(c, MP_DBL);
            if (v) v->u.d = d;
            return v;
        }
        case 0xcc: {
            if (!need(c, 1)) return NULL;
            MpVal *v = new_val(c, MP_INT);
            if (v) v->u.i = (int64_t)be(c, 1);
            return v;
        }
        case 0xcd: {
            if (!need(c, 2)) return NULL;
            MpVal *v = new_val(c, MP_INT);
            if (v) v->u.i = (int64_t)be(c, 2);
            return v;
        }
        case 0xce: {
            if (!need(c, 4)) return NULL;
            MpVal *v = new_val(c, MP_INT);
            if (v) v->u.i = (int64_t)be(c, 4);
            return v;
        }
        case 0xcf: {
            if (!need(c, 8)) return NULL;
            uint64_t u = be(c, 8);
            if (u <= (uint64_t)MP_SAFE_INT) {
                MpVal *v = new_val(c, MP_INT);
                if (v) v->u.i = (int64_t)u;
                return v;
            }
            char buf[24];
            snprintf(buf, sizeof buf, "%llu", (unsigned long long)u);
            char *s = arena_strdup(c->a, buf);
            return s ? mk_str(c, s, strlen(s)) : NULL;
        }
        case 0xd0: {
            if (!need(c, 1)) return NULL;
            MpVal *v = new_val(c, MP_INT);
            if (v) v->u.i = (int64_t)(int8_t)c->p[c->i];
            c->i++;
            return v;
        }
        case 0xd1: {
            if (!need(c, 2)) return NULL;
            MpVal *v = new_val(c, MP_INT);
            int16_t x = (int16_t)(uint16_t)be(c, 2);
            if (v) v->u.i = (int64_t)x;
            return v;
        }
        case 0xd2: {
            if (!need(c, 4)) return NULL;
            int32_t x = (int32_t)(uint32_t)be(c, 4);
            MpVal *v = new_val(c, MP_INT);
            if (v) v->u.i = (int64_t)x;
            return v;
        }
        case 0xd3: {
            if (!need(c, 8)) return NULL;
            int64_t x = (int64_t)be(c, 8);
            if (x >= -MP_SAFE_INT && x <= MP_SAFE_INT) {
                MpVal *v = new_val(c, MP_INT);
                if (v) v->u.i = x;
                return v;
            }
            char buf[24];
            snprintf(buf, sizeof buf, "%lld", (long long)x);
            char *s = arena_strdup(c->a, buf);
            return s ? mk_str(c, s, strlen(s)) : NULL;
        }
        case 0xd9: {
            if (!need(c, 1)) return NULL;
            uint32_t len = (uint32_t)be(c, 1);
            return str_ref(c, len);
        }
        case 0xda: {
            if (!need(c, 2)) return NULL;
            uint32_t len = (uint32_t)be(c, 2);
            return str_ref(c, len);
        }
        case 0xdb: {
            if (!need(c, 4)) return NULL;
            uint32_t len = (uint32_t)be(c, 4);
            return str_ref(c, len);
        }
        case 0xdc: {
            if (!need(c, 2)) return NULL;
            return decode_array(c, (uint32_t)be(c, 2), depth);
        }
        case 0xdd: {
            if (!need(c, 4)) return NULL;
            return decode_array(c, (uint32_t)be(c, 4), depth);
        }
        case 0xde: {
            if (!need(c, 2)) return NULL;
            return decode_map(c, (uint32_t)be(c, 2), depth);
        }
        case 0xdf: {
            if (!need(c, 4)) return NULL;
            return decode_map(c, (uint32_t)be(c, 4), depth);
        }
        default: {
            err_set(c->e, "不支持的 MessagePack 标记 0x%02x（偏移 %zu）", b, c->i - 1);
            c->failed = true;
            return NULL;
        }
    }
}

MpVal *mp_decode_all(Arena *a, const char *buf, size_t n, Err *e) {
    Cur c = {(const unsigned char *)buf, n, 0, a, e, false};
    MpVal *v = decode_val(&c, 0);
    if (!v) return NULL;
    if (c.i != n) {
        err_set(e, "MessagePack 解码未完整消费: %zu/%zu bytes", c.i, n);
        return NULL;
    }
    return v;
}

MpVal *mp_get(const MpVal *map, const char *key) {
    if (!map || map->t != MP_MAP || !key) return NULL;
    for (uint32_t i = 0; i < map->u.map.n; i++) {
        if (strcmp(map->u.map.keys[i], key) == 0) return map->u.map.vals[i];
    }
    return NULL;
}
