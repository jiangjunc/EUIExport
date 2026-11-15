/* cmd.c - command line surface: export / list / types / config / version. */
#include "cmd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "eui.h"
#include "fs.h"
#include "json.h"
#include "maps.h"
#include "mdout.h"
#include "msgpack.h"
#include "tui.h"
#include "tty.h"
#include "utf8.h"
#include "zstdwrap.h"

#define VERSION "2.0.0"
#define TOOL "蛋仔 EUI 导出器"

typedef struct ExportOpts {
    const char *map;
    const char *root;
    const char *output;
    bool list;
    bool interactive;
    bool compact;
    bool list_types;
    bool quiet;
    bool no_color;
} ExportOpts;

/* ------------------------------------------------------------------ */
/* tiny console helpers                                                */
/* ------------------------------------------------------------------ */
static const char *FIG_CROSS = "\xe2\x9c\x96";   /* ✖ */
static const char *FIG_TICK = "\xe2\x9c\x94";    /* ✔ */
static const char *FIG_POINTER = "\xe2\x9d\xaf"; /* ❯ */

static void say(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

static void err_line(const char *msg) {
    say("%s%s%s %s%s%s\n", sty_err(), sty_bold(), FIG_CROSS, sty_err(), msg, sty_reset());
}

static void wr_sb(SB *b) {
    tty_write(b->p ? b->p : "", b->n);
    sb_free(b);
}

static char *sanitize_name(Arena *a, const char *s) {
    char *t = js_trim(a, s ? s : "");
    size_t n = strlen(t);
    char *out = (char *)arena_alloc(a, n + 1);
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)t[i];
        if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' ||
            c == '>' || c == '|' || c < 0x20) {
            out[o++] = '_';
        } else {
            out[o++] = (char)c;
        }
    }
    while (o > 0 && (out[o - 1] == '.' || out[o - 1] == ' ')) o--;
    out[o] = '\0';
    char *r = js_trim(a, out);
    if (!*r) return arena_strdup(a, "map");
    return r;
}

static void fmt_kb(char out[32], size_t n) { snprintf(out, 32, "%.1f KB", (double)n / 1024.0); }

/* ------------------------------------------------------------------ */
/* help                                                               */
/* ------------------------------------------------------------------ */
static void print_examples(void) {
    say("\n%s示例%s\n", sty_accent(), sty_reset());
    say("  eui export                          # 交互式选择草稿并导出\n");
    say("  eui export 我的地图                 # 按草稿名称导出\n");
    say("  eui export <base64id> -c            # 按地图 id 导出压缩 JSON\n");
    say("  eui config set export-dir D:/out    # 设置默认导出目录\n");
    say("\n%s输出说明%s\n", sty_accent(), sty_reset());
    say("  输出目录为“<草稿名>/”，内含三个文件：\n");
    say("  · UIData.json  完整节点数据：summary 统计 + node_index 索引 + scene 节点树\n");
    say("  · UITree.md    完整节点树状图索引（文本结构树，每行带 id）\n");
    say("  · README.md    AI 阅读指南 + 全部字段含义字典\n");
    say("\n%s说明：eui.mm / euidata.mm 内部为 Zstandard 压缩 + MessagePack 序列化；%s\n", sty_dim(),
        sty_reset());
    say("%s草稿名称取自各工程 desc.mm 的 map_name 字段。%s\n", sty_dim(), sty_reset());
}

static void print_top_help(void) {
    SB b;
    sb_init(&b);
    tui_banner(&b, TOOL " v" VERSION, "Eggy Party PC Editor · EUI -> AI-readable JSON");
    wr_sb(&b);

    say("\n%s用法%s\n", sty_accent(), sty_reset());
    say("  eui export [地图] [选项]     导出地图 UI（缺省交互选择草稿）\n");
    say("  eui list [选项]              列出所有地图草稿\n");
    say("  eui types                    打印控件类型对照表\n");
    say("  eui config <子命令>          管理配置（导出目录 / 编辑器根目录）\n");
    say("  eui version                  显示版本号\n");
    say("  eui help                     显示本帮助\n");
    say("\n%s选项（export）%s\n", sty_accent(), sty_reset());
    say("  -l, --list             列出草稿名称后退出\n");
    say("  -i, --interactive      强制交互选择（管道环境也可用）\n");
    say("  -r, --root <路径>      手动指定编辑器根目录\n");
    say("  -o, --output <目录>    直接指定输出目录\n");
    say("  -c, --compact          UIData.json 输出压缩 JSON\n");
    say("  -t, --list-types       打印控件类型对照表后退出\n");
    say("  -q, --quiet            安静模式：只打印一行结果摘要\n");
    say("      --no-color         禁用 ANSI 颜色\n");
    print_examples();
}

