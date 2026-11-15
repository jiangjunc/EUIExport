#include "eui.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utf8.h"

/* ------------------------------------------------------------------ */
/* type table                                                          */
/* ------------------------------------------------------------------ */
static const EuiType kTypes[] = {
    {200, "Canvas", "画布(根)"},
    {10002, "Text", "文本"},
    {10003, "Image", "图片"},
    {10012, "Container", "容器/面板"},
    {20000, "SystemControl", "系统控件(摇杆/按钮)"},
    {21000, "PresetPanel", "预置面板"},
    {21101, "PresetPanel", "预置面板"},
    {21102, "PresetPanel", "预置面板"},
    {21103, "PresetPanel", "预置面板"},
    {21104, "PresetPanel", "预置面板"},
    {21105, "PresetPanel", "预置面板"},
    {21106, "PresetPanel", "预置面板"},
};
static const size_t kTypeCount = sizeof(kTypes) / sizeof(kTypes[0]);

const EuiType *eui_type_lookup(int64_t code) {
    for (size_t i = 0; i < kTypeCount; i++) {
        if (kTypes[i].code == code) return &kTypes[i];
    }
    return NULL;
}

const EuiType *eui_type_table(size_t *count) {
    *count = kTypeCount;
    return kTypes;
}

/* ------------------------------------------------------------------ */
/* field groups (mirrors the reference implementation)                 */
/* ------------------------------------------------------------------ */
static const char *const LAYOUT_FIELDS[] = {
    "anchor", "pos", "size", "rotation", "opacity", "flip_x", "flip_y",
    "auto_adaption", "need_horizontal",
};
static const char *const VISIBILITY_FIELDS[] = {
    "default_show", "default_show_by_player", "default_show_by_player_enabled",
    "spectate_hide", "take_photo_hide", "click_auto_hide", "auto_hide_enabled",
    "auto_hide_time", "editor_visible", "only_level_id", "vertical_screen", "visible",
};
static const char *const EVENT_FIELDS[] = {
    "event_list", "touch_begin_event", "touch_click_event", "touch_end_event",
    "long_touch_event", "show_event", "hide_event", "show_joystick_event",
    "hide_joystick_event", "touch_begin_audio", "touch_click_audio", "touch_end_audio",
    "reset_anim_event", "local_ui_event", "send_event_list", "listen_event_list",
    "scene_3d_ui_listen_event_list",
};
static const char *const BEHAVIOR_FIELDS[] = {
    "touch_enabled", "touch_drag_enabled", "long_touch_time", "animations",
    "default_play_anim", "preset_style", "is_locked", "lock", "has_name_changed",
    "native_layout_has_changed", "simplified_resize_mode", "enable_auto_size",
};
static const char *const TEXT_FIELDS[] = {
    "text", "font_path", "font_names", "font_size", "text_color", "text_h_alignment",
    "text_v_alignment", "text_spacing", "line_spacing", "line_count", "min_font_size",
    "enable_italic", "enable_outline", "outline_color", "outline_opacity", "outline_width",
    "enable_shadow", "shadow_color", "shadow_posx", "shadow_posy", "type_writer_enabled",
    "type_writer_speed", "rich_mode", "overflow_strategy", "num_auto_scroll_enabled",
    "reset_size_policy", "label_bind_attr", "label_bind_attr_type",
    "label_bind_attr_sub_type", "label_bind_attr_related_player_role_id",
    "label_bind_attr_related_creature_id", "label_equipment_bind_attr",
};
static const char *const IMAGE_FIELDS[] = {
    "image_path", "view_image_path", "image_color", "stretch_area",
};
static const char *const PANEL_FIELDS[] = {
    "background_color", "background_opacity", "corner_radius", "polygon_type",
    "polygon_points", "line_color", "line_width", "line_count", "fill_color",
    "gradient_enabled", "gradient_colors", "gradient_degree",
};

#define NELEM(x) (sizeof(x) / sizeof((x)[0]))

static const char *const kColorKeys[] = {
    "image_color", "text_color", "outline_color", "shadow_color",
    "background_color", "fill_color", "line_color",
};

static const char *const kUsedKeys[] = {
    "id", "name", "type", "cc_type", "z_order", "parent", "nodes", "inner_children",
    /* layout */
    "anchor", "pos", "size", "rotation", "opacity", "flip_x", "flip_y", "auto_adaption",
    "need_horizontal",
    /* visibility */
    "default_show", "default_show_by_player", "default_show_by_player_enabled",
    "spectate_hide", "take_photo_hide", "click_auto_hide", "auto_hide_enabled",
    "auto_hide_time", "editor_visible", "only_level_id", "vertical_screen", "visible",
    /* events */
    "event_list", "touch_begin_event", "touch_click_event", "touch_end_event",
    "long_touch_event", "show_event", "hide_event", "show_joystick_event",
    "hide_joystick_event", "touch_begin_audio", "touch_click_audio", "touch_end_audio",
    "reset_anim_event", "local_ui_event", "send_event_list", "listen_event_list",
    "scene_3d_ui_listen_event_list",
    /* behavior */
    "touch_enabled", "touch_drag_enabled", "long_touch_time", "animations",
    "default_play_anim", "preset_style", "is_locked", "lock", "has_name_changed",
    "native_layout_has_changed", "simplified_resize_mode", "enable_auto_size",
    /* text */
    "text", "font_path", "font_names", "font_size", "text_color", "text_h_alignment",
    "text_v_alignment", "text_spacing", "line_spacing", "line_count", "min_font_size",
    "enable_italic", "enable_outline", "outline_color", "outline_opacity", "outline_width",
    "enable_shadow", "shadow_color", "shadow_posx", "shadow_posy", "type_writer_enabled",
    "type_writer_speed", "rich_mode", "overflow_strategy", "num_auto_scroll_enabled",
    "reset_size_policy", "label_bind_attr", "label_bind_attr_type",
    "label_bind_attr_sub_type", "label_bind_attr_related_player_role_id",
    "label_bind_attr_related_creature_id", "label_equipment_bind_attr",
    /* image */
    "image_path", "view_image_path", "image_color", "stretch_area",
    /* panel */
    "background_color", "background_opacity", "corner_radius", "polygon_type",
    "polygon_points", "line_color", "line_width", "line_count", "fill_color",
    "gradient_enabled", "gradient_colors", "gradient_degree",
};

