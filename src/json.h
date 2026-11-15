/* json.h - ordered JSON value tree, serializer and minimal parser. */
#ifndef EUI_JSON_H
#define EUI_JSON_H

#include "util.h"

typedef enum JType { J_NULL, J_BOOL, J_INT, J_DBL, J_STR, J_ARR, J_OBJ } JType;

typedef struct J J;
struct J {
    JType t;
    union {
        bool b;
        int64_t i;
        double d;
        struct {
            const char *p;
            size_t n;
        } s;
        struct {
            J **items;
            size_t n, cap;
        } arr;
        struct {
            const char **keys;
            J **vals;
            size_t n, cap;
        } obj;
    } u;
};

J *j_null(Arena *a);
J *j_bool(Arena *a, bool v);
J *j_int(Arena *a, int64_t v);
J *j_dbl(Arena *a, double v);
J *j_str(Arena *a, const char *s);
J *j_strn(Arena *a, const char *s, size_t n);
J *j_arr(Arena *a);
J *j_obj(Arena *a);

void j_push(Arena *a, J *arr, J *item);
/* Appends a key/value pair; replaces the value when the key already exists. */
void j_set(Arena *a, J *obj, const char *key, J *val);
J *j_get(const J *obj, const char *key);
size_t j_len(const J *arr_or_obj);

/* ECMAScript Number#toString formatting (what JSON.stringify emits). */
void js_number_to_str(double v, char out[32]);

void json_write(SB *sb, const J *v, bool pretty);
J *json_parse(Arena *a, const char *s, size_t n, Err *e);

#endif /* EUI_JSON_H */
