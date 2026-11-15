/* zstdwrap.h - thin wrapper over the vendored Zstandard decoder. */
#ifndef EUI_ZSTDWRAP_H
#define EUI_ZSTDWRAP_H

#include "util.h"

/* Decompresses a complete Zstandard frame (content size in header preferred,
 * streaming fallback). Returns an arena buffer of *out_len bytes. */
char *zstd_decompress(Arena *a, const void *src, size_t n, size_t *out_len, Err *e);

#endif /* EUI_ZSTDWRAP_H */