static bool is_used_key(const char *key) {
    for (size_t i = 0; i < NELEM(kUsedKeys); i++) {
        if (strcmp(kUsedKeys[i], key) == 0) return true;
    }
    return false;
}

static bool is_color_key(const char *key) {
    for (size_t i = 0; i < NELEM(kColorKeys); i++) {
        if (strcmp(kColorKeys[i], key) == 0) return true;
    }
    return false;
}

/* ------------------------------------------------------------------ */
/* helpers                                                             */
/* ------------------------------------------------------------------ */
J *j_from_mp(Arena *a, const MpVal *v) {
    if (!v) return j_null(a);
    switch (v->t) {
        case MP_NULL: return j_null(a);
        case MP_BOOL: return j_bool(a, v->u.b);
        case MP_INT: return j_int(a, v->u.i);
        case MP_DBL: return j_dbl(a, v->u.d);
        case MP_STR: return j_strn(a, v->u.s.p, v->u.s.n);
        case MP_ARR: {
            J *arr = j_arr(a);
            for (uint32_t i = 0; i < v->u.arr.n; i++) j_push(a, arr, j_from_mp(a, v->u.arr.items[i]));
            return arr;
        }
        case MP_MAP: {
            J *obj = j_obj(a);
            for (uint32_t i = 0; i < v->u.map.n; i++) {
                j_set(a, obj, v->u.map.keys[i], j_from_mp(a, v->u.map.vals[i]));
            }
            return obj;
        }
    }
    return j_null(a);
}

static double js_round(double x) { return floor(x + 0.5); }
static double round3(double n) { return js_round(n * 1000.0) / 1000.0; }

static bool mp_is_num(const MpVal *v) { return v && (v->t == MP_INT || v->t == MP_DBL); }
static double mp_num(const MpVal *v) { return v->t == MP_INT ? (double)v->u.i : v->u.d; }

static bool mp_is_tagged(const MpVal *v) {
    if (!v || v->t != MP_ARR || v->u.arr.n == 0) return false;
    const MpVal *last = v->u.arr.items[v->u.arr.n - 1];
    return last->t == MP_STR && last->u.s.n == 1 && (unsigned char)last->u.s.p[0] == 0x11;
}

static uint32_t mp_body_len(const MpVal *v) {
    if (!v || v->t != MP_ARR) return 0;
    return mp_is_tagged(v) ? v->u.arr.n - 1 : v->u.arr.n;
}

static bool all_numbers(const MpVal *arr, uint32_t len) {
    if (len == 0) return false;
    for (uint32_t i = 0; i < len; i++) {
        if (!mp_is_num(arr->u.arr.items[i])) return false;
    }
    return true;
}

/* JavaScript String(value) */
static char *js_string_of(Arena *a, const MpVal *v) {
    char buf[64];
    if (!v) return arena_strdup(a, "undefined");
    switch (v->t) {
        case MP_NULL: return arena_strdup(a, "null");
        case MP_BOOL: return arena_strdup(a, v->u.b ? "true" : "false");
        case MP_INT:
            snprintf(buf, sizeof buf, "%lld", (long long)v->u.i);
            return arena_strdup(a, buf);
        case MP_DBL:
            js_number_to_str(v->u.d, buf);
            return arena_strdup(a, buf);
        case MP_STR: return arena_strndup(a, v->u.s.p, v->u.s.n);
        case MP_ARR: {
            SB sb;
            sb_init(&sb);
            for (uint32_t i = 0; i < v->u.arr.n; i++) {
                if (i) sb_putc(&sb, ',');
                const MpVal *e = v->u.arr.items[i];
                if (e->t == MP_NULL) {
                    /* Array#toString renders null/undefined as empty */
                } else {
                    sb_puts(&sb, js_string_of(a, e));
                }
            }
            char *out = arena_strdup(a, sb_cstr(&sb));
            sb_free(&sb);
            return out;
        }
        default: return arena_strdup(a, "[object Object]");
    }
}

static bool type_code_of(const MpVal *v, int64_t *out) {
    if (!v) return false;
    if (v->t == MP_INT) {
        *out = v->u.i;
        return true;
    }
    if (v->t == MP_DBL) {
        if (v->u.d == floor(v->u.d)) {
            *out = (int64_t)v->u.d;
            return true;
        }
        return false;
    }
    if (v->t == MP_STR && v->u.s.n > 0) {
        char tmp[24];
        if (v->u.s.n >= sizeof tmp) return false;
        memcpy(tmp, v->u.s.p, v->u.s.n);
        tmp[v->u.s.n] = '\0';
        char *end = NULL;
        long long code = strtoll(tmp, &end, 10);
        if (end && *end == '\0') {
            *out = (int64_t)code;
            return true;
        }
    }
    return false;
}

/* ------------------------------------------------------------------ */
/* humanize                                                            */
/* ------------------------------------------------------------------ */
static J *humanize(Arena *a, const MpVal *v, const char *key);

static bool is_cjk_cp(uint32_t cp) {
    return (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0x3000 && cp <= 0x303F) ||
           (cp >= 0xFF00 && cp <= 0xFFEF);
}

