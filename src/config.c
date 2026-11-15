#include "config.h"

#include <stdio.h>
#include <string.h>

#include "fs.h"

char *config_path(Arena *a) {
    char *env = env_get(a, "EGGY_EUI_CONFIG");
    if (env && *env) return path_resolve(a, fs_cwd(a), env);
    return path_join(a, fs_home(a), ".euiexport.json");
}

void config_load(Config *cfg, Arena *a, Err *e) {
    cfg->a = a;
    cfg->path = config_path(a);
    cfg->root = j_obj(a);
    if (!fs_exists(cfg->path)) return;
    size_t n = 0;
    Err local = {{0}, false};
    char *raw = fs_read_all(a, cfg->path, &n, &local);
    if (!raw) return; /* unreadable config must never block the CLI */
    const char *p = raw;
    size_t len = n;
    if (len >= 3 && (unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB &&
        (unsigned char)p[2] == 0xBF) {
        p += 3;
        len -= 3;
    }
    J *parsed = json_parse(a, p, len, &local);
    if (parsed && parsed->t == J_OBJ) cfg->root = parsed;
    (void)e;
}

static char *config_dirname(Arena *a, const char *path) { return path_dirname(a, path); }

bool config_save(const Config *cfg, Err *e) {
    SB sb;
    sb_init(&sb);
    json_write(&sb, cfg->root, true);
    sb_putc(&sb, '\n');
    char *dir = config_dirname(cfg->a, cfg->path);
    if (!fs_mkdir_p(dir, e)) {
        sb_free(&sb);
        return false;
    }
    bool ok = fs_write_all(cfg->path, sb_cstr(&sb), sb.n, e);
    sb_free(&sb);
    return ok;
}

const char *config_get_str(const Config *cfg, const char *key) {
    J *v = j_get(cfg->root, key);
    return (v && v->t == J_STR) ? v->u.s.p : NULL;
}
