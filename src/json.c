#include "json.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utf8.h"

static J *j_new(Arena *a, JType t) {
    J *v = (J *)arena_alloc(a, sizeof(J));
    if (!v) return NULL;
    memset(v, 0, sizeof(J));
    v->t = t;
    return v;
}

J *j_null(Arena *a) { return j_new(a, J_NULL); }

J *j_bool(Arena *a, bool val) {
    J *v = j_new(a, J_BOOL);
    if (v) v->u.b = val;
    return v;
}

J *j_int(Arena *a, int64_t val) {
    J *v = j_new(a, J_INT);
    if (v) v->u.i = val;
    return v;
}

J *j_dbl(Arena *a, double val) {
    J *v = j_new(a, J_DBL);
    if (v) v->u.d = val;
    return v;
}

J *j_strn(Arena *a, const char *s, size_t n) {
    J *v = j_new(a, J_STR);
    if (!v) return NULL;
    if (!s) {
        v->u.s.p = "";
        v->u.s.n = 0;
        return v;
    }
    /* Copy so every J string is NUL terminated; callers print them with %s. */
    char *dup = (char *)arena_alloc(a, n + 1);
    if (!dup) {
        v->u.s.p = "";
        v->u.s.n = 0;
        return v;
    }
    memcpy(dup, s, n);
    dup[n] = '\0';
    v->u.s.p = dup;
    v->u.s.n = n;
    return v;
}

J *j_str(Arena *a, const char *s) { return j_strn(a, s, s ? strlen(s) : 0); }

J *j_arr(Arena *a) { return j_new(a, J_ARR); }
J *j_obj(Arena *a) { return j_new(a, J_OBJ); }

void j_push(Arena *a, J *arr, J *item) {
    if (!arr || arr->t != J_ARR) return;
    if (arr->u.arr.n == arr->u.arr.cap) {
        size_t ncap = arr->u.arr.cap ? arr->u.arr.cap * 2 : 8;
        arr->u.arr.items = (J **)arena_grow(a, arr->u.arr.items, arr->u.arr.cap * sizeof(J *),
                                            ncap * sizeof(J *));
        arr->u.arr.cap = ncap;
    }
    arr->u.arr.items[arr->u.arr.n++] = item;
}

void j_set(Arena *a, J *obj, const char *key, J *val) {
    if (!obj || obj->t != J_OBJ) return;
    for (size_t i = 0; i < obj->u.obj.n; i++) {
        if (strcmp(obj->u.obj.keys[i], key) == 0) {
            obj->u.obj.vals[i] = val;
            return;
        }
    }
    if (obj->u.obj.n == obj->u.obj.cap) {
        size_t ncap = obj->u.obj.cap ? obj->u.obj.cap * 2 : 8;
        const char **nk = (const char **)arena_alloc(a, ncap * sizeof(char *));
        J **nv = (J **)arena_alloc(a, ncap * sizeof(J *));
        if (!nk || !nv) return;
        if (obj->u.obj.n) {
            memcpy(nk, obj->u.obj.keys, obj->u.obj.n * sizeof(char *));
            memcpy(nv, obj->u.obj.vals, obj->u.obj.n * sizeof(J *));
        }
        obj->u.obj.keys = nk;
        obj->u.obj.vals = nv;
        obj->u.obj.cap = ncap;
    }
    obj->u.obj.keys[obj->u.obj.n] = key;
    obj->u.obj.vals[obj->u.obj.n] = val;
    obj->u.obj.n++;
}

J *j_get(const J *obj, const char *key) {
    if (!obj || obj->t != J_OBJ) return NULL;
    for (size_t i = 0; i < obj->u.obj.n; i++) {
        if (strcmp(obj->u.obj.keys[i], key) == 0) return obj->u.obj.vals[i];
    }
    return NULL;
}

size_t j_len(const J *v) {
    if (!v) return 0;
    if (v->t == J_ARR) return v->u.arr.n;
    if (v->t == J_OBJ) return v->u.obj.n;
    if (v->t == J_STR) return v->u.s.n;
    return 0;
}

