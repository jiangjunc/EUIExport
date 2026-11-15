#include "utf8.h"

#include <string.h>

/* Replace invalid input the same way Node's Buffer#toString('utf8') does:
 * each maximal invalid subsequence becomes one U+FFFD per invalid byte. */
static uint32_t decode_at(const unsigned char *s, size_t n, size_t *pos, bool *ok) {
    size_t i = *pos;
    unsigned char b = s[i];
    if (b < 0x80) {
        *pos = i + 1;
        if (ok) *ok = true;
        return b;
    }
    uint32_t cp = 0;
    size_t need = 0;
    if ((b & 0xE0) == 0xC0) {
        cp = b & 0x1F;
        need = 1;
    } else if ((b & 0xF0) == 0xE0) {
        cp = b & 0x0F;
        need = 2;
    } else if ((b & 0xF8) == 0xF0) {
        cp = b & 0x07;
        need = 3;
    } else {
        *pos = i + 1;
        if (ok) *ok = false;
        return UTF8_REPL;
    }
    for (size_t k = 1; k <= need; k++) {
        if (i + k >= n) {
            *pos = i + 1;
            if (ok) *ok = false;
            return UTF8_REPL;
        }
        unsigned char c = s[i + k];
        if ((c & 0xC0) != 0x80) {
            *pos = i + 1;
            if (ok) *ok = false;
            return UTF8_REPL;
        }
        cp = (cp << 6) | (uint32_t)(c & 0x3F);
    }
    /* overlong / surrogate / out of range checks */
    static const uint32_t min_cp[4] = {0, 0x80, 0x800, 0x10000};
    if (cp < min_cp[need] || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
        *pos = i + 1;
        if (ok) *ok = false;
        return UTF8_REPL;
    }
    *pos = i + need + 1;
    if (ok) *ok = true;
    return cp;
}

uint32_t utf8_next(const char *s, size_t n, size_t *pos) {
    if (*pos >= n) return 0;
    return decode_at((const unsigned char *)s, n, pos, NULL);
}

uint32_t utf8_decode1(const char *s, size_t n, size_t *pos, bool *ok) {
    return decode_at((const unsigned char *)s, n, pos, ok);
}

size_t utf8_count(const char *s, size_t n) {
    size_t pos = 0, count = 0;
    while (pos < n) {
        (void)decode_at((const unsigned char *)s, n, &pos, NULL);
        count++;
    }
    return count;
}

bool utf8_valid(const char *s, size_t n) {
    size_t pos = 0;
    while (pos < n) {
        bool ok = false;
        (void)decode_at((const unsigned char *)s, n, &pos, &ok);
        if (!ok) return false;
    }
    return true;
}

size_t utf8_encode(uint32_t cp, char out[4]) {
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

size_t utf8_to_cps(const char *s, size_t n, uint32_t *out, size_t out_cap) {
    size_t pos = 0, count = 0;
    while (pos < n) {
        uint32_t cp = decode_at((const unsigned char *)s, n, &pos, NULL);
        if (count < out_cap) out[count] = cp;
        count++;
    }
    return count;
}

int wcwidth_u(uint32_t cp) {
    if (cp == 0) return 0;
    if (cp < 32 || (cp >= 0x7F && cp < 0xA0)) return 0;
    if (cp >= 0x300 && cp <= 0x36F) return 0;   /* combining marks */
    if (cp >= 0x200B && cp <= 0x200F) return 0; /* zero width */
    if (cp >= 0xFE00 && cp <= 0xFE0F) return 0;
    if (cp == 0xFEFF) return 0;
    if ((cp >= 0x1100 && cp <= 0x115F) || cp == 0x2329 || cp == 0x232A ||
        (cp >= 0x2E80 && cp <= 0xA4CF && cp != 0x303F) ||
        (cp >= 0xA960 && cp <= 0xA97F) || (cp >= 0xAC00 && cp <= 0xD7A3) ||
        (cp >= 0xF900 && cp <= 0xFAFF) || (cp >= 0xFE10 && cp <= 0xFE19) ||
        (cp >= 0xFE30 && cp <= 0xFE6F) || (cp >= 0xFF00 && cp <= 0xFF60) ||
        (cp >= 0xFFE0 && cp <= 0xFFE6) || (cp >= 0x1F004 && cp <= 0x1F9FF) ||
        (cp >= 0x17000 && cp <= 0x18AFF) || (cp >= 0x1B000 && cp <= 0x1B2FF) ||
        (cp >= 0x20000 && cp <= 0x3FFFD)) {
        return 2;
    }
    return 1;
}

int display_width(const char *s, size_t n) {
    size_t pos = 0;
    int w = 0;
    while (pos < n) {
        w += wcwidth_u(decode_at((const unsigned char *)s, n, &pos, NULL));
    }
    return w;
}

static int b64_val(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

size_t base64_decode(const char *s, size_t n, unsigned char *out, size_t out_cap) {
    size_t o = 0;
    uint32_t acc = 0;
    int bits = 0;
    for (size_t i = 0; i < n; i++) {
        int c = (unsigned char)s[i];
        if (c == '\r' || c == '\n' || c == ' ' || c == '\t') continue;
        if (c == '=') break;
        int v = b64_val(c);
        if (v < 0) return (size_t)-1;
        acc = (acc << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (o >= out_cap) return (size_t)-1;
            out[o++] = (unsigned char)((acc >> bits) & 0xFF);
        }
    }
    return o;
}

char *base64_encode(Arena *a, const unsigned char *data, size_t n) {
    static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t out_n = ((n + 2) / 3) * 4;
    char *out = (char *)arena_alloc(a, out_n + 1);
    if (!out) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < n; i += 3) {
        unsigned int b0 = data[i];
        unsigned int b1 = (i + 1 < n) ? data[i + 1] : 0;
        unsigned int b2 = (i + 2 < n) ? data[i + 2] : 0;
        out[o++] = tbl[b0 >> 2];
        out[o++] = tbl[((b0 & 0x03) << 4) | (b1 >> 4)];
        out[o++] = (i + 1 < n) ? tbl[((b1 & 0x0F) << 2) | (b2 >> 6)] : '=';
        out[o++] = (i + 2 < n) ? tbl[b2 & 0x3F] : '=';
    }
    out[o] = '\0';
    return out;
}
