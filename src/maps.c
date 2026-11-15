#include "maps.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "fs.h"
#include "json.h"
#include "msgpack.h"
#include "utf8.h"
#include "zstdwrap.h"

static const char *kMapSubdirs[] = {
    "editor_maps",
    "help_build_gmps",
    "joint_construction_gmps",
    "se_maps",
};

static char *g_forced_root = NULL;

void maps_set_editor_root(Arena *a, const char *root) {
    g_forced_root = NULL;
    if (root && *root) g_forced_root = path_resolve(a, fs_cwd(a), root);
}

/* ------------------------------------------------------------------ */
/* whitespace (JS String#trim set)                                     */
/* ------------------------------------------------------------------ */
static bool cp_is_space(uint32_t cp) {
    return cp == 0x09 || cp == 0x0A || cp == 0x0B || cp == 0x0C || cp == 0x0D || cp == 0x20 ||
           cp == 0xA0 || cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200A) || cp == 0x2028 ||
           cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000 || cp == 0xFEFF;
}

char *js_trim(Arena *a, const char *s) {
    if (!s) return arena_strdup(a, "");
    size_t n = strlen(s);
    size_t start = 0, end = n;
    /* trim from the front */
    while (start < n) {
        size_t pos = start;
        uint32_t cp = utf8_decode1(s, n, &pos, NULL);
        if (!cp_is_space(cp)) break;
        start = pos;
    }
    while (end > start) {
        /* find the start of the last code point */
        size_t k = end - 1;
        while (k > start && ((unsigned char)s[k] & 0xC0) == 0x80) k--;
        size_t pos = k;
        uint32_t cp = utf8_decode1(s, n, &pos, NULL);
        if (!cp_is_space(cp)) break;
        end = k;
    }
    return arena_strndup(a, s + start, end - start);
}

/* ------------------------------------------------------------------ */
/* roots                                                               */
/* ------------------------------------------------------------------ */
char **maps_editor_roots(Arena *a, size_t *count) {
    char **out = (char **)arena_alloc(a, 8 * sizeof(char *));
    size_t n = 0;
    if (g_forced_root) out[n++] = arena_strdup(a, g_forced_root);

    Err e = {{0}, false};
    Config cfg;
    config_load(&cfg, a, &e);
    const char *cfg_root = config_get_str(&cfg, "editorRoot");
    if (cfg_root && *cfg_root) {
        char *r = path_resolve(a, fs_cwd(a), cfg_root);
        bool dup = false;
        for (size_t i = 0; i < n; i++) {
            if (str_eq_ci(out[i], r)) dup = true;
        }
        if (!dup) out[n++] = r;
    }
    {
        char *cwd = fs_cwd(a);
        bool dup = false;
        for (size_t i = 0; i < n; i++) {
            if (str_eq_ci(out[i], cwd)) dup = true;
        }
        if (!dup) out[n++] = cwd;
    }
    {
        char *p = path_join3(a, fs_home(a), ".eggitor", "cli");
        p = path_join(a, p, "editor_config.json");
        if (fs_is_file(p)) {
            size_t len = 0;
            Err le = {{0}, false};
            char *raw = fs_read_all(a, p, &len, &le);
            J *j = raw ? json_parse(a, raw, len, &le) : NULL;
            if (j && j->t == J_OBJ) {
                J *pd = j_get(j, "project_dir");
                J *ce = j_get(j, "client_exe");
                if (pd && pd->t == J_STR && pd->u.s.n) {
                    bool dup = false;
                    for (size_t i = 0; i < n; i++) {
                        if (str_eq_ci(out[i], pd->u.s.p)) dup = true;
                    }
                    if (!dup) out[n++] = arena_strndup(a, pd->u.s.p, pd->u.s.n);
                }
                if (ce && ce->t == J_STR && ce->u.s.n) {
                    char *dir = path_dirname(a, ce->u.s.p);
                    bool dup = false;
                    for (size_t i = 0; i < n; i++) {
                        if (str_eq_ci(out[i], dir)) dup = true;
                    }
                    if (!dup) out[n++] = dir;
                }
            }
        }
    }
    *count = n;
    return out;
}