static void print_export_help(void) {
    say("%s%s export%s\n", sty_bold(), TOOL, sty_reset());
    say("%s把 EUI 数据还原成 AI 可读的 UI 数据包（UIData.json / UITree.md / README.md）%s\n",
        sty_dim(), sty_reset());
    say("\n%s用法%s\n", sty_accent(), sty_reset());
    say("  eui export [地图] [选项]\n");
    say("\n%s参数%s\n", sty_accent(), sty_reset());
    say("  [地图]  地图草稿名称 / 工程目录 / base64 地图 id（缺省则交互选择）\n");
    say("\n%s选项%s\n", sty_accent(), sty_reset());
    say("  -l, --list             列出所有地图草稿名称后退出\n");
    say("  -i, --interactive      强制进入交互选择（脚本 / 管道也可用）\n");
    say("  -r, --root <路径>      手动指定编辑器根目录（也可 `eui config set editor-root <path>`）\n");
    say("  -o, --output <目录>    输出目录（直接指定）；未指定时按优先级取：\n");
    say("                         EGGY_EUI_OUTPUT > eui config export-dir > 草稿 lua 工程目录 > 桌面\n");
    say("  -c, --compact          UIData.json 输出压缩 JSON（默认美化缩进 2 格）\n");
    say("  -t, --list-types       打印控件类型对照表后退出\n");
    say("  -q, --quiet            安静模式：只打印一行结果摘要\n");
    say("      --no-color         禁用 ANSI 颜色\n");
    print_examples();
}

static void print_config_help(void) {
    say("%s%s config%s\n", sty_bold(), TOOL, sty_reset());
    say("%s用户配置管理（配置文件默认位于 ~/.euiexport.json）%s\n", sty_dim(), sty_reset());
    say("\n%s用法%s\n", sty_accent(), sty_reset());
    say("  eui config path              显示配置文件路径\n");
    say("  eui config get [键]          读取配置（无键打印全部）\n");
    say("  eui config set <键> <值>     设置配置\n");
    say("  eui config unset <键>        删除配置\n");
    say("\n%s支持的键%s\n", sty_accent(), sty_reset());
    say("  export-dir    默认导出目录（基础目录；每次导出创建 <exportDir>/<草稿名>/）\n");
    say("                优先级：-o 参数 > EGGY_EUI_OUTPUT 环境变量 > 此配置 > 草稿 lua 工程目录 > 桌面\n");
    say("  editor-root   编辑器根目录（自动发现失败时手动指定）\n");
    say("                优先级：-r/--root 参数 > 此配置 > ~/.eggitor/cli/editor_config.json > 当前目录\n");
}

/* ------------------------------------------------------------------ */
/* types                                                              */
/* ------------------------------------------------------------------ */
static int cmd_types(void) {
    size_t n = 0;
    const EuiType *types = eui_type_table(&n);
    say("%s控件类型对照表 (type -> 含义)%s\n\n", sty_bold(), sty_reset());

    Arena a;
    arena_init(&a);
    const char **cells = (const char **)arena_alloc(&a, n * 3 * sizeof(char *));
    for (size_t i = 0; i < n; i++) {
        cells[i * 3 + 0] = arena_printf(&a, "%lld", (long long)types[i].code);
        cells[i * 3 + 1] = types[i].name;
        cells[i * 3 + 2] = types[i].zh;
    }
    TuiCol cols[3] = {{"type", true}, {"名称", false}, {"含义", false}};
    SB b;
    sb_init(&b);
    tui_table(&b, cols, 3, cells, n);
    wr_sb(&b);
    say("\n%s说明：type 为 eui.mm 中节点存贮的数字编码；cc_type 为编辑器内部控件类型字符串。%s\n",
        sty_dim(), sty_reset());
    arena_free(&a);
    return 0;
}

/* ------------------------------------------------------------------ */
/* draft lists                                                        */
/* ------------------------------------------------------------------ */
static void print_draft_table(Arena *a, const DraftList *dl) {
    bool show_id = tty_columns() >= 110;
    size_t stride = show_id ? 4 : 3;
    const char **cells = (const char **)arena_alloc(a, dl->n * stride * sizeof(char *));
    for (size_t i = 0; i < dl->n; i++) {
        FolderId fid;
        bool has = maps_parse_folder_id(a, dl->items[i].folder, &fid);
        cells[i * stride + 0] = arena_printf(a, "%zu", i + 1);
        cells[i * stride + 1] = dl->items[i].name ? dl->items[i].name : dl->items[i].folder;
        cells[i * stride + 2] = dl->items[i].folder;
        if (show_id) cells[i * stride + 3] = has ? fid.raw_hex : "";
    }
    SB b;
    sb_init(&b);
    if (show_id) {
        TuiCol cols[4] = {{"#", true}, {"草稿名称", false}, {"工程目录", false}, {"地图 id", false}};
        tui_table(&b, cols, 4, cells, dl->n);
    } else {
        TuiCol cols[3] = {{"#", true}, {"草稿名称", false}, {"工程目录", false}};
        tui_table(&b, cols, 3, cells, dl->n);
    }
    wr_sb(&b);
}