/* ------------------------------------------------------------------ */
/* number formatting                                                   */
/* ------------------------------------------------------------------ */
void js_number_to_str(double v, char out[32]) {
    if (isnan(v) || isinf(v)) {
        strcpy(out, "null");
        return;
    }
    if (v == 0) {
        strcpy(out, "0");
        return;
    }
    if (v == floor(v) && fabs(v) < 1e21) {
        snprintf(out, 32, "%.0f", v);
        return;
    }
    /* shortest representation that round-trips */
    char buf[64];
    int prec = 17;
    for (int p = 1; p <= 17; p++) {
        snprintf(buf, sizeof(buf), "%.*e", p - 1, v);
        if (strtod(buf, NULL) == v) {
            prec = p;
            break;
        }
    }
    (void)prec;
    /* buf looks like "[-]d[.ddd]e[+-]XX" */
    const char *s = buf;
    bool neg = false;
    if (*s == '-') {
        neg = true;
        s++;
    }
    char digits[32];
    int k = 0;
    while (*s && *s != 'e' && *s != 'E') {
        if (*s >= '0' && *s <= '9') digits[k++] = *s;
        s++;
    }
    int exp10 = 0;
    if (*s == 'e' || *s == 'E') exp10 = atoi(s + 1);
    while (k > 1 && digits[k - 1] == '0') k--;
    digits[k] = '\0';
    int n = exp10 + 1; /* decimal point position */

    char *o = out;
    if (neg) *o++ = '-';
    if (k <= n && n <= 21) {
        memcpy(o, digits, (size_t)k);
        o += k;
        for (int i = 0; i < n - k; i++) *o++ = '0';
        *o = '\0';
    } else if (0 < n && n <= 21) {
        memcpy(o, digits, (size_t)n);
        o += n;
        *o++ = '.';
        memcpy(o, digits + n, (size_t)(k - n));
        o += k - n;
        *o = '\0';
    } else if (-6 < n && n <= 0) {
        *o++ = '0';
        *o++ = '.';
        for (int i = 0; i < -n; i++) *o++ = '0';
        memcpy(o, digits, (size_t)k);
        o += k;
        *o = '\0';
    } else {
        *o++ = digits[0];
        if (k > 1) {
            *o++ = '.';
            memcpy(o, digits + 1, (size_t)(k - 1));
            o += k - 1;
        }
        *o++ = 'e';
        int e = n - 1;
        *o++ = e >= 0 ? '+' : '-';
        int ae = e >= 0 ? e : -e;
        char eb[16];
        snprintf(eb, sizeof eb, "%d", ae);
        strcpy(o, eb);
    }
}

/* ------------------------------------------------------------------ */
/* writer                                                              */
/* ------------------------------------------------------------------ */
static void write_string(SB *sb, const char *s, size_t n) {
    sb_putc(sb, '"');
    size_t i = 0;
    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80) {
            switch (c) {
                case '"': sb_puts(sb, "\\\""); break;
                case '\\': sb_puts(sb, "\\\\"); break;
                case '\b': sb_puts(sb, "\\b"); break;
                case '\f': sb_puts(sb, "\\f"); break;
                case '\n': sb_puts(sb, "\\n"); break;
                case '\r': sb_puts(sb, "\\r"); break;
                case '\t': sb_puts(sb, "\\t"); break;
                default:
                    if (c < 0x20) {
                        char esc[8];
                        snprintf(esc, sizeof esc, "\\u%04x", c);
                        sb_puts(sb, esc);
                    } else {
                        sb_putc(sb, (char)c);
                    }
            }
            i++;
            continue;
        }
        size_t pos = i;
        bool ok = false;
        uint32_t cp = utf8_decode1(s, n, &pos, &ok);
        if (!ok) {
            sb_write(sb, "\xEF\xBF\xBD", 3); /* U+FFFD, matching Buffer#toString */
            i = pos;
            continue;
        }
        (void)cp;
        sb_write(sb, s + i, pos - i);
        i = pos;
    }
    sb_putc(sb, '"');
}

static void indent(SB *sb, int depth) {
    sb_putc(sb, '\n');
    sb_repeat(sb, ' ', (size_t)depth * 2);
}

