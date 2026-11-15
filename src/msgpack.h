/* msgpack.h - MessagePack decoder producing an ordered value tree. */
#ifndef EUI_MSGPACK_H
#define EUI_MSGPACK_H

#include "util.h"

typedef enum MpType { MP_NULL, MP_BOOL, MP_INT, MP_DBL, MP_STR, MP_ARR, MP_MAP } MpType;

typedef struct MpVal MpVal;
struct MpVal {
    MpType t;
    union {
        bool b;
        int64_t i;
        double d;
        struct {
            const char *p;
            uint32_t n;
        } s;
        struct {
            MpVal **items;
            uint32_t n;
        } arr;
        struct {
            const char **keys;
            MpVal **vals;
            uint32_t n;
        } map;
    } u;
};

/* Decodes the whole buffer; fails when bytes remain unconsumed. */
MpVal *mp_decode_all(Arena *a, const char *buf, size_t n, Err *e);
/* Map lookup; NULL when the key is absent or the value is not a map. */
MpVal *mp_get(const MpVal *map, const char *key);

#endif /* EUI_MSGPACK_H */