static void print_draft_list(Arena *a, const DraftList *dl) {
    if (!dl->n) {
        say("%s未找到任何地图草稿（未探测到含 desc.mm 的地图工程目录）。%s\n", sty_warn(), sty_reset());
        return;
    }
    say("%s共 %zu 个地图草稿：%s\n\n", sty_bold(), dl->n, sty_reset());
    print_draft_table(a, dl);
    say("%s提示：eui export <草稿名称 | 工程目录 | 地图 id> 可导出对应地图%s\n", sty_dim(),
        sty_reset());
}

static void print_draft_item(Arena *a, const Draft *d, const char *num_prefix) {
    FolderId fid;
    bool has = maps_parse_folder_id(a, d->folder, &fid);
    say("%s%s%s%s%s  %s(%s)%s", sty_dim(), num_prefix, sty_reset(), sty_bold(), d->name, sty_dim(),
        d->folder, sty_reset());
    if (has) say("%s  id: %s%s", sty_dim(), fid.raw_hex, sty_reset());
    say("\n");
}

static size_t match_drafts(const DraftList *dl, const char *token, size_t *out, size_t out_cap) {
    int level;
    bool any = false;
    size_t n = 0;
    for (level = 0; level < 3; level++) {
        n = 0;
        any = false;
        for (size_t i = 0; i < dl->n && n < out_cap; i++) {
            const char *name = dl->items[i].name ? dl->items[i].name : dl->items[i].folder;
            bool hit = false;
            if (level == 0) hit = str_eq_ci(name, token);
            else if (level == 1) hit = str_starts_ci(name, token);
            else hit = str_contains_ci(name, token);
            if (hit) {
                out[n++] = i;
                any = true;
            }
        }
        if (any) return n;
    }
    return 0;
}

/* Numbered fallback prompt used when the console cannot host the picker. */
static int read_number(size_t limit) {
    char buf[64];
    say("  输入编号后回车 > ");
    fflush(stdout);
    if (!fgets(buf, sizeof(buf), stdin)) return -1;
    char *t = buf;
    while (*t == ' ' || *t == '\t') t++;
    long v = strtol(t, NULL, 10);
    if (v <= 0 || (size_t)v > limit) return -1;
    return (int)(v - 1);
}

static int pick_draft(Arena *a, const DraftList *dl, const ExportOpts *o) {
    if (tty_supports_ui()) {
        TuiItem *items = (TuiItem *)arena_alloc(a, dl->n * sizeof(TuiItem));
        for (size_t i = 0; i < dl->n; i++) {
            FolderId fid;
            bool has = maps_parse_folder_id(a, dl->items[i].folder, &fid);
            items[i].label = dl->items[i].name ? dl->items[i].name : dl->items[i].folder;
            items[i].detail = has ? arena_printf(a, "(%s)  id: %s", dl->items[i].folder, fid.raw_hex)
                                  : arena_printf(a, "(%s)", dl->items[i].folder);
        }
        return tui_select("请选择要导出的地图草稿", "输入关键字可模糊筛选 · ↑↓ 移动 · Enter 确认", items,
                          dl->n);
    }

    if (!o->quiet) {
        say("\n  请选择要导出的地图草稿：\n");
        print_draft_table(a, dl);
        say("   0.  退出\n\n");
    }
    if (!o->interactive) return -1;
    return read_number(dl->n);
}

/* ------------------------------------------------------------------ */
/* export                                                             */
/* ------------------------------------------------------------------ */
static char *pick_out_dir(Arena *a, const ExportOpts *o, const char *cwd, const char *map_dir,
                          const char *folder, const char *map_name, const Config *cfg, char **base,
                          const char **base_src, const char **dir_name) {
    char *name = sanitize_name(a, map_name ? map_name : folder);
    *dir_name = name;
    *base = NULL;
    *base_src = NULL;

    if (o->output) return path_resolve(a, cwd, o->output);

    const char *env = env_get(a, "EGGY_EUI_OUTPUT");
    if (env && *env) {
        *base = path_resolve(a, cwd, env);
        *base_src = "环境变量 EGGY_EUI_OUTPUT";
        return path_join(a, *base, name);
    }
    const char *cfg_dir = config_get_str(cfg, "exportDir");
    if (cfg_dir && *cfg_dir) {
        *base = (char *)cfg_dir;
        *base_src = "配置文件 exportDir";
        return path_join(a, *base, name);
    }
    char *lua = maps_find_lua_project_dir(a, map_dir, map_name);
    if (lua && *lua) {
        *base = lua;
        *base_src = "草稿 lua 工程目录";
        return path_join(a, *base, name);
    }
    *base = fs_desktop(a);
    *base_src = "桌面（兜底）";
    return path_join(a, *base, name);
}