static void write_value(SB *sb, const J *v, bool pretty, int depth) {
    if (!v) {
        sb_puts(sb, "null");
        return;
    }
    switch (v->t) {
        case J_NULL: sb_puts(sb, "null"); break;
        case J_BOOL: sb_puts(sb, v->u.b ? "true" : "false"); break;
        case J_INT: {
            char buf[24];
            snprintf(buf, sizeof buf, "%lld", (long long)v->u.i);
            sb_puts(sb, buf);
            break;
        }
        case J_DBL: {
            char buf[32];
            js_number_to_str(v->u.d, buf);
            sb_puts(sb, buf);
            break;
        }
        case J_STR: write_string(sb, v->u.s.p, v->u.s.n); break;
        case J_ARR: {
            if (v->u.arr.n == 0) {
                sb_puts(sb, "[]");
                break;
            }
            sb_putc(sb, '[');
            for (size_t i = 0; i < v->u.arr.n; i++) {
                if (i) sb_putc(sb, ',');
                if (pretty) indent(sb, depth + 1);
                write_value(sb, v->u.arr.items[i], pretty, depth + 1);
            }
            if (pretty) indent(sb, depth);
            sb_putc(sb, ']');
            break;
        }
        case J_OBJ: {
            if (v->u.obj.n == 0) {
                sb_puts(sb, "{}");
                break;
            }
            sb_putc(sb, '{');
            for (size_t i = 0; i < v->u.obj.n; i++) {
                if (i) sb_putc(sb, ',');
                if (pretty) indent(sb, depth + 1);
                const char *key = v->u.obj.keys[i];
                write_string(sb, key, strlen(key));
                sb_puts(sb, pretty ? ": " : ":");
                write_value(sb, v->u.obj.vals[i], pretty, depth + 1);
            }
            if (pretty) indent(sb, depth);
            sb_putc(sb, '}');
            break;
        }
    }
}

void json_write(SB *sb, const J *v, bool pretty) { write_value(sb, v, pretty, 0); }

/* ------------------------------------------------------------------ */
/* parser (config files)                                               */
/* ------------------------------------------------------------------ */
typedef struct P {
    const char *s;
    size_t n;
    size_t i;
    Err *e;
} P;

static void skip_ws(P *p) {
    while (p->i < p->n) {
        char c = p->s[p->i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            p->i++;
        } else {
            break;
        }
    }
}

static J *parse_value(Arena *a, P *p, int depth);