static J *maybe_decode_text(Arena *a, const char *s, size_t n) {
    if (n < 16) return j_strn(a, s, n);
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                  c == '+' || c == '/' || c == '=' || c == '\r' || c == '\n';
        if (!ok) return j_strn(a, s, n);
    }
    size_t padded_n = n;
    size_t rem = padded_n % 4;
    size_t buf_cap = (padded_n + 3) / 4 * 3 + 4;
    char *padded = (char *)arena_alloc(a, padded_n + 4);
    unsigned char *buf = (unsigned char *)arena_alloc(a, buf_cap);
    if (!padded || !buf) return j_strn(a, s, n);
    memcpy(padded, s, n);
    if (rem) {
        for (size_t i = rem; i < 4; i++) padded[padded_n++] = '=';
    }
    padded[padded_n] = '\0';
    size_t got = base64_decode(padded, padded_n, buf, buf_cap);
    if (got == 0 || got == (size_t)-1) return j_strn(a, s, n);
    if (!utf8_valid((const char *)buf, got)) return j_strn(a, s, n);

    size_t printable = 0, total = 0;
    bool has_cjk = false;
    size_t pos = 0;
    while (pos < got) {
        uint32_t cp = utf8_decode1((const char *)buf, got, &pos, NULL);
        total++;
        if ((cp >= 0x20 && cp <= 0x7E) || cp >= 0x80) printable++;
        if (is_cjk_cp(cp)) has_cjk = true;
    }
    if (total == 0) return j_strn(a, s, n);
    if ((double)printable / (double)total <= 0.9) return j_strn(a, s, n);
    if (!has_cjk) return j_strn(a, s, n);
    return j_strn(a, (const char *)buf, got);
}

/* Number#toString(16) then left pad with '0' to width 2. */
static void hex_pair_signed(int v, char out[16]) {
    if (v < 0) snprintf(out, 16, "-%x", -v);
    else snprintf(out, 16, "%x", v);
    if (out[0] != '-' && strlen(out) < 2) {
        char tmp[16];
        snprintf(tmp, sizeof tmp, "0%s", out);
        strcpy(out, tmp);
    }
}

static J *color_obj(Arena *a, const MpVal *arr, uint32_t len) {
    double comp[4] = {0, 0, 0, 255};
    for (uint32_t i = 0; i < len && i < 4; i++) comp[i] = js_round(round3(mp_num(arr->u.arr.items[i])));
    int r = (int)comp[0], g = (int)comp[1], b = (int)comp[2];
    bool has_alpha = len >= 4;
    int alpha = has_alpha ? (int)comp[3] : 255;

    char hr[16], hg[16], hb[16], ha[16];
    hex_pair_signed(r, hr);
    hex_pair_signed(g, hg);
    hex_pair_signed(b, hb);
    char hex[64];
    snprintf(hex, sizeof hex, "#%s%s%s", hr, hg, hb);
    if (has_alpha && alpha < 255) {
        hex_pair_signed(alpha, ha);
        strncat(hex, ha, sizeof hex - strlen(hex) - 1);
    }
    J *o = j_obj(a);
    j_set(a, o, "r", j_int(a, r));
    j_set(a, o, "g", j_int(a, g));
    j_set(a, o, "b", j_int(a, b));
    j_set(a, o, "hex", j_str(a, hex));
    if (has_alpha) j_set(a, o, "a", j_int(a, alpha));
    return o;
}

static J *humanize_array(Arena *a, const MpVal *v, const char *key) {
    uint32_t body_len = mp_body_len(v);
    bool nums = all_numbers(v, body_len);

    if (nums && key && is_color_key(key) && (body_len == 3 || body_len == 4)) {
        return color_obj(a, v, body_len);
    }
    if (key && strcmp(key, "gradient_colors") == 0 && body_len > 0) {
        bool all_arr = true;
        for (uint32_t i = 0; i < body_len; i++) {
            if (v->u.arr.items[i]->t != MP_ARR) all_arr = false;
        }
        if (all_arr) {
            J *arr = j_arr(a);
            for (uint32_t i = 0; i < body_len; i++) {
                const MpVal *e = v->u.arr.items[i];
                /* JS compares the raw element length here (tag included), so a
                   tagged [r,g,b,a,tag] stays a raw array. */
                uint32_t elen = e->u.arr.n;
                if ((elen == 3 || elen == 4) && all_numbers(e, elen)) {
                    j_push(a, arr, color_obj(a, e, elen));
                } else {
                    j_push(a, arr, j_from_mp(a, e));
                }
            }
            return arr;
        }
    }
    if (key && strcmp(key, "size") == 0 && body_len == 2 && nums) {
        J *o = j_obj(a);
        j_set(a, o, "w", j_dbl(a, round3(mp_num(v->u.arr.items[0]))));
        j_set(a, o, "h", j_dbl(a, round3(mp_num(v->u.arr.items[1]))));
        return o;
    }
    if (body_len == 2 && nums) {
        J *o = j_obj(a);
        j_set(a, o, "x", j_dbl(a, round3(mp_num(v->u.arr.items[0]))));
        j_set(a, o, "y", j_dbl(a, round3(mp_num(v->u.arr.items[1]))));
        return o;
    }
    if (body_len == 3 && nums) {
        J *o = j_obj(a);
        j_set(a, o, "x", j_dbl(a, round3(mp_num(v->u.arr.items[0]))));
        j_set(a, o, "y", j_dbl(a, round3(mp_num(v->u.arr.items[1]))));
        j_set(a, o, "z", j_dbl(a, round3(mp_num(v->u.arr.items[2]))));
        return o;
    }
    {
        /* note: the tail marker (if any) is kept, exactly like the reference */
        J *arr = j_arr(a);
        for (uint32_t i = 0; i < v->u.arr.n; i++) {
            char sub[256];
            snprintf(sub, sizeof sub, "%s[%u]", key ? key : "", i);
            j_push(a, arr, humanize(a, v->u.arr.items[i], sub));
        }
        return arr;
    }
}