static int run_export(ExportOpts *o) {
    Arena a;
    arena_init(&a);
    Err e = {{0}, false};
    char *cwd = fs_cwd(&a);

    if (o->root) maps_set_editor_root(&a, o->root);

    if (o->list_types) {
        int code = cmd_types();
        arena_free(&a);
        return code;
    }
    if (o->list) {
        DraftList dl = maps_collect_drafts(&a);
        print_draft_list(&a, &dl);
        arena_free(&a);
        return 0;
    }

    if (!o->quiet) {
        SB b;
        sb_init(&b);
        tui_banner(&b, TOOL " v" VERSION, "Eggy Party PC Editor · EUI -> AI-readable JSON");
        wr_sb(&b);
        say("\n");
    }

    char *map_dir = NULL;
    char *folder = NULL;
    char *map_name = NULL;

    if (!o->map) {
        DraftList dl = maps_collect_drafts(&a);
        if (!dl.n) {
            err_line("未找到任何地图草稿。\n请指定草稿名称 / 工程目录 / 地图 id，或用 -h 查看帮助。");
            size_t rn = 0;
            char **roots = maps_editor_roots(&a, &rn);
            say("%s已探测的编辑器根目录：%s\n", sty_dim(), sty_reset());
            for (size_t i = 0; i < rn; i++) say("  · %s\n", roots[i]);
            arena_free(&a);
            return 1;
        }
        if (dl.n == 1) {
            map_dir = dl.items[0].dir;
            folder = dl.items[0].folder;
            map_name = dl.items[0].name;
        } else {
            int picked = pick_draft(&a, &dl, o);
            if (picked < 0) {
                if (!o->quiet) {
                    if (!o->interactive && !tty_stdin_is_tty()) {
                        say("\n%s非交互环境：请指定草稿名称 / 工程目录 / 地图 id（或用 -i 强制交互）。%s\n",
                            sty_warn(), sty_reset());
                    } else {
                        say("\n%s已取消。%s\n", sty_warn(), sty_reset());
                    }
                }
                arena_free(&a);
                return 1;
            }
            map_dir = dl.items[picked].dir;
            folder = dl.items[picked].folder;
            map_name = dl.items[picked].name;
        }
    } else {
        Resolve r = maps_resolve(&a, o->map, cwd);
        if (r.ok) {
            map_dir = r.dir;
            folder = r.folder;
        } else {
            DraftList dl = maps_collect_drafts(&a);
            size_t *hits = (size_t *)arena_alloc(&a, (dl.n ? dl.n : 1) * sizeof(size_t));
            size_t hits_n = match_drafts(&dl, o->map, hits, dl.n);
            if (hits_n == 1) {
                map_dir = dl.items[hits[0]].dir;
                folder = dl.items[hits[0]].folder;
                map_name = dl.items[hits[0]].name;
            } else if (hits_n > 1) {
                SB mb;
                sb_init(&mb);
                sb_printf(&mb, "草稿名称“%s”匹配到多个，请更具体：", o->map);
                say("%s%s%s %s%s\n", sty_err(), sty_bold(), FIG_CROSS, sb_cstr(&mb), sty_reset());
                sb_free(&mb);
                for (size_t i = 0; i < hits_n; i++) {
                    char num[32];
                    snprintf(num, sizeof(num), " %2zu.  ", i + 1);
                    print_draft_item(&a, &dl.items[hits[i]], num);
                }
                arena_free(&a);
                return 1;
            } else {
                SB mb;
                sb_init(&mb);
                if (r.error) sb_puts(&mb, r.error);
                else
                    sb_printf(&mb,
                              "找不到地图“%s”。\n可输入：草稿名称（如“我的地图”）/ 工程目录 / base64 地图 id，"
                              "或用 -h 查看帮助。",
                              o->map);
                err_line(sb_cstr(&mb));
                sb_free(&mb);
                arena_free(&a);
                return 1;
            }
        }
    }

    if (!map_name) map_name = maps_read_map_name(&a, map_dir, NULL);

    FolderId fid;
    bool has_id = maps_parse_folder_id(&a, folder, &fid);

    if (!o->quiet) {
        say("  %s%s%s %s%s  %s%s%s\n", sty_accent(), FIG_POINTER, sty_reset(), sty_bold(), "草稿",
            sty_accent(), map_name ? map_name : "(未命名)", sty_reset());
        say("  %s%s%s %s%s  %s%s%s\n", sty_accent(), FIG_POINTER, sty_reset(), sty_bold(), "工程",
            sty_accent(), map_dir, sty_reset());
        if (has_id) {
            say("  %s%s%s %s%s %sbase64:%s %s  %shex:%s %s\n", sty_accent(), FIG_POINTER, sty_reset(),
                sty_bold(), "地图ID", sty_dim(), sty_reset(), fid.base64, sty_dim(), sty_reset(),
                fid.hex);
        }
        say("\n");
    }

    char *eui_path = maps_eui_file(&a, map_dir);
    if (!eui_path) {
        SB mb;
        sb_init(&mb);
        sb_printf(&mb, "未找到 %s 或 euidata.mm", map_dir);
        err_line(sb_cstr(&mb));
        sb_free(&mb);
        arena_free(&a);
        return 1;
    }

    size_t comp_n = 0;
    char *comp = fs_read_all(&a, eui_path, &comp_n, &e);
    if (!comp) {
        SB mb;
        sb_init(&mb);
        sb_printf(&mb, "读取失败：%s", e.msg[0] ? e.msg : eui_path);
        err_line(sb_cstr(&mb));
        sb_free(&mb);
        arena_free(&a);
        return 1;
    }

    size_t raw_n = 0;
    char *raw = zstd_decompress(&a, comp, comp_n, &raw_n, &e);
    if (!raw) {
        SB mb;
        sb_init(&mb);
        sb_printf(&mb, "Zstandard 解压失败（可能不是合法的 eui.mm）：%s", e.msg[0] ? e.msg : "未知错误");
        err_line(sb_cstr(&mb));
        sb_free(&mb);
        arena_free(&a);
        return 1;
    }

    MpVal *root = mp_decode_all(&a, raw, raw_n, &e);
    if (!root) {
        SB mb;
        sb_init(&mb);
        sb_printf(&mb, "MessagePack 解码失败：%s", e.msg[0] ? e.msg : "未知错误");
        err_line(sb_cstr(&mb));
        sb_free(&mb);
        arena_free(&a);
        return 1;
    }

    Err se = {{0}, false};
    J *scene = eui_export_scene(&a, root, &se);
    if (!scene) {
        SB mb;
        sb_init(&mb);
        sb_printf(&mb,
                  "EUI 数据解析失败：%s\n"
                  "  可能原因：该地图还没有可导出的 UI（新建地图未放任何控件）；或 eui.mm / euidata.mm "
                  "数据为空/损坏。\n"
                  "  建议：打开该地图放一个控件再保存，或确认选中的不是打包后的 .gmp 文件。",
                  se.msg[0] ? se.msg : "未知错误");
        err_line(sb_cstr(&mb));
        sb_free(&mb);
        arena_free(&a);
        return 1;
    }

    EuiStats stats;
    eui_count_nodes(&a, scene, &stats);

    Config cfg;
    config_load(&cfg, &a, &e);

    char *base = NULL;
    const char *base_src = NULL;
    const char *dir_name = NULL;
    char *out_dir = pick_out_dir(&a, o, cwd, map_dir, folder, map_name, &cfg, &base, &base_src,
                                 &dir_name);

    if (!fs_mkdir_p(out_dir, &e)) {
        SB mb;
        sb_init(&mb);
        sb_printf(&mb, "创建输出目录失败：%s", e.msg[0] ? e.msg : out_dir);
        err_line(sb_cstr(&mb));
        sb_free(&mb);
        arena_free(&a);
        return 1;
    }

    char *now = now_iso8601(&a);
    J *doc = j_obj(&a);
    j_set(&a, doc, "format", j_str(&a, "eggy-eui-export"));
    j_set(&a, doc, "schema_version", j_int(&a, 2));
    j_set(&a, doc, "map_name", map_name ? j_str(&a, map_name) : j_null(&a));
    if (has_id) {
        J *id = j_obj(&a);
        j_set(&a, id, "base64", j_str(&a, fid.base64));
        j_set(&a, id, "hex", j_str(&a, fid.hex));
        j_set(&a, doc, "map_id", id);
    } else {
        j_set(&a, doc, "map_id", j_null(&a));
    }
    j_set(&a, doc, "source_file", j_str(&a, eui_path));
    j_set(&a, doc, "exported_at", j_str(&a, now));
    J *sum = j_obj(&a);
    j_set(&a, sum, "node_count", j_int(&a, stats.count));
    j_set(&a, sum, "canvas_count", j_int(&a, stats.canvas_count));
    j_set(&a, sum, "max_depth", j_int(&a, stats.max_depth));
    j_set(&a, sum, "joystick_count", j_int(&a, stats.joystick_count));
    j_set(&a, sum, "type_counts", stats.type_counts);
    j_set(&a, doc, "summary", sum);
    j_set(&a, doc, "node_index", eui_build_node_index(&a, scene));
    j_set(&a, doc, "scene", scene);

    SB jb;
    sb_init(&jb);
    json_write(&jb, doc, !o->compact);
    sb_putc(&jb, '\n');

    SB tree;
    sb_init(&tree);
    size_t tree_lines = 0;
    md_build_tree(&tree, map_name, &stats, scene, &tree_lines);

    SB readme;
    sb_init(&readme);
    md_build_readme(&readme, map_name, dir_name, eui_path, &stats, true, now);

    char *data_path = path_join(&a, out_dir, "UIData.json");
    char *tree_path = path_join(&a, out_dir, "UITree.md");
    char *readme_path = path_join(&a, out_dir, "README.md");

    const char *fail = NULL;
    if (!fs_write_all(data_path, jb.p, jb.n, &e)) fail = "UIData.json";
    else if (!fs_write_all(tree_path, tree.p, tree.n, &e)) fail = "UITree.md";
    else if (!fs_write_all(readme_path, readme.p, readme.n, &e)) fail = "README.md";
    if (fail) {
        SB mb;
        sb_init(&mb);
        sb_printf(&mb, "写入 %s 失败：%s", fail, e.msg[0] ? e.msg : "未知错误");
        err_line(sb_cstr(&mb));
        sb_free(&mb);
        sb_free(&jb);
        sb_free(&tree);
        sb_free(&readme);
        arena_free(&a);
        return 1;
    }

    char data_kb[32], tree_kb[32], readme_kb[32];
    fmt_kb(data_kb, jb.n);
    fmt_kb(tree_kb, tree.n);
    fmt_kb(readme_kb, readme.n);

    if (o->quiet) {
        say("%s\t%lld nodes\t(%s + %s + %s)\n", out_dir, (long long)stats.count, data_kb, tree_kb,
            readme_kb);
    } else {
        SB b;
        sb_init(&b);
        sb_puts(&b, sty_ok());
        sb_puts(&b, FIG_TICK);
        sb_puts(&b, " ");
        sb_puts(&b, sty_reset());
        sb_puts(&b, sty_ok());
        sb_puts(&b, sty_bold());
        sb_puts(&b, "解析完成");
        sb_puts(&b, sty_reset());
        sb_puts(&b, "\n");
        sb_printf(&b, "   %s节点%s %lld    %s画布%s %lld    %s深度%s %lld\n", sty_bold(),
                  sty_reset(), (long long)stats.count, sty_bold(), sty_reset(),
                  (long long)stats.canvas_count, sty_bold(), sty_reset(),
                  (long long)stats.max_depth);

        sb_puts(&b, "   ");
        sb_puts(&b, sty_bold());
        sb_puts(&b, "类型");
        sb_puts(&b, sty_reset());
        sb_puts(&b, " ");
        for (size_t i = 0; i < stats.type_counts->u.obj.n; i++) {
            if (i) sb_puts(&b, "  ");
            sb_puts(&b, stats.type_counts->u.obj.keys[i]);
            sb_puts(&b, "=");
            sb_puts(&b, sty_warn());
            sb_printf(&b, "%lld", (long long)stats.type_counts->u.obj.vals[i]->u.i);
            sb_puts(&b, sty_reset());
        }
        sb_puts(&b, "\n");
        sb_puts(&b, "\n");
        wr_sb(&b);

        say("%s输出目录%s  %s%s%s\n", sty_bold(), sty_reset(), sty_ok(), out_dir, sty_reset());
        if (base) say("%s  （基础目录来源：%s = %s）%s\n", sty_dim(), base_src, base, sty_reset());
        say("  ├─ %sUIData.json%s  %s%s%s  完整节点数据（summary + node_index + scene 树）\n",
            sty_ok(), sty_reset(), sty_dim(), data_kb, sty_reset());
        say("  ├─ %sUITree.md%s    %s%s%s  完整节点树状图索引（%zu 行）\n", sty_ok(), sty_reset(),
            sty_dim(), tree_kb, sty_reset(), tree_lines);
        say("  └─ %sREADME.md%s    %s%s%s  AI 阅读指南 + 字段字典\n", sty_ok(), sty_reset(),
            sty_dim(), readme_kb, sty_reset());
        say("\n%s提示：AI 先读 README.md 了解阅读顺序，再读 UITree.md 浏览层级，最后用 UIData.json "
            "查节点细节。%s\n",
            sty_dim(), sty_reset());
    }

    sb_free(&jb);
    sb_free(&tree);
    sb_free(&readme);
    arena_free(&a);
    return 0;
}