static bool parse_hex4(P *p, uint32_t *out) {
    if (p->i + 4 > p->n) return false;
    uint32_t v = 0;
    for (int k = 0; k < 4; k++) {
        char c = p->s[p->i + (size_t)k];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (uint32_t)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (uint32_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (uint32_t)(c - 'A' + 10);
        else return false;
    }
    p->i += 4;
    *out = v;
    return true;
}

static char *parse_string_raw(Arena *a, P *p) {
    if (p->i >= p->n || p->s[p->i] != '"') {
        err_set(p->e, "JSON 解析失败：期望字符串");
        return NULL;
    }
    p->i++;
    SB sb;
    sb_init(&sb);
    while (p->i < p->n) {
        char c = p->s[p->i++];
        if (c == '"') {
            char *out = arena_strndup(a, sb_cstr(&sb), sb.n);
            sb_free(&sb);
            return out;
        }
        if (c != '\\') {
            sb_putc(&sb, c);
            continue;
        }
        if (p->i >= p->n) break;
        char esc = p->s[p->i++];
        switch (esc) {
            case '"': sb_putc(&sb, '"'); break;
            case '\\': sb_putc(&sb, '\\'); break;
            case '/': sb_putc(&sb, '/'); break;
            case 'b': sb_putc(&sb, '\b'); break;
            case 'f': sb_putc(&sb, '\f'); break;
            case 'n': sb_putc(&sb, '\n'); break;
            case 'r': sb_putc(&sb, '\r'); break;
            case 't': sb_putc(&sb, '\t'); break;
            case 'u': {
                uint32_t cp = 0;
                if (!parse_hex4(p, &cp)) {
                    err_set(p->e, "JSON 解析失败：非法 \\u 转义");
                    sb_free(&sb);
                    return NULL;
                }
                if (cp >= 0xD800 && cp <= 0xDBFF && p->i + 6 <= p->n && p->s[p->i] == '\\' &&
                    p->s[p->i + 1] == 'u') {
                    size_t save = p->i;
                    p->i += 2;
                    uint32_t lo = 0;
                    if (parse_hex4(p, &lo) && lo >= 0xDC00 && lo <= 0xDFFF) {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    } else {
                        p->i = save;
                        cp = UTF8_REPL;
                    }
                } else if (cp >= 0xD800 && cp <= 0xDFFF) {
                    cp = UTF8_REPL;
                }
                char enc[4];
                size_t len = utf8_encode(cp, enc);
                sb_write(&sb, enc, len);
                break;
            }
            default:
                err_set(p->e, "JSON 解析失败：非法转义 \\%c", esc);
                sb_free(&sb);
                return NULL;
        }
    }
    err_set(p->e, "JSON 解析失败：字符串未闭合");
    sb_free(&sb);
    return NULL;
}

static J *parse_value(Arena *a, P *p, int depth) {
    if (depth > 64) {
        err_set(p->e, "JSON 嵌套过深");
        return NULL;
    }
    skip_ws(p);
    if (p->i >= p->n) {
        err_set(p->e, "JSON 解析失败：数据意外结束");
        return NULL;
    }
    char c = p->s[p->i];
    if (c == '{') {
        p->i++;
        J *obj = j_obj(a);
        skip_ws(p);
        if (p->i < p->n && p->s[p->i] == '}') {
            p->i++;
            return obj;
        }
        for (;;) {
            skip_ws(p);
            char *key = parse_string_raw(a, p);
            if (!key) return NULL;
            skip_ws(p);
            if (p->i >= p->n || p->s[p->i] != ':') {
                err_set(p->e, "JSON 解析失败：期望 ':'");
                return NULL;
            }
            p->i++;
            J *val = parse_value(a, p, depth + 1);
            if (!val) return NULL;
            j_set(a, obj, key, val);
            skip_ws(p);
            if (p->i < p->n && p->s[p->i] == ',') {
                p->i++;
                continue;
            }
            if (p->i < p->n && p->s[p->i] == '}') {
                p->i++;
                return obj;
            }
            err_set(p->e, "JSON 解析失败：期望 ',' 或 '}'");
            return NULL;
        }
    }
    if (c == '[') {
        p->i++;
        J *arr = j_arr(a);
        skip_ws(p);
        if (p->i < p->n && p->s[p->i] == ']') {
            p->i++;
            return arr;
        }
        for (;;) {
            J *val = parse_value(a, p, depth + 1);
            if (!val) return NULL;
            j_push(a, arr, val);
            skip_ws(p);
            if (p->i < p->n && p->s[p->i] == ',') {
                p->i++;
                continue;
            }
            if (p->i < p->n && p->s[p->i] == ']') {
                p->i++;
                return arr;
            }
            err_set(p->e, "JSON 解析失败：期望 ',' 或 ']'");
            return NULL;
        }
    }
    if (c == '"') {
        char *s = parse_string_raw(a, p);
        if (!s) return NULL;
        return j_str(a, s);
    }
    if (c == 't' && p->i + 4 <= p->n && memcmp(p->s + p->i, "true", 4) == 0) {
        p->i += 4;
        return j_bool(a, true);
    }
    if (c == 'f' && p->i + 5 <= p->n && memcmp(p->s + p->i, "false", 5) == 0) {
        p->i += 5;
        return j_bool(a, false);
    }
    if (c == 'n' && p->i + 4 <= p->n && memcmp(p->s + p->i, "null", 4) == 0) {
        p->i += 4;
        return j_null(a);
    }
    {
        size_t start = p->i;
        if (c == '-' || c == '+') p->i++;
        bool is_int = true;
        while (p->i < p->n) {
            char d = p->s[p->i];
            if ((d >= '0' && d <= '9')) {
                p->i++;
            } else if (d == '.' || d == 'e' || d == 'E' || d == '+' || d == '-') {
                is_int = false;
                p->i++;
            } else {
                break;
            }
        }
        if (p->i == start) {
            err_set(p->e, "JSON 解析失败：非法字符 '%c'", c);
            return NULL;
        }
        char *tmp = arena_strndup(a, p->s + start, p->i - start);
        if (is_int) {
            errno = 0;
            long long v = strtoll(tmp, NULL, 10);
            if (errno == 0) return j_int(a, (int64_t)v);
        }
        return j_dbl(a, strtod(tmp, NULL));
    }
}

J *json_parse(Arena *a, const char *s, size_t n, Err *e) {
    P p = {s, n, 0, e};
    J *v = parse_value(a, &p, 0);
    if (!v) return NULL;
    skip_ws(&p);
    if (p.i != p.n) {
        err_set(e, "JSON 解析失败：末尾存在多余内容");
        return NULL;
    }
    return v;
}