static J *humanize(Arena *a, const MpVal *v, const char *key) {
    if (!v) return j_null(a);
    switch (v->t) {
        case MP_NULL: return j_null(a);
        case MP_BOOL: return j_bool(a, v->u.b);
        case MP_INT: return j_int(a, v->u.i);
        case MP_DBL: return j_dbl(a, v->u.d);
        case MP_STR: return maybe_decode_text(a, v->u.s.p, v->u.s.n);
        case MP_ARR: return humanize_array(a, v, key);
        case MP_MAP: {
            J *o = j_obj(a);
            for (uint32_t i = 0; i < v->u.map.n; i++) {
                j_set(a, o, v->u.map.keys[i], humanize(a, v->u.map.vals[i], v->u.map.keys[i]));
            }
            return o;
        }
    }
    return j_null(a);
}

/* ------------------------------------------------------------------ */
/* field picking                                                       */
/* ------------------------------------------------------------------ */
static const char *h_align_label(double v, const char *fallback) {
    int k = (int)v;
    if ((double)k == v) {
        if (k == 0) return "left";
        if (k == 1) return "center";
        if (k == 2) return "right";
    }
    return fallback;
}

static const char *v_align_label(double v, const char *fallback) {
    int k = (int)v;
    if ((double)k == v) {
        if (k == 0) return "top";
        if (k == 1) return "center";
        if (k == 2) return "bottom";
    }
    return fallback;
}

static J *pick_fields(Arena *a, const MpVal *raw, const char *const *fields, size_t n) {
    J *out = j_obj(a);
    if (!raw || raw->t != MP_MAP) return out;
    for (size_t i = 0; i < n; i++) {
        const char *k = fields[i];
        const MpVal *rv = mp_get(raw, k);
        if (!rv) continue;
        J *v = humanize(a, rv, k);
        if (v && (v->t == J_INT || v->t == J_DBL)) {
            bool is_h = strcmp(k, "text_h_alignment") == 0;
            bool is_v = strcmp(k, "text_v_alignment") == 0;
            if (is_h || is_v) {
                double num = v->t == J_INT ? (double)v->u.i : v->u.d;
                char numbuf[32];
                js_number_to_str(num, numbuf);
                char unknown[48];
                snprintf(unknown, sizeof unknown, "unknown(%s)", numbuf);
                const char *label = is_h ? h_align_label(num, unknown) : v_align_label(num, unknown);
                J *wrapped = j_obj(a);
                j_set(a, wrapped, "value", v);
                j_set(a, wrapped, "label", j_str(a, label));
                v = wrapped;
            }
        }
        j_set(a, out, k, v);
    }
    return out;
}

static void merge_into(Arena *a, J *dst, const J *src) {
    if (!src || src->t != J_OBJ) return;
    for (size_t i = 0; i < src->u.obj.n; i++) {
        j_set(a, dst, src->u.obj.keys[i], src->u.obj.vals[i]);
    }
}

static bool obj_non_empty(const J *o) { return o && o->t == J_OBJ && o->u.obj.n > 0; }

/* ------------------------------------------------------------------ */
/* node export                                                         */
/* ------------------------------------------------------------------ */
static J *export_node(Arena *a, const MpVal *raw);

static J *export_children(Arena *a, const MpVal *nodes) {
    J *arr = j_arr(a);
    for (uint32_t i = 0; i < nodes->u.arr.n; i++) {
        j_push(a, arr, export_node(a, nodes->u.arr.items[i]));
    }
    return arr;
}

static J *text_fields_of(Arena *a, const MpVal *raw) {
    return pick_fields(a, raw, TEXT_FIELDS, NELEM(TEXT_FIELDS));
}

static J *panel_plus_layout(Arena *a, const MpVal *raw) {
    J *out = j_obj(a);
    J *panel = pick_fields(a, raw, PANEL_FIELDS, NELEM(PANEL_FIELDS));
    merge_into(a, out, panel);
    J *layout = pick_fields(a, raw, LAYOUT_FIELDS, NELEM(LAYOUT_FIELDS));
    merge_into(a, out, layout);
    return out;
}