/* ------------------------------------------------------------------ */
/* config                                                             */
/* ------------------------------------------------------------------ */
static const char *resolve_key(const char *key) {
    if (str_eq(key, "export-dir") || str_eq(key, "export_dir")) return "exportDir";
    if (str_eq(key, "editor-root") || str_eq(key, "editor_root")) return "editorRoot";
    return key;
}

static void config_print(const Config *cfg, const char *key) {
    const char *v = config_get_str(cfg, key);
    if (v) {
        say("%s\n", v);
    } else {
        J *val = j_get(cfg->root, key);
        if (!val) say("\n");
        else {
            SB b;
            sb_init(&b);
            json_write(&b, val, false);
            sb_putc(&b, '\n');
            wr_sb(&b);
        }
    }
}

static int cmd_config(int n, char **args) {
    if (n == 0) {
        print_config_help();
        return 0;
    }
    const char *sub = args[0];
    for (int i = 1; i < n; i++) {
        if (str_eq(args[i], "-h") || str_eq(args[i], "--help")) {
            print_config_help();
            return 0;
        }
    }

    Arena a;
    arena_init(&a);
    Err e = {{0}, false};

    if (str_eq(sub, "path")) {
        say("%s\n", config_path(&a));
        arena_free(&a);
        return 0;
    }

    Config cfg;
    config_load(&cfg, &a, &e);

    if (str_eq(sub, "get")) {
        if (n < 2) {
            SB b;
            sb_init(&b);
            json_write(&b, cfg.root, true);
            sb_putc(&b, '\n');
            wr_sb(&b);
        } else {
            config_print(&cfg, resolve_key(args[1]));
        }
        arena_free(&a);
        return 0;
    }
    if (str_eq(sub, "set")) {
        if (n < 3) {
            err_line("用法：eui config set <键> <值>");
            arena_free(&a);
            return 1;
        }
        const char *internal = resolve_key(args[1]);
        const char *value = args[2];
        if ((str_eq(internal, "exportDir") || str_eq(internal, "editorRoot")) && *value &&
            !path_is_abs(value)) {
            value = path_resolve(&a, fs_cwd(&a), value);
        }
        j_set(&a, cfg.root, internal, j_str(&a, value));
        if (!config_save(&cfg, &e)) {
            err_line(e.msg[0] ? e.msg : "写入配置失败");
            arena_free(&a);
            return 1;
        }
        say("%s%s%s %s = %s%s%s\n", sty_ok(), FIG_TICK, sty_reset(), args[1], sty_bold(), value,
            sty_reset());
        say("%s配置文件：%s%s\n", sty_dim(), cfg.path, sty_reset());
        arena_free(&a);
        return 0;
    }
    if (str_eq(sub, "unset")) {
        if (n < 2) {
            err_line("用法：eui config unset <键>");
            arena_free(&a);
            return 1;
        }
        const char *internal = resolve_key(args[1]);
        J *next = j_obj(&a);
        for (size_t i = 0; i < cfg.root->u.obj.n; i++) {
            if (str_eq(cfg.root->u.obj.keys[i], internal)) continue;
            j_set(&a, next, cfg.root->u.obj.keys[i], cfg.root->u.obj.vals[i]);
        }
        cfg.root = next;
        if (!config_save(&cfg, &e)) {
            err_line(e.msg[0] ? e.msg : "写入配置失败");
            arena_free(&a);
            return 1;
        }
        say("%s%s%s 已删除 %s\n", sty_ok(), FIG_TICK, sty_reset(), args[1]);
        say("%s配置文件：%s%s\n", sty_dim(), cfg.path, sty_reset());
        arena_free(&a);
        return 0;
    }

    err_line("未知的 config 子命令");
    print_config_help();
    arena_free(&a);
    return 1;
}

