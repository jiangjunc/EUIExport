#include "zstdwrap.h"

#include <stdlib.h>
#include <string.h>

#include "zstd.h"

char *zstd_decompress(Arena *a, const void *src, size_t n, size_t *out_len, Err *e) {
    unsigned long long hint = ZSTD_getFrameContentSize(src, n);
    if (hint == ZSTD_CONTENTSIZE_ERROR) {
        err_set(e, "不是有效的 Zstandard 数据");
        return NULL;
    }
    if (hint != ZSTD_CONTENTSIZE_UNKNOWN && hint > 0) {
        char *buf = (char *)arena_alloc(a, (size_t)hint + 1);
        if (!buf) {
            err_set(e, "内存不足（需要 %llu 字节）", hint);
            return NULL;
        }
        size_t got = ZSTD_decompress(buf, (size_t)hint, src, n);
        if (ZSTD_isError(got)) {
            err_set(e, "Zstandard 解压失败：%s", ZSTD_getErrorName(got));
            return NULL;
        }
        buf[got] = '\0';
        *out_len = got;
        return buf;
    }

    ZSTD_DStream *ds = ZSTD_createDStream();
    if (!ds) {
        err_set(e, "内存不足（无法创建解压上下文）");
        return NULL;
    }
    size_t rc = ZSTD_initDStream(ds);
    if (ZSTD_isError(rc)) {
        err_set(e, "Zstandard 初始化失败：%s", ZSTD_getErrorName(rc));
        ZSTD_freeDStream(ds);
        return NULL;
    }
    size_t cap = 256 * 1024;
    char *buf = (char *)arena_alloc(a, cap + 1);
    if (!buf) {
        ZSTD_freeDStream(ds);
        err_set(e, "内存不足");
        return NULL;
    }
    ZSTD_inBuffer in = {src, n, 0};
    size_t total = 0;
    for (;;) {
        ZSTD_outBuffer out = {buf + total, cap - total, 0};
        rc = ZSTD_decompressStream(ds, &out, &in);
        if (ZSTD_isError(rc)) {
            err_set(e, "Zstandard 解压失败：%s", ZSTD_getErrorName(rc));
            ZSTD_freeDStream(ds);
            return NULL;
        }
        total += out.pos;
        if (rc == 0 && in.pos == in.size) break;
        if (out.pos == 0 && in.pos == in.size) {
            err_set(e, "Zstandard 数据不完整");
            ZSTD_freeDStream(ds);
            return NULL;
        }
        if (total == cap) {
            size_t ncap = cap * 2;
            char *nbuf = (char *)arena_grow(a, buf, cap + 1, ncap + 1);
            if (!nbuf) {
                ZSTD_freeDStream(ds);
                err_set(e, "内存不足");
                return NULL;
            }
            buf = nbuf;
            cap = ncap;
        }
    }
    ZSTD_freeDStream(ds);
    buf[total] = '\0';
    *out_len = total;
    return buf;
}