static J *export_node(Arena *a, const MpVal *raw) {
    if (!raw || raw->t == MP_NULL) {
        J *n = j_obj(a);
        j_set(a, n, "id", j_str(a, ""));
        j_set(a, n, "name", j_str(a, "(空节点)"));
        j_set(a, n, "type", j_str(a, "null"));
        j_set(a, n, "type_code", j_null(a));
        return n;
    }
    if (raw->t != MP_MAP) {
        J *n = j_obj(a);
        j_set(a, n, "id", j_str(a, ""));
        j_set(a, n, "name", j_str(a, ""));
        char tbuf[96];
        char *ts = js_string_of(a, mp_get(raw, "type"));
        snprintf(tbuf, sizeof tbuf, "Unknown(%s)", ts ? ts : "undefined");
        j_set(a, n, "type", j_str(a, tbuf));
        j_set(a, n, "type_zh", j_str(a, "未知类型"));
        return n;
    }

    J *node = j_obj(a);

    const MpVal *id_v = mp_get(raw, "id");
    if (id_v) {
        j_set(a, node, "id", j_str(a, js_string_of(a, id_v)));
    } else {
        j_set(a, node, "id", j_str(a, ""));
    }

    const MpVal *name_v = mp_get(raw, "name");
    j_set(a, node, "name", name_v && name_v->t == MP_STR ? j_strn(a, name_v->u.s.p, name_v->u.s.n)
                                                         : j_str(a, ""));

    const MpVal *type_v = mp_get(raw, "type");
    int64_t code = 0;
    const EuiType *ti = type_code_of(type_v, &code) ? eui_type_lookup(code) : NULL;
    char tbuf[96];
    if (ti) {
        j_set(a, node, "type", j_str(a, ti->name));
    } else {
        char *ts = js_string_of(a, type_v);
        snprintf(tbuf, sizeof tbuf, "Unknown(%s)", ts ? ts : "undefined");
        j_set(a, node, "type", j_str(a, tbuf));
    }
    if (type_v) j_set(a, node, "type_code", j_from_mp(a, type_v));
    j_set(a, node, "type_zh", j_str(a, ti ? ti->zh : "未知类型"));

    const MpVal *cc_v = mp_get(raw, "cc_type");
    if (cc_v) {
        bool truthy = true;
        if (cc_v->t == MP_NULL) truthy = false;
        else if (cc_v->t == MP_BOOL) truthy = cc_v->u.b;
        else if (mp_is_num(cc_v)) truthy = mp_num(cc_v) != 0;
        else if (cc_v->t == MP_STR && cc_v->u.s.n == 0) truthy = false;
        if (truthy) j_set(a, node, "cc_type", j_from_mp(a, cc_v));
    }

    const MpVal *z_v = mp_get(raw, "z_order");
    if (z_v && mp_is_num(z_v)) j_set(a, node, "z_order", j_from_mp(a, z_v));

    const MpVal *parent_v = mp_get(raw, "parent");
    if (parent_v) j_set(a, node, "parent", j_str(a, js_string_of(a, parent_v)));

    J *layout = pick_fields(a, raw, LAYOUT_FIELDS, NELEM(LAYOUT_FIELDS));
    if (obj_non_empty(layout)) j_set(a, node, "layout", layout);
    J *vis = pick_fields(a, raw, VISIBILITY_FIELDS, NELEM(VISIBILITY_FIELDS));
    if (obj_non_empty(vis)) j_set(a, node, "visibility", vis);
    J *img = pick_fields(a, raw, IMAGE_FIELDS, NELEM(IMAGE_FIELDS));
    if (obj_non_empty(img)) j_set(a, node, "image", img);
    J *panel = pick_fields(a, raw, PANEL_FIELDS, NELEM(PANEL_FIELDS));
    if (obj_non_empty(panel)) j_set(a, node, "panel", panel);
    J *events = pick_fields(a, raw, EVENT_FIELDS, NELEM(EVENT_FIELDS));
    if (obj_non_empty(events)) j_set(a, node, "events", events);
    J *behavior = pick_fields(a, raw, BEHAVIOR_FIELDS, NELEM(BEHAVIOR_FIELDS));
    if (obj_non_empty(behavior)) j_set(a, node, "behavior", behavior);

    const MpVal *inner = mp_get(raw, "inner_children");
    bool is_text = false;
    if (type_code_of(type_v, &code) && code == 10002) is_text = true;
    bool has_text = false;

    if (is_text) {
        J *text_top = text_fields_of(a, raw);
        if (inner && (inner->t == MP_MAP || inner->t == MP_ARR)) {
            const MpVal *tc = mp_get(inner, "Text_content");
            const MpVal *rt = mp_get(inner, "RichTextEx");
            const MpVal *bg = mp_get(inner, "Panel_bg");
            if (tc) {
                J *merged = j_obj(a);
                merge_into(a, merged, text_top);
                J *extra = text_fields_of(a, tc);
                merge_into(a, merged, extra);
                j_set(a, node, "text", merged);
                has_text = true;
            }
            if (rt) j_set(a, node, "rich_text", text_fields_of(a, rt));
            if (bg) j_set(a, node, "background", panel_plus_layout(a, bg));
        }
        if (!has_text && obj_non_empty(text_top)) j_set(a, node, "text", text_top);
    } else if (inner && (inner->t == MP_MAP || inner->t == MP_ARR)) {
        const MpVal *bg = mp_get(inner, "Panel_bg");
        if (bg) j_set(a, node, "background", panel_plus_layout(a, bg));
    }

    {
        J *other = j_obj(a);
        for (uint32_t i = 0; i < raw->u.map.n; i++) {
            const char *k = raw->u.map.keys[i];
            if (is_used_key(k)) continue;
            j_set(a, other, k, humanize(a, raw->u.map.vals[i], k));
        }
        if (obj_non_empty(other)) j_set(a, node, "other", other);
    }

    const MpVal *nodes = mp_get(raw, "nodes");
    if (nodes && nodes->t == MP_ARR && nodes->u.arr.n > 0) {
        j_set(a, node, "children", export_children(a, nodes));
    }
    return node;
}

/* ------------------------------------------------------------------ */
/* scene export                                                        */
/* ------------------------------------------------------------------ */
static const MpVal *find_scene_map(const MpVal *root, Err *e) {
    if (!root || root->t != MP_ARR) {
        err_set(e, "EUI 数据结构未知：根对象不是数组");
        return NULL;
    }
    if (root->u.arr.n == 0) {
        err_set(e, "EUI 数据为空。请在蛋仔编辑器中打开 UI 场景并保存后重试。");
        return NULL;
    }
    const MpVal *picked = NULL;
    for (uint32_t i = 0; i < root->u.arr.n; i++) {
        const MpVal *el = root->u.arr.items[i];
        if (el->t == MP_MAP) {
            picked = el;
            break;
        }
    }
    if (!picked) picked = root->u.arr.items[0];
    if (!picked || picked->t != MP_MAP) {
        err_set(e, "EUI 数据顶层结构异常（%s）", picked && picked->t == MP_ARR ? "数组" : "非对象");
        return NULL;
    }
    return picked;
}

/* parent key: '' for missing / null / empty */
static char *parent_key(Arena *a, const MpVal *n) {
    const MpVal *p = mp_get(n, "parent");
    if (!p || p->t == MP_NULL) return arena_strdup(a, "");
    if (p->t == MP_STR && p->u.s.n == 0) return arena_strdup(a, "");
    return js_string_of(a, p);
}

typedef struct Group {
    char *key;
    PVec nodes;
} Group;

typedef struct GroupSet {
    Group *items;
    size_t n, cap;
} GroupSet;

static Group *group_find(GroupSet *g, const char *key) {
    for (size_t i = 0; i < g->n; i++) {
        if (strcmp(g->items[i].key, key) == 0) return &g->items[i];
    }
    return NULL;
}