MapRoots maps_find_map_roots(Arena *a) {
    MapRoots out = {0};
    size_t root_n = 0;
    char **roots = maps_editor_roots(a, &root_n);
    size_t cap = 16;
    char **items = (char **)arena_alloc(a, cap * sizeof(char *));
    for (size_t r = 0; r < root_n; r++) {
        for (size_t s = 0; s < sizeof(kMapSubdirs) / sizeof(kMapSubdirs[0]); s++) {
            const char *sub = kMapSubdirs[s];
            char *bases[3];
            bases[0] = path_join3(a, "Documents", "etc", sub);
            bases[1] = path_join(a, "etc", sub);
            bases[2] = arena_strdup(a, sub);
            for (int b = 0; b < 3; b++) {
                char *p = path_resolve(a, roots[r], bases[b]);
                if (!fs_is_dir(p)) continue;
                bool dup = false;
                for (size_t i = 0; i < out.n; i++) {
                    if (str_eq_ci(items[i], p)) dup = true;
                }
                if (dup) continue;
                if (out.n == cap) {
                    size_t ncap = cap * 2;
                    char **ni = (char **)arena_alloc(a, ncap * sizeof(char *));
                    if (!ni) break;
                    memcpy(ni, items, out.n * sizeof(char *));
                    items = ni;
                    cap = ncap;
                }
                items[out.n++] = p;
            }
        }
    }
    out.items = items;
    return out;
}

char *maps_first_editor_maps_root(Arena *a) {
    MapRoots roots = maps_find_map_roots(a);
    for (size_t i = 0; i < roots.n; i++) {
        if (path_ends_with_ci(roots.items[i], "editor_maps")) return roots.items[i];
    }
    return roots.n ? roots.items[0] : NULL;
}

/* ------------------------------------------------------------------ */
/* projects                                                            */
/* ------------------------------------------------------------------ */
bool maps_is_project_dir(const char *dir) {
    if (!fs_is_dir(dir)) return false;
    Arena a;
    arena_init(&a);
    bool ok = fs_exists(path_join(&a, dir, "desc.mm")) || fs_exists(path_join(&a, dir, "header.map"));
    arena_free(&a);
    return ok;
}

char **maps_list_projects(Arena *a, const char *root, size_t *count) {
    *count = 0;
    if (!root) return NULL;
    DirList dl;
    Err e = {{0}, false};
    if (!fs_list_dir(a, root, &dl, &e)) return NULL;
    char **out = (char **)arena_alloc(a, (dl.n + 1) * sizeof(char *));
    size_t n = 0;
    for (size_t i = 0; i < dl.n; i++) {
        if (!dl.items[i].is_dir) continue;
        char *p = path_join(a, root, dl.items[i].name);
        if (!maps_is_project_dir(p)) continue;
        out[n++] = dl.items[i].name;
    }
    *count = n;
    return out;
}

bool maps_has_eui_file(const char *dir) {
    Arena a;
    arena_init(&a);
    bool ok = fs_exists(path_join(&a, dir, "eui.mm")) || fs_exists(path_join(&a, dir, "euidata.mm"));
    arena_free(&a);
    return ok;
}

char *maps_eui_file(Arena *a, const char *dir) {
    char *p = path_join(a, dir, "eui.mm");
    if (fs_is_file(p)) return p;
    p = path_join(a, dir, "euidata.mm");
    if (fs_is_file(p)) return p;
    return NULL;
}

static bool b64_char(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '+' ||
           c == '/' || c == '=';
}

bool maps_parse_folder_id(Arena *a, const char *folder, FolderId *out) {
    if (!folder) return false;
    size_t n = strlen(folder);
    const char *b64 = NULL;
    size_t b64_n = 0;
    if (n > 4 && path_ends_with_ci(folder, "__pc")) {
        size_t k = n - 4;
        bool ok = k > 0;
        for (size_t i = 0; i < k; i++) {
            if (!b64_char(folder[i])) ok = false;
        }
        if (ok) {
            b64 = folder;
            b64_n = k;
        }
    }
    if (!b64 && n >= 16 && n <= 40) {
        bool ok = true;
        for (size_t i = 0; i < n; i++) {
            if (!b64_char(folder[i])) ok = false;
        }
        if (ok) {
            b64 = folder;
            b64_n = n;
        }
    }
    if (!b64) return false;

    size_t cap = b64_n / 4 * 3 + 4;
    unsigned char *buf = (unsigned char *)arena_alloc(a, cap);
    if (!buf) return false;
    size_t got = base64_decode(b64, b64_n, buf, cap);
    if (got == (size_t)-1 || got != 16) return false;

    static const char *hexd = "0123456789abcdef";
    char raw[33];
    char pretty[48];
    size_t rp = 0;
    for (size_t i = 0; i < 16; i++) {
        raw[i * 2] = hexd[buf[i] >> 4];
        raw[i * 2 + 1] = hexd[buf[i] & 0x0F];
    }
    raw[32] = '\0';
    for (size_t i = 0; i < 16; i++) {
        if (i) pretty[rp++] = ' ';
        char hi = (char)((buf[i] >> 4) < 10 ? '0' + (buf[i] >> 4) : 'A' + (buf[i] >> 4) - 10);
        char lo = (char)((buf[i] & 0x0F) < 10 ? '0' + (buf[i] & 0x0F) : 'A' + (buf[i] & 0x0F) - 10);
        pretty[rp++] = hi;
        pretty[rp++] = lo;
    }
    pretty[rp] = '\0';

    out->base64 = arena_strndup(a, b64, b64_n);
    out->hex = arena_strdup(a, pretty);
    out->raw_hex = arena_strdup(a, raw);
    return true;
}

