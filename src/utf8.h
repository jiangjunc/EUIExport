/* utf8.h - UTF-8 handling with JavaScript-compatible replacement semantics. */
#ifndef EUI_UTF8_H
#define EUI_UTF8_H

#include "util.h"

#define UTF8_REPL 0xFFFDu

/* Decode one code point. Invalid sequences yield 0xFFFD and consume 1 byte. */
uint32_t utf8_next(const char *s, size_t n, size_t *pos);
/* Like utf8_next, but reports whether the sequence was well formed. */
uint32_t utf8_decode1(const char *s, size_t n, size_t *pos, bool *ok);
/* Number of code points in [s, s+n). */
size_t utf8_count(const char *s, size_t n);
/* Strict check: valid UTF-8, no surrogates, no overlongs, no values > 0x10FFFF. */
bool utf8_valid(const char *s, size_t n);
/* Encode a code point into out (>= 4 bytes). Returns byte count. */
size_t utf8_encode(uint32_t cp, char out[4]);
/* Decode a byte range into code points. Returns count; caller supplies buffer. */
size_t utf8_to_cps(const char *s, size_t n, uint32_t *out, size_t out_cap);

/* Terminal display width of one code point (0, 1 or 2). */
int wcwidth_u(uint32_t cp);
/* Terminal display width of a UTF-8 string. */
int display_width(const char *s, size_t n);

/* Base64 decode. Accepts CR/LF. Caller provides exact-size output buffer.
 * Returns decoded byte count, or (size_t)-1 when the input is not decodable. */
size_t base64_decode(const char *s, size_t n, unsigned char *out, size_t out_cap);
/* Standard base64 with padding; returns an arena string. */
char *base64_encode(Arena *a, const unsigned char *data, size_t n);

#endif /* EUI_UTF8_H */