static void group_add(Arena *a, GroupSet *g, const char *key, void *item) {
    Group *grp = group_find(g, key);
    if (!grp) {
        if (g->n == g->cap) {
            size_t ncap = g->cap ? g->cap * 2 : 32;
            Group *ni = (Group *)arena_alloc(a, ncap * sizeof(Group));
            if (!ni) return;
            if (g->n) memcpy(ni, g->items, g->n * sizeof(Group));
            g->items = ni;
            g->cap = ncap;
        }
        grp = &g->items[g->n++];
        grp->key = arena_strdup(a, key);
        grp->nodes.items = NULL;
        grp->nodes.n = 0;
        grp->nodes.cap = 0;
    }
    pvec_push(a, &grp->nodes, item);
}

typedef struct Ctx {
    Arena *a;
    GroupSet children; /* parent id -> nodes */
    GroupSet ids;      /* every node id */
    MpVal **roots;
    size_t root_n, root_cap;
} Ctx;

static void ctx_add_root(Ctx *c, MpVal *n) {
    if (c->root_n == c->root_cap) {
        size_t ncap = c->root_cap ? c->root_cap * 2 : 16;
        MpVal **ni = (MpVal **)arena_alloc(c->a, ncap * sizeof(MpVal *));
        if (!ni) return;
        if (c->root_n) memcpy(ni, c->roots, c->root_n * sizeof(MpVal *));
        c->roots = ni;
        c->root_cap = ncap;
    }
    c->roots[c->root_n++] = n;
}

#define EUI_MAX_DEPTH 512

static J *build_node(Ctx *c, MpVal *raw, int depth);

static J *build_children(Ctx *c, const PVec *kids, int depth) {
    J *children = j_arr(c->a);
    for (size_t i = 0; i < kids->n; i++) {
        j_push(c->a, children, build_node(c, (MpVal *)kids->items[i], depth + 1));
    }
    return children;
}

static J *build_node(Ctx *c, MpVal *raw, int depth) {
    J *node = export_node(c->a, raw);
    if (depth >= EUI_MAX_DEPTH) return node;
    char *id = js_string_of(c->a, mp_get(raw, "id"));
    Group *kids = id ? group_find(&c->children, id) : NULL;
    if (kids && kids->nodes.n) {
        j_set(c->a, node, "children", build_children(c, &kids->nodes, depth));
    } else {
        const MpVal *own = mp_get(raw, "nodes");
        if (own && own->t == MP_ARR && own->u.arr.n > 0) {
            PVec v = {0};
            for (uint32_t i = 0; i < own->u.arr.n; i++) pvec_push(c->a, &v, own->u.arr.items[i]);
            j_set(c->a, node, "children", build_children(c, &v, depth));
        }
    }
    return node;
}

J *eui_export_scene(Arena *a, const MpVal *root, Err *e) {
    const MpVal *scene_map = find_scene_map(root, e);
    if (!scene_map) return NULL;

    J *scene = j_obj(a);
    const MpVal *type_v = mp_get(scene_map, "type");
    const MpVal *ver_v = mp_get(scene_map, "version");
    if (type_v) j_set(a, scene, "type", j_from_mp(a, type_v));
    if (ver_v) j_set(a, scene, "version", j_from_mp(a, ver_v));

    const MpVal *id_v = mp_get(scene_map, "id");
    if (id_v) j_set(a, scene, "id", j_str(a, js_string_of(a, id_v)));
    const MpVal *canvas_ids = mp_get(scene_map, "canvas_layer_ids");
    if (canvas_ids && canvas_ids->t == MP_ARR) {
        j_set(a, scene, "canvas_layer_ids", j_from_mp(a, canvas_ids));
    }
    const MpVal *design = mp_get(scene_map, "design_size");
    if (design) j_set(a, scene, "design_size", humanize(a, design, "design_size"));
    const MpVal *vertical = mp_get(scene_map, "vertical_screen");
    if (vertical && vertical->t == MP_BOOL) j_set(a, scene, "vertical_screen", j_bool(a, vertical->u.b));
    const MpVal *simplified = mp_get(scene_map, "simplified_resize_mode");
    if (simplified && simplified->t == MP_BOOL) {
        j_set(a, scene, "simplified_resize_mode", j_bool(a, simplified->u.b));
    }

    PVec all = {0};
    const MpVal *nodes_v = mp_get(scene_map, "nodes");
    if (nodes_v && nodes_v->t == MP_ARR) {
        for (uint32_t i = 0; i < nodes_v->u.arr.n; i++) {
            const MpVal *n = nodes_v->u.arr.items[i];
            if (n->t == MP_MAP) pvec_push(a, &all, (void *)n);
        }
    }
    const MpVal *joy_v = mp_get(scene_map, "joysticks");
    if (joy_v && joy_v->t == MP_ARR) {
        for (uint32_t i = 0; i < joy_v->u.arr.n; i++) {
            const MpVal *n = joy_v->u.arr.items[i];
            if (n->t == MP_MAP) pvec_push(a, &all, (void *)n);
        }
    }

    Ctx ctx;
    memset(&ctx, 0, sizeof ctx);
    ctx.a = a;

    for (size_t i = 0; i < all.n; i++) {
        MpVal *n = (MpVal *)all.items[i];
        char *id = js_string_of(a, mp_get(n, "id"));
        if (id) group_add(a, &ctx.ids, id, n);
        char *pid = parent_key(a, n);
        if (pid) group_add(a, &ctx.children, pid, n);
    }
    for (size_t i = 0; i < all.n; i++) {
        MpVal *n = (MpVal *)all.items[i];
        char *pid = parent_key(a, n);
        bool is_root = !pid || !*pid || !group_find(&ctx.ids, pid);
        if (is_root) ctx_add_root(&ctx, n);
    }

    J *out_nodes = j_arr(a);
    for (size_t i = 0; i < ctx.root_n; i++) j_push(a, out_nodes, build_node(&ctx, ctx.roots[i], 0));
    j_set(a, scene, "nodes", out_nodes);

    int64_t joy_count = 0;
    if (joy_v && joy_v->t == MP_ARR) joy_count = (int64_t)joy_v->u.arr.n;
    j_set(a, scene, "joystick_count", j_int(a, joy_count));
    return scene;
}