/* ------------------------------------------------------------------ */
/* dispatch                                                           */
/* ------------------------------------------------------------------ */
static bool is_known_command(const char *s) {
    return str_eq(s, "export") || str_eq(s, "list") || str_eq(s, "types") || str_eq(s, "config") ||
           str_eq(s, "help") || str_eq(s, "version") || str_eq(s, "edit");
}

static int parse_export(int n, char **args) {
    ExportOpts o;
    memset(&o, 0, sizeof(o));
    for (int i = 0; i < n; i++) {
        const char *s = args[i];
        if (str_eq(s, "-h") || str_eq(s, "--help")) {
            print_export_help();
            return 0;
        }
        if (str_eq(s, "-l") || str_eq(s, "--list")) o.list = true;
        else if (str_eq(s, "-i") || str_eq(s, "--interactive")) o.interactive = true;
        else if (str_eq(s, "-c") || str_eq(s, "--compact")) o.compact = true;
        else if (str_eq(s, "-t") || str_eq(s, "--list-types")) o.list_types = true;
        else if (str_eq(s, "-q") || str_eq(s, "--quiet")) o.quiet = true;
        else if (str_eq(s, "--no-color")) o.no_color = true;
        else if (str_eq(s, "-r") || str_eq(s, "--root")) {
            if (i + 1 >= n) {
                err_line("选项 --root 需要一个路径");
                return 1;
            }
            o.root = args[++i];
        } else if (str_starts_ci(s, "--root=")) {
            o.root = s + 7;
        } else if (str_eq(s, "-o") || str_eq(s, "--output")) {
            if (i + 1 >= n) {
                err_line("选项 --output 需要一个目录");
                return 1;
            }
            o.output = args[++i];
        } else if (str_starts_ci(s, "--output=")) {
            o.output = s + 9;
        } else if (s[0] == '-' && s[1]) {
            say("%s错误：未知选项 '%s'%s\n\n", sty_err(), s, sty_reset());
            print_export_help();
            return 1;
        } else if (!o.map) {
            o.map = s;
        } else {
            say("%s错误：多余的参数 '%s'%s\n\n", sty_err(), s, sty_reset());
            print_export_help();
            return 1;
        }
    }
    return run_export(&o);
}

