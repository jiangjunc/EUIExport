/* maps.h - draft discovery, naming and input resolution. */
#ifndef EUI_MAPS_H
#define EUI_MAPS_H

#include "util.h"

typedef struct MapRoots {
    char **items;
    size_t n;
} MapRoots;

typedef struct Draft {
    char *folder;
    char *dir;
    char *name;
    char *root;
} Draft;

typedef struct DraftList {
    Draft *items;
    size_t n;
} DraftList;

typedef struct FolderId {
    char *base64;
    char *hex;    /* "AA BB CC …" */
    char *raw_hex;/* "aabbcc…" */
} FolderId;

typedef struct ProjEntry {
    const char *id;
    const char *lua_root;
    const char *project_name; /* may be NULL */
    const char *vscode_name;  /* may be NULL */
    const char *map_dir;      /* eggy.json mapDir：工程当前绑定的草稿目录，可能为 NULL */
    int64_t server_port;      /* eggy.json serverPort：0 表示未配置 */
} ProjEntry;

typedef struct ProjIndex {
    ProjEntry *items;
    size_t n;
} ProjIndex;

typedef struct Resolve {
    bool ok;
    bool not_found;
    char *dir;
    char *folder;
    char *error;
} Resolve;

void maps_set_editor_root(Arena *a, const char *root);
char **maps_editor_roots(Arena *a, size_t *count);
MapRoots maps_find_map_roots(Arena *a);
char *maps_first_editor_maps_root(Arena *a);
bool maps_is_project_dir(const char *dir);
char **maps_list_projects(Arena *a, const char *root, size_t *count);
bool maps_parse_folder_id(Arena *a, const char *folder, FolderId *out);
bool maps_has_eui_file(const char *dir);
char *maps_eui_file(Arena *a, const char *dir); /* NULL when absent */
char *maps_read_map_name(Arena *a, const char *dir, const ProjIndex *idx);
DraftList maps_collect_drafts(Arena *a);
Resolve maps_resolve(Arena *a, const char *input, const char *cwd);
/* 找到草稿对应的 lua 工程目录。依次尝试：.gmp 里的工程 id → 工程 eggy.json
   的 mapDir → 工程 serverPort 与草稿 desc.mm 的 vscode_server_port → 草稿
   desc.mm 的 alias（复制来源）所指向的草稿 → 草稿名与工程名匹配。 */
char *maps_find_lua_project_dir(Arena *a, const char *dir, const char *map_name);

/* JS-like trim helpers shared by the CLI. */
char *js_trim(Arena *a, const char *s);

#endif /* EUI_MAPS_H */
