/* eui.h - EUI node model: readable-tree export, statistics, tree indexes. */
#ifndef EUI_EUI_H
#define EUI_EUI_H

#include "json.h"
#include "msgpack.h"
#include "util.h"

typedef struct EuiStats {
    int64_t count;
    int64_t canvas_count;
    int64_t max_depth;
    int64_t joystick_count;
    J *type_counts; /* ordered by first appearance */
} EuiStats;

typedef struct EuiType {
    int64_t code;
    const char *name;
    const char *zh;
} EuiType;

/* type code -> readable name/meaning (NULL when unknown). */
const EuiType *eui_type_lookup(int64_t code);
/* All mapped types, sorted ascending by code. */
const EuiType *eui_type_table(size_t *count);

/* Uncompressed scene root -> readable scene object (throws via Err). */
J *eui_export_scene(Arena *a, const MpVal *root, Err *e);
void eui_count_nodes(Arena *a, const J *scene, EuiStats *out);
void eui_tree_markdown(SB *sb, const J *scene, size_t *line_count);
/* id -> {name, type, type_zh, path}, ordered the way JSON.stringify would. */
J *eui_build_node_index(Arena *a, const J *scene);

J *j_from_mp(Arena *a, const MpVal *v);

#endif /* EUI_EUI_H */