static int parse_list(int n, char **args) {
    ExportOpts o;
    memset(&o, 0, sizeof(o));
    o.list = true;
    for (int i = 0; i < n; i++) {
        const char *s = args[i];
        if (str_eq(s, "-h") || str_eq(s, "--help")) {
            say("%s%s list%s\n", sty_bold(), TOOL, sty_reset());
            say("  eui list [-r <路径>] [--no-color]\n\n");
            say("  列出所有地图草稿（自动探测编辑器根目录）。\n");
            return 0;
        }
        if (str_eq(s, "-r") || str_eq(s, "--root")) {
            if (i + 1 >= n) {
                err_line("选项 --root 需要一个路径");
                return 1;
            }
            o.root = args[++i];
        } else if (str_starts_ci(s, "--root=")) {
            o.root = s + 7;
        } else if (str_eq(s, "--no-color")) {
            o.no_color = true;
        } else if (str_eq(s, "-q") || str_eq(s, "--quiet")) {
            o.quiet = true;
        } else if (s[0] == '-' && s[1]) {
            say("%s错误：未知选项 '%s'%s\n", sty_err(), s, sty_reset());
            return 1;
        } else {
            say("%s错误：多余的参数 '%s'%s\n", sty_err(), s, sty_reset());
            return 1;
        }
    }
    return run_export(&o);
}