/* ------------------------------------------------------------------ */
/* statistics                                                          */
/* ------------------------------------------------------------------ */
static void count_walk(Arena *a, J *type_counts, const J *node, int depth, EuiStats *st) {
    if (!node || node->t != J_OBJ) return;
    st->count++;
    if (depth > st->max_depth) st->max_depth = depth;
    J *type_j = j_get(node, "type");
    const char *type = (type_j && type_j->t == J_STR) ? type_j->u.s.p : "null";
    if (str_eq(type, "Canvas")) st->canvas_count++;
    J *prev = j_get(type_counts, type);
    if (prev && prev->t == J_INT) {
        j_set(a, type_counts, type, j_int(a, prev->u.i + 1));
    } else {
        j_set(a, type_counts, type, j_int(a, 1));
    }
    J *kids = j_get(node, "children");
    if (kids && kids->t == J_ARR) {
        for (size_t i = 0; i < kids->u.arr.n; i++) {
            count_walk(a, type_counts, kids->u.arr.items[i], depth + 1, st);
        }
    }
}

void eui_count_nodes(Arena *a, const J *scene, EuiStats *out) {
    memset(out, 0, sizeof *out);
    J *tc = j_obj(a);
    const J *nodes = j_get(scene, "nodes");
    if (nodes && nodes->t == J_ARR) {
        for (size_t i = 0; i < nodes->u.arr.n; i++) count_walk(a, tc, nodes->u.arr.items[i], 0, out);
    }
    J *joy = j_get(scene, "joystick_count");
    if (joy && joy->t == J_INT) out->joystick_count = joy->u.i;
    out->type_counts = tc;
}

/* ------------------------------------------------------------------ */
/* tree text                                                           */
/* ------------------------------------------------------------------ */
static const char *str_field(const J *o, const char *key) {
    J *v = j_get(o, key);
    return (v && v->t == J_STR) ? v->u.s.p : NULL;
}

static double num_field(const J *o, const char *key, bool *has) {
    J *v = j_get(o, key);
    *has = false;
    if (!v) return 0;
    if (v->t == J_INT) {
        *has = true;
        return (double)v->u.i;
    }
    if (v->t == J_DBL) {
        *has = true;
        return v->u.d;
    }
    return 0;
}

/* Opacity is rendered as `${n}` and only when it differs from 1. */
static void append_opacity(SB *sb, const J *n) {
    J *layout = j_get(n, "layout");
    if (!layout || layout->t != J_OBJ) return;
    bool has = false;
    double op = num_field(layout, "opacity", &has);
    if (!has || op == 1.0) return;
    char buf[32];
    js_number_to_str(op, buf);
    sb_printf(sb, "  α=%s", buf);
}

static void append_image(SB *sb, const J *n) {
    J *img = j_get(n, "image");
    if (!img || img->t != J_OBJ) return;
    J *path = j_get(img, "image_path");
    if (!path) return;
    if (path->t == J_STR) {
        if (!path->u.s.p || !*path->u.s.p) return;
        sb_printf(sb, "  img=%s", path->u.s.p);
    } else if (path->t == J_INT) {
        if (path->u.i == 0) return;
        char buf[32];
        js_number_to_str((double)path->u.i, buf);
        sb_printf(sb, "  img=%s", buf);
    } else if (path->t == J_DBL) {
        if (path->u.d == 0) return;
        char buf[32];
        js_number_to_str(path->u.d, buf);
        sb_printf(sb, "  img=%s", buf);
    }
}

/* ------------------------------------------------------------------ */
/* markdown tree                                                       */
/* ------------------------------------------------------------------ */
static bool js_is_space(uint32_t cp) {
    return cp == 0x09 || cp == 0x0A || cp == 0x0B || cp == 0x0C || cp == 0x0D || cp == 0x20 ||
           cp == 0xA0 || cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200A) || cp == 0x2028 ||
           cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000 || cp == 0xFEFF;
}

static void append_text_md(SB *sb, const J *n) {
    J *text = j_get(n, "text");
    if (!text || text->t != J_OBJ) return;
    const char *p = str_field(text, "text");
    if (!p || !*p) return;
    size_t len = strlen(p);
    size_t cap = len + 4;
    uint32_t *cps = (uint32_t *)malloc(cap * sizeof(uint32_t));
    if (!cps) return;
    size_t cn = 0;
    size_t pos = 0;
    bool prev_space = false;
    while (pos < len) {
        uint32_t cp = utf8_decode1(p, len, &pos, NULL);
        if (js_is_space(cp)) {
            if (!prev_space) {
                cps[cn++] = ' ';
                prev_space = true;
            }
        } else {
            cps[cn++] = cp;
            prev_space = false;
        }
    }
    /* trim */
    size_t start = 0, end = cn;
    while (start < end && cps[start] == ' ') start++;
    while (end > start && cps[end - 1] == ' ') end--;

    sb_puts(sb, "  文本=\"");
    size_t units = 0;
    bool truncated = false;
    for (size_t i = start; i < end; i++) {
        unsigned u = cps[i] >= 0x10000 ? 2u : 1u;
        if (units + u > 40) {
            truncated = true;
            break;
        }
        char enc[4];
        size_t el = utf8_encode(cps[i], enc);
        sb_write(sb, enc, el);
        units += u;
    }
    if (truncated) sb_puts(sb, "…");
    sb_puts(sb, "\"");
    free(cps);
}

