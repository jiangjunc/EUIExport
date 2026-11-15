/* mdout.h - generated README.md and UITree.md documents. */
#ifndef EUI_MDOUT_H
#define EUI_MDOUT_H

#include "eui.h"
#include "json.h"
#include "util.h"

/* ISO-8601 UTC timestamp with milliseconds, e.g. 2026-11-15T02:44:32.634Z */
char *now_iso8601(Arena *a);

void md_build_readme(SB *sb, const char *map_name, const char *folder, const char *source_file,
                     const EuiStats *stats, bool has_node_index, const char *now_iso);

/* Complete UITree.md (header + fenced tree). tree_lines receives the tree row count. */
void md_build_tree(SB *sb, const char *map_name, const EuiStats *stats, const J *scene,
                   size_t *tree_lines);

#endif /* EUI_MDOUT_H */