static int dispatch(int n, char **args) {
    if (n == 0) {
        print_top_help();
        return 0;
    }

    const char *cmd = args[0];
    if (str_eq(cmd, "-v") || str_eq(cmd, "--version")) {
        say("%s\n", VERSION);
        return 0;
    }
    if (str_eq(cmd, "-h") || str_eq(cmd, "--help")) {
        print_top_help();
        return 0;
    }
    if (cmd[0] == '-') return parse_export(n, args);

    /* Legacy form: `eui <draft>` behaves like `eui export <draft>`. */
    if (!is_known_command(cmd)) return parse_export(n, args);

    if (str_eq(cmd, "export")) return parse_export(n - 1, args + 1);
    if (str_eq(cmd, "list")) return parse_list(n - 1, args + 1);
    if (str_eq(cmd, "types")) {
        for (int i = 1; i < n; i++) {
            if (str_eq(args[i], "-h") || str_eq(args[i], "--help")) {
                say("%s%s types%s\n  eui types [--no-color]\n\n  打印控件类型对照表。\n", sty_bold(), TOOL,
                    sty_reset());
                return 0;
            }
        }
        return cmd_types();
    }
    if (str_eq(cmd, "config")) return cmd_config(n - 1, args + 1);
    if (str_eq(cmd, "version") || str_eq(cmd, "-v") || str_eq(cmd, "--version")) {
        say("%s\n", VERSION);
        return 0;
    }
    if (str_eq(cmd, "help") || str_eq(cmd, "-h") || str_eq(cmd, "--help")) {
        print_top_help();
        return 0;
    }
    if (str_eq(cmd, "edit")) {
        err_line("本版本不再支持 UI 编辑，仅支持导出。请使用 `eui export`。");
        return 1;
    }
    say("%s错误：未知命令 '%s'%s\n\n", sty_err(), cmd, sty_reset());
    print_top_help();
    return 1;
}

int cmd_main(int argc, char **argv) {
    bool no_color = false;
    char **args = (char **)malloc(sizeof(char *) * (size_t)(argc + 1));
    if (!args) return 1;
    int n = 0;
    args[n++] = argv[0];
    for (int i = 1; i < argc; i++) {
        if (str_eq(argv[i], "--no-color")) {
            no_color = true;
            continue;
        }
        args[n++] = argv[i];
    }
    tty_init(no_color);
    int code = dispatch(n - 1, args + 1);
    fflush(stdout);
    tty_shutdown();
    free(args);
    return code;
}