static char *read_project_id_from_gmp(Arena *a, const char *dir) {
    DirList dl;
    Err e = {{0}, false};
    if (!fs_list_dir(a, dir, &dl, &e)) return NULL;
    const char *gmp = NULL;
    for (size_t i = 0; i < dl.n; i++) {
        if (!dl.items[i].is_dir && path_ends_with_ci(dl.items[i].name, ".gmp")) {
            gmp = dl.items[i].name;
            break;
        }
    }
    if (!gmp) return NULL;
    char *p = path_join(a, dir, gmp);
    size_t n = 0;
    Err le = {{0}, false};
    char *raw = fs_read_all(a, p, &n, &le);
    if (!raw || n < 18) return NULL;
    if ((unsigned char)raw[0] != 0x44 || (unsigned char)raw[1] != 0x5a) return NULL;
    static const char *hexd = "0123456789abcdef";
    char *out = (char *)arena_alloc(a, 25);
    if (!out) return NULL;
    for (size_t i = 0; i < 12; i++) {
        unsigned char b = (unsigned char)raw[6 + i];
        out[i * 2] = hexd[b >> 4];
        out[i * 2 + 1] = hexd[b & 0x0F];
    }
    out[24] = '\0';
    return out;
}

/* ------------------------------------------------------------------ */
/* ids                                                                 */
/* ------------------------------------------------------------------ */
static char *bytes_to_hex(Arena *a, const unsigned char *p, size_t n) {
    static const char *hexd = "0123456789abcdef";
    char *out = (char *)arena_alloc(a, n * 2 + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) {
        out[i * 2] = hexd[p[i] >> 4];
        out[i * 2 + 1] = hexd[p[i] & 0x0F];
    }
    out[n * 2] = '\0';
    return out;
}

static bool is_hex_str(const char *s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (!hex) return false;
    }
    return n > 0;
}

/* 12 字节地图 uuid 的 16 字符 base64（desc.mm 的 alias 即此格式）→ 24 位 hex */
static char *uuid16_to_hex(Arena *a, const char *s) {
    if (!s || strlen(s) != 16) return NULL;
    unsigned char raw[12];
    if (base64_decode(s, 16, raw, sizeof(raw)) != 12) return NULL;
    return bytes_to_hex(a, raw, 12);
}

/* 草稿目录名 → 24 位 hex 地图 uuid。历史上存在两种编码：
   · base64(24 位 hex 文本)   —— 如 NmE5MTgx…__pc
   · base64(16 字符 uuid)     —— 如 YXR1cUxibGt3…__pc
   另外还有直接以 hex 命名的目录。全部归一化到 24 位 hex。 */