static void md_walk(SB *sb, const J *nodes, int depth, const char *prefix, size_t *line_count) {
    if (!nodes || nodes->t != J_ARR) return;
    for (size_t i = 0; i < nodes->u.arr.n; i++) {
        const J *n = nodes->u.arr.items[i];
        bool last = (i + 1 == nodes->u.arr.n);
        const char *connector = depth == 0 ? "" : (last ? "└─ " : "├─ ");
        const char *name = str_field(n, "name");
        const char *type = str_field(n, "type");
        const char *id = str_field(n, "id");
        sb_puts(sb, depth == 0 ? "" : prefix);
        sb_puts(sb, connector);
        sb_printf(sb, "「%s」 [%s #%s]", (name && *name) ? name : "(未命名)", type ? type : "null",
                  id ? id : "");
        append_text_md(sb, n);
        append_image(sb, n);
        append_opacity(sb, n);
        {
            J *bg = j_get(n, "background");
            if (bg && bg->t == J_OBJ) {
                J *bc = j_get(bg, "background_color");
                if (bc && bc->t == J_OBJ) {
                    const char *hex = str_field(bc, "hex");
                    if (hex) sb_printf(sb, "  背景=%s", hex);
                }
            }
        }
        sb_putc(sb, '\n');
        (*line_count)++;

        J *kids = j_get(n, "children");
        if (kids && kids->t == J_ARR && kids->u.arr.n) {
            SB next;
            sb_init(&next);
            if (depth != 0) {
                sb_puts(&next, prefix);
                sb_puts(&next, last ? "   " : "│  ");
            }
            md_walk(sb, kids, depth + 1, sb_cstr(&next), line_count);
            sb_free(&next);
        }
    }
}

void eui_tree_markdown(SB *sb, const J *scene, size_t *line_count) {
    size_t lines = 0;
    md_walk(sb, j_get(scene, "nodes"), 0, "", &lines);
    if (line_count) *line_count = lines;
}

/* ------------------------------------------------------------------ */
/* node index                                                          */
/* ------------------------------------------------------------------ */
typedef struct IdEntry {
    const char *id;
    J *value;
} IdEntry;

typedef struct IndexCtx {
    Arena *a;
    IdEntry *entries;
    size_t n, cap;
} IndexCtx;

static bool is_array_index_key(const char *s) {
    size_t n = strlen(s);
    if (n == 0 || n > 10) return false;
    if (s[0] == '0' && n > 1) return false;
    uint64_t v = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') return false;
        v = v * 10 + (uint64_t)(s[i] - '0');
    }
    return v <= 4294967294ull;
}

static void index_set(IndexCtx *c, const char *id, J *value) {
    for (size_t i = 0; i < c->n; i++) {
        if (strcmp(c->entries[i].id, id) == 0) {
            c->entries[i].value = value;
            return;
        }
    }
    if (c->n == c->cap) {
        size_t ncap = c->cap ? c->cap * 2 : 32;
        IdEntry *ni = (IdEntry *)arena_alloc(c->a, ncap * sizeof(IdEntry));
        if (!ni) return;
        if (c->n) memcpy(ni, c->entries, c->n * sizeof(IdEntry));
        c->entries = ni;
        c->cap = ncap;
    }
    c->entries[c->n].id = id;
    c->entries[c->n].value = value;
    c->n++;
}

static void index_walk(IndexCtx *c, const J *nodes, const char *crumb) {
    if (!nodes || nodes->t != J_ARR) return;
    for (size_t i = 0; i < nodes->u.arr.n; i++) {
        const J *n = nodes->u.arr.items[i];
        const char *name = str_field(n, "name");
        const char *id = str_field(n, "id");
        const char *shown = (name && *name) ? name : "(未命名)";
        char *path;
        if (crumb && *crumb) path = arena_printf(c->a, "%s > %s", crumb, shown);
        else path = arena_strdup(c->a, shown);
        if (id && *id) {
            J *entry = j_obj(c->a);
            j_set(c->a, entry, "name", j_str(c->a, name ? name : ""));
            const char *type = str_field(n, "type");
            j_set(c->a, entry, "type", j_str(c->a, type ? type : "null"));
            const char *zh = str_field(n, "type_zh");
            if (zh) j_set(c->a, entry, "type_zh", j_str(c->a, zh));
            j_set(c->a, entry, "path", j_str(c->a, path));
            index_set(c, id, entry);
        }
        index_walk(c, j_get(n, "children"), path);
    }
}

static int cmp_id_entry(const void *pa, const void *pb) {
    const IdEntry *a = (const IdEntry *)pa;
    const IdEntry *b = (const IdEntry *)pb;
    unsigned long long va = strtoull(a->id, NULL, 10);
    unsigned long long vb = strtoull(b->id, NULL, 10);
    if (va < vb) return -1;
    if (va > vb) return 1;
    return 0;
}

J *eui_build_node_index(Arena *a, const J *scene) {
    IndexCtx c;
    memset(&c, 0, sizeof c);
    c.a = a;
    index_walk(&c, j_get(scene, "nodes"), "");
    IdEntry *ints = (IdEntry *)arena_alloc(a, (c.n + 1) * sizeof(IdEntry));
    IdEntry *strs = (IdEntry *)arena_alloc(a, (c.n + 1) * sizeof(IdEntry));
    size_t ni = 0, ns = 0;
    for (size_t i = 0; i < c.n; i++) {
        if (is_array_index_key(c.entries[i].id)) ints[ni++] = c.entries[i];
        else strs[ns++] = c.entries[i];
    }
    qsort(ints, ni, sizeof(IdEntry), cmp_id_entry);
    J *obj = j_obj(a);
    for (size_t i = 0; i < ni; i++) j_set(a, obj, ints[i].id, ints[i].value);
    for (size_t i = 0; i < ns; i++) j_set(a, obj, strs[i].id, strs[i].value);
    return obj;
}