static char *folder_uuid_hex(Arena *a, const char *folder) {
    if (!folder) return NULL;
    size_t n = strlen(folder);
    if (n > 4 && path_ends_with_ci(folder, "__pc")) n -= 4;
    if (n == 24 && is_hex_str(folder, n)) return str_lower_dup(a, folder);
    char *head = arena_strndup(a, folder, n);
    unsigned char buf[64];
    size_t got = base64_decode(head, n, buf, sizeof(buf));
    if (got == (size_t)-1) return NULL;
    if (got == 24 && is_hex_str((const char *)buf, got)) {
        char *txt = arena_strndup(a, (const char *)buf, got);
        return str_lower_dup(a, txt);
    }
    if (got == 16) {
        char txt[17];
        memcpy(txt, buf, 16);
        txt[16] = '\0';
        return uuid16_to_hex(a, txt);
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* desc.mm                                                             */
/* ------------------------------------------------------------------ */
static MpVal *read_desc_map(Arena *a, const char *dir) {
    char *desc = path_join(a, dir, "desc.mm");
    if (!fs_is_file(desc)) return NULL;
    size_t n = 0;
    Err e = {{0}, false};
    char *raw = fs_read_all(a, desc, &n, &e);
    if (!raw) return NULL;
    size_t out_n = 0;
    char *plain = zstd_decompress(a, raw, n, &out_n, &e);
    if (!plain) return NULL;
    MpVal *v = mp_decode_all(a, plain, out_n, &e);
    return (v && v->t == MP_MAP) ? v : NULL;
}

static char *desc_str(Arena *a, const MpVal *desc, const char *key) {
    MpVal *v = desc ? mp_get(desc, key) : NULL;
    if (!v || v->t != MP_STR || !v->u.s.n) return NULL;
    char *t = js_trim(a, arena_strndup(a, v->u.s.p, v->u.s.n));
    return (*t) ? t : NULL;
}

static int64_t desc_int(const MpVal *desc, const char *key) {
    MpVal *v = desc ? mp_get(desc, key) : NULL;
    return (v && v->t == MP_INT) ? v->u.i : 0;
}

typedef struct EggyInfo {
    char *project_name;
    char *map_dir;
    int64_t server_port;
} EggyInfo;

static EggyInfo read_eggy_info(Arena *a, const char *lua_root) {
    EggyInfo info = {0};
    if (!lua_root || !*lua_root) return info;
    char *p = path_join(a, lua_root, "eggy.json");
    if (!fs_is_file(p)) return info;
    size_t n = 0;
    Err e = {{0}, false};
    char *raw = fs_read_all(a, p, &n, &e);
    if (!raw) return info;
    if (n >= 3 && (unsigned char)raw[0] == 0xEF && (unsigned char)raw[1] == 0xBB &&
        (unsigned char)raw[2] == 0xBF) {
        raw += 3;
        n -= 3;
    }
    J *j = json_parse(a, raw, n, &e);
    if (!j || j->t != J_OBJ) return info;
    J *pn = j_get(j, "projectName");
    if (pn && pn->t == J_STR) {
        char *t = js_trim(a, pn->u.s.p);
        if (t && *t) info.project_name = t;
    }
    J *md = j_get(j, "mapDir");
    if (md && md->t == J_STR && md->u.s.n) info.map_dir = arena_strndup(a, md->u.s.p, md->u.s.n);
    J *sp = j_get(j, "serverPort");
    if (sp && sp->t == J_INT) info.server_port = sp->u.i;
    else if (sp && sp->t == J_DBL) info.server_port = (int64_t)sp->u.d;
    return info;
}

static char *vscode_name_alloc(Arena *a, const char *lua_root) {
    if (!lua_root) return NULL;
    const char *hit = NULL;
    for (const char *p = lua_root; (p = strstr(p, "LuaSource_")) != NULL; p += 10) hit = p + 10;
    if (!hit) return NULL;
    /* JS: /LuaSource_(.*?)\s*$/ → capture up to trailing whitespace */
    size_t len = strlen(hit);
    while (len > 0 && (hit[len - 1] == ' ' || hit[len - 1] == '\t' || hit[len - 1] == '\r' ||
                       hit[len - 1] == '\n')) {
        len--;
    }
    return arena_strndup(a, hit, len);
}

static ProjIndex read_project_index(Arena *a) {
    ProjIndex idx = {0};
    size_t root_n = 0;
    char **roots = maps_editor_roots(a, &root_n);
    const char *rels[2] = {"Documents\\vscode_projs.json", "vscode_projs.json"};
    for (size_t r = 0; r < root_n; r++) {
        for (int k = 0; k < 2; k++) {
            char *p = path_resolve(a, roots[r], rels[k]);
            if (!fs_is_file(p)) continue;
            size_t n = 0;
            Err e = {{0}, false};
            char *raw = fs_read_all(a, p, &n, &e);
            if (raw && n >= 3 && (unsigned char)raw[0] == 0xEF && (unsigned char)raw[1] == 0xBB &&
                (unsigned char)raw[2] == 0xBF) {
                raw += 3;
                n -= 3;
            }
            J *j = raw ? json_parse(a, raw, n, &e) : NULL;
            if (!j || j->t != J_OBJ) continue;
            idx.items = (ProjEntry *)arena_alloc(a, (j->u.obj.n + 1) * sizeof(ProjEntry));
            idx.n = 0;
            for (size_t i = 0; i < j->u.obj.n; i++) {
                const char *id = j->u.obj.keys[i];
                J *info = j->u.obj.vals[i];
                if (!info || info->t != J_OBJ) continue;
                J *lr = j_get(info, "lua_root");
                J *ws = j_get(info, "workspace");
                const char *lua_root = "";
                if (lr && lr->t == J_STR && lr->u.s.n) lua_root = lr->u.s.p;
                else if (ws && ws->t == J_STR && ws->u.s.n) lua_root = ws->u.s.p;
                idx.items[idx.n].id = id;
                idx.items[idx.n].lua_root = lua_root;
                EggyInfo eg = read_eggy_info(a, lua_root);
                idx.items[idx.n].project_name = eg.project_name;
                idx.items[idx.n].map_dir = eg.map_dir;
                idx.items[idx.n].server_port = eg.server_port;
                idx.items[idx.n].vscode_name = vscode_name_alloc(a, lua_root);
                idx.n++;
            }
            return idx;
        }
    }
    return idx;
}

static const ProjEntry *proj_lookup(const ProjIndex *idx, const char *id) {
    if (!idx || !id) return NULL;
    for (size_t i = 0; i < idx->n; i++) {
        if (str_eq(idx->items[i].id, id)) return &idx->items[i];
    }
    return NULL;
}

char *maps_read_map_name(Arena *a, const char *dir, const ProjIndex *idx) {
    MpVal *desc = read_desc_map(a, dir);
    char *mn = desc_str(a, desc, "map_name");
    if (mn) return mn;
    char *pid = read_project_id_from_gmp(a, dir);
    const ProjEntry *entry = pid ? proj_lookup(idx, pid) : NULL;
    if (entry) {
        if (entry->project_name && *entry->project_name) return arena_strdup(a, entry->project_name);
        if (entry->vscode_name && *entry->vscode_name) return arena_strdup(a, entry->vscode_name);
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* freshness                                                           */
/* ------------------------------------------------------------------ */
static int64_t walk_freshness(Arena *a, const char *dir, int64_t latest, int depth) {
    if (depth > 32) return latest;
    DirList dl;
    Err e = {{0}, false};
    if (!fs_list_dir(a, dir, &dl, &e)) return latest;
    for (size_t i = 0; i < dl.n; i++) {
        char *p = path_join(a, dir, dl.items[i].name);
        if (dl.items[i].is_dir) {
            latest = walk_freshness(a, p, latest, depth + 1);
            continue;
        }
        int64_t t = fs_mtime_ms(p);
        if (t > latest) latest = t;
    }
    return latest;
}

static int64_t project_freshness(const char *dir) {
    Arena tmp;
    arena_init(&tmp);
    int64_t latest = walk_freshness(&tmp, dir, 0, 0);
    arena_free(&tmp);
    if (!latest) latest = fs_mtime_ms(dir);
    return latest;
}

/* ------------------------------------------------------------------ */
/* collection                                                          */
/* ------------------------------------------------------------------ */
DraftList maps_collect_drafts(Arena *a) {
    DraftList out = {0};
    MapRoots roots = maps_find_map_roots(a);
    if (!roots.n) return out;
    ProjIndex idx = read_project_index(a);

    size_t cap = 32;
    Draft *items = (Draft *)arena_alloc(a, cap * sizeof(Draft));
    size_t n = 0;

    /* uuid dedupe, first-insert position retained */
    typedef struct UuidEntry {
        const char *key;
        size_t draft_index;
    } UuidEntry;
    size_t ucap = 32, un = 0;
    UuidEntry *ub = (UuidEntry *)arena_alloc(a, ucap * sizeof(UuidEntry));

    for (size_t r = 0; r < roots.n; r++) {
        size_t pn = 0;
        char **projects = maps_list_projects(a, roots.items[r], &pn);
        for (size_t i = 0; i < pn; i++) {
            Draft d;
            d.folder = projects[i];
            d.dir = path_join(a, roots.items[r], projects[i]);
            char *nm = maps_read_map_name(a, d.dir, &idx);
            d.name = (nm && *nm) ? nm : d.folder;
            d.root = roots.items[r];

            FolderId fid;
            bool has_id = maps_parse_folder_id(a, d.folder, &fid);
            if (has_id) {
                size_t found = (size_t)-1;
                for (size_t k = 0; k < un; k++) {
                    if (str_eq(ub[k].key, fid.raw_hex)) found = k;
                }
                if (found != (size_t)-1) {
                    Draft *prev = &items[ub[found].draft_index];
                    if (project_freshness(d.dir) > project_freshness(prev->dir)) {
                        items[ub[found].draft_index] = d;
                    }
                    continue;
                }
                if (un == ucap) {
                    size_t ncap = ucap * 2;
                    UuidEntry *ni = (UuidEntry *)arena_alloc(a, ncap * sizeof(UuidEntry));
                    if (!ni) continue;
                    memcpy(ni, ub, un * sizeof(UuidEntry));
                    ub = ni;
                    ucap = ncap;
                }
                if (n == cap) {
                    size_t ncap = cap * 2;
                    Draft *ni = (Draft *)arena_alloc(a, ncap * sizeof(Draft));
                    if (!ni) continue;
                    memcpy(ni, items, n * sizeof(Draft));
                    items = ni;
                    cap = ncap;
                }
                ub[un].key = fid.raw_hex;
                ub[un].draft_index = n;
                un++;
                items[n++] = d;
            } else {
                if (n == cap) {
                    size_t ncap = cap * 2;
                    Draft *ni = (Draft *)arena_alloc(a, ncap * sizeof(Draft));
                    if (!ni) continue;
                    memcpy(ni, items, n * sizeof(Draft));
                    items = ni;
                    cap = ncap;
                }
                items[n++] = d;
            }
        }
    }
    out.items = items;
    out.n = n;
    return out;
}

/* ------------------------------------------------------------------ */
/* matching                                                            */
/* ------------------------------------------------------------------ */
typedef struct Match {
    size_t *idx;
    size_t n;
} Match;

static Match match_by_priority(Arena *a, const char **keys, size_t n, const char *token) {
    Match m = {0};
    m.idx = (size_t *)arena_alloc(a, (n + 1) * sizeof(size_t));
    if (!m.idx || !n) return m;
    char *t = js_trim(a, token ? token : "");
    char *tl = str_lower_dup(a, t);
    if (!*tl) return m;

    for (int pass = 0; pass < 3; pass++) {
        size_t count = 0;
        for (size_t i = 0; i < n; i++) {
            char *k = str_lower_dup(a, keys[i] ? keys[i] : "");
            bool hit = false;
            if (pass == 0) hit = str_eq(k, tl);
            else if (pass == 1) hit = str_starts_ci(k, tl);
            else hit = str_contains_ci(k, tl);
            if (hit) m.idx[count++] = i;
        }
        if (count) {
            m.n = count;
            return m;
        }
    }
    return m;
}

/* ------------------------------------------------------------------ */
/* resolution                                                          */
/* ------------------------------------------------------------------ */
typedef struct Pick {
    bool ok;
    char *dir;
    char *folder;
    char *error;
} Pick;

static Pick pick_project_dir(Arena *a, const char *root, char **projects, size_t pn,
                             const char *token) {
    Pick p = {0};
    if (pn == 1) {
        p.ok = true;
        p.folder = projects[0];
        p.dir = path_join(a, root, projects[0]);
        return p;
    }
    Match m = match_by_priority(a, (const char **)projects, pn, token);
    if (m.n == 1) {
        p.ok = true;
        p.folder = projects[m.idx[0]];
        p.dir = path_join(a, root, p.folder);
        return p;
    }
    SB sb;
    sb_init(&sb);
    if (m.n > 1) {
        sb_printf(&sb, "“%s” 匹配到多个工程，请指定唯一：", token);
        for (size_t i = 0; i < m.n; i++) sb_printf(&sb, "\n  · %s", projects[m.idx[i]]);
    } else {
        sb_printf(&sb, "目录下有 %zu 个地图工程，请指定唯一路径：", pn);
        for (size_t i = 0; i < pn; i++) sb_printf(&sb, "\n  · %s", projects[i]);
    }
    p.error = arena_strdup(a, sb_cstr(&sb));
    sb_free(&sb);
    return p;
}

Resolve maps_resolve(Arena *a, const char *input, const char *cwd) {
    Resolve r = {0};
    char *p = path_resolve(a, cwd, input);

    if (fs_exists(p)) {
        if (fs_is_dir(p)) {
            if (maps_has_eui_file(p)) {
                r.ok = true;
                r.dir = p;
                r.folder = arena_strdup(a, path_basename(p));
                return r;
            }
            size_t pn = 0;
            char **projects = maps_list_projects(a, p, &pn);
            if (pn) {
                Pick pk = pick_project_dir(a, p, projects, pn, path_basename(p));
                r.ok = pk.ok;
                r.dir = pk.dir;
                r.folder = pk.folder;
                r.error = pk.error;
                return r;
            }
            r.error = arena_printf(a, "目录中未找到 eui.mm / euidata.mm：%s", p);
            return r;
        }
        if (fs_is_file(p) && path_ends_with_ci(p, ".gmp")) {
            r.error = arena_printf(
                a,
                "%s 是打包后的 .gmp 文件。\n请指向编辑中的地图工程目录（含 eui.mm / euidata.mm），如 "
                "Documents/etc/editor_maps/<id>__pc/ 或 Documents/etc/joint_construction_gmps/<id>/",
                path_basename(p));
            return r;
        }
        r.error = arena_printf(a, "无法识别的路径：%s", p);
        return r;
    }

    typedef struct Cand {
        char *root;
        char *folder;
        char *dir;
    } Cand;
    size_t ccap = 16, cn = 0;
    Cand *cands = (Cand *)arena_alloc(a, ccap * sizeof(Cand));
    MapRoots roots = maps_find_map_roots(a);
    for (size_t i = 0; i < roots.n; i++) {
        char *direct = path_join(a, roots.items[i], input);
        if (fs_is_dir(direct) && maps_has_eui_file(direct)) {
            if (cn == ccap) {
                size_t ncap = ccap * 2;
                Cand *ni = (Cand *)arena_alloc(a, ncap * sizeof(Cand));
                if (!ni) break;
                memcpy(ni, cands, cn * sizeof(Cand));
                cands = ni;
                ccap = ncap;
            }
            cands[cn].root = roots.items[i];
            cands[cn].folder = arena_strdup(a, path_basename(direct));
            cands[cn].dir = direct;
            cn++;
        }
        size_t pn = 0;
        char **projects = maps_list_projects(a, roots.items[i], &pn);
        Match m = match_by_priority(a, (const char **)projects, pn, input);
        for (size_t k = 0; k < m.n; k++) {
            if (cn == ccap) {
                size_t ncap = ccap * 2;
                Cand *ni = (Cand *)arena_alloc(a, ncap * sizeof(Cand));
                if (!ni) break;
                memcpy(ni, cands, cn * sizeof(Cand));
                cands = ni;
                ccap = ncap;
            }
            cands[cn].root = roots.items[i];
            cands[cn].folder = projects[m.idx[k]];
            cands[cn].dir = path_join(a, roots.items[i], projects[m.idx[k]]);
            cn++;
        }
    }
    if (!cn) {
        r.not_found = true;
        return r;
    }

    /* dedupe: same map uuid keeps the freshest copy; others keyed by full path */
    typedef struct Entry {
        char *key;
        Cand cand;
    } Entry;
    size_t ecap = 16, en = 0;
    Entry *ents = (Entry *)arena_alloc(a, ecap * sizeof(Entry));
    for (size_t i = 0; i < cn; i++) {
        FolderId fid;
        char *key;
        if (maps_parse_folder_id(a, cands[i].folder, &fid)) key = arena_strdup(a, fid.raw_hex);
        else key = arena_printf(a, "!%s", cands[i].dir);
        size_t found = (size_t)-1;
        for (size_t k = 0; k < en; k++) {
            if (str_eq(ents[k].key, key)) found = k;
        }
        if (found == (size_t)-1) {
            if (en == ecap) {
                size_t ncap = ecap * 2;
                Entry *ni = (Entry *)arena_alloc(a, ncap * sizeof(Entry));
                if (!ni) break;
                memcpy(ni, ents, en * sizeof(Entry));
                ents = ni;
                ecap = ncap;
            }
            ents[en].key = key;
            ents[en].cand = cands[i];
            en++;
        } else if (project_freshness(cands[i].dir) > project_freshness(ents[found].cand.dir)) {
            ents[found].cand = cands[i];
        }
    }
    if (en == 1) {
        r.ok = true;
        r.dir = ents[0].cand.dir;
        r.folder = ents[0].cand.folder;
        return r;
    }
    SB sb;
    sb_init(&sb);
    sb_printf(&sb, "“%s” 匹配到多个工程，请指定唯一：", input);
    for (size_t i = 0; i < en; i++) sb_printf(&sb, "\n  · %s", ents[i].cand.folder);
    r.error = arena_strdup(a, sb_cstr(&sb));
    sb_free(&sb);
    return r;
}

/* 草稿名与工程显示名（eggy.json 的 projectName）或 LuaSource_ 目录名一致时算命中，
   只有唯一命中才认，避免同名地图误绑定到别人的工程。 */
static const ProjEntry *proj_match_by_name(const ProjIndex *idx, const char *name) {
    const ProjEntry *hit = NULL;
    size_t hits = 0;
    for (size_t i = 0; i < idx->n; i++) {
        const ProjEntry *e = &idx->items[i];
        bool m = (e->project_name && *e->project_name && str_eq_ci(e->project_name, name)) ||
                 (e->vscode_name && *e->vscode_name && str_eq_ci(e->vscode_name, name));
        if (m) {
            hit = e;
            hits++;
        }
    }
    return hits == 1 ? hit : NULL;
}

static bool proj_has_lua(const ProjEntry *e) {
    return e && e->lua_root && *e->lua_root && fs_is_dir(e->lua_root);
}

/* 工程 eggy.json 的 mapDir 即它当前绑定的草稿目录，按目录 uuid 比对。 */
static const ProjEntry *proj_match_by_map_dir(Arena *a, const ProjIndex *idx,
                                              const char *uuid_hex) {
    if (!uuid_hex) return NULL;
    const ProjEntry *hit = NULL;
    size_t hits = 0;
    for (size_t i = 0; i < idx->n; i++) {
        const ProjEntry *e = &idx->items[i];
        if (!e->map_dir || !*e->map_dir) continue;
        char *h = folder_uuid_hex(a, path_basename(e->map_dir));
        if (h && str_eq_ci(h, uuid_hex)) {
            hit = e;
            hits++;
        }
    }
    return hits == 1 ? hit : NULL;
}

/* 草稿 desc.mm 的 vscode_server_port 记录的就是它最近连过的 lua 工程端口，
   草稿被复制 / 改名后依然保留，是「共享同一工程」最可靠的线索。 */
static const ProjEntry *proj_match_by_port(const ProjIndex *idx, int64_t port) {
    if (port <= 0) return NULL;
    const ProjEntry *hit = NULL;
    size_t hits = 0;
    for (size_t i = 0; i < idx->n; i++) {
        const ProjEntry *e = &idx->items[i];
        if (e->server_port != port) continue;
        hit = e;
        hits++;
    }
    return hits == 1 ? hit : NULL;
}

static char *resolve_lua(Arena *a, const ProjIndex *idx, const char *dir, const char *map_name,
                         int depth) {
    char *pid = read_project_id_from_gmp(a, dir);
    const ProjEntry *e = pid ? proj_lookup(idx, pid) : NULL;
    if (proj_has_lua(e)) return arena_strdup(a, e->lua_root);

    char *folder = arena_strdup(a, path_basename(dir));
    char *uuid = folder_uuid_hex(a, folder);

    e = proj_match_by_map_dir(a, idx, uuid);
    if (proj_has_lua(e)) return arena_strdup(a, e->lua_root);

    MpVal *desc = read_desc_map(a, dir);
    e = proj_match_by_port(idx, desc_int(desc, "vscode_server_port"));
    if (proj_has_lua(e)) return arena_strdup(a, e->lua_root);

    /* 复制出来的草稿与工程名对不上：alias 保存了被复制的原始草稿 uuid */
    if (depth == 0) {
        char *alias_hex = uuid16_to_hex(a, desc_str(a, desc, "alias"));
        if (alias_hex) {
            const ProjEntry *p = proj_lookup(idx, alias_hex);
            if (proj_has_lua(p)) return arena_strdup(a, p->lua_root);
            DraftList dl = maps_collect_drafts(a);
            for (size_t i = 0; i < dl.n; i++) {
                char *h = folder_uuid_hex(a, dl.items[i].folder);
                if (!h || !str_eq_ci(h, alias_hex)) continue;
                char *r = resolve_lua(a, idx, dl.items[i].dir, dl.items[i].name, depth + 1);
                if (r) return r;
            }
        }
    }

    if (map_name && *map_name) {
        const ProjEntry *m = proj_match_by_name(idx, map_name);
        if (proj_has_lua(m)) return arena_strdup(a, m->lua_root);
    }
    return NULL;
}

char *maps_find_lua_project_dir(Arena *a, const char *dir, const char *map_name) {
    ProjIndex idx = read_project_index(a);
    return resolve_lua(a, &idx, dir, map_name, 0);
}
