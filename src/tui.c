/* tui.c - boxes, tables and the interactive fuzzy picker. */
#include "tui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tty.h"
#include "utf8.h"

/* ------------------------------------------------------------------ */
/* small helpers                                                       */
/* ------------------------------------------------------------------ */
static int dwidth(const char *s) { return s ? display_width(s, strlen(s)) : 0; }

static void sb_spaces(SB *b, int n) {
    if (n > 0) sb_repeat(b, ' ', (size_t)n);
}

/* Repeats a multi-byte rule character n times. */
static void sb_rule(SB *b, const char *ch, int n) {
    for (int i = 0; i < n; i++) sb_puts(b, ch);
}

size_t tui_fit(const char *s, size_t n, int w) {
    if (!s || w <= 0) return 0;
    size_t pos = 0;
    int used = 0;
    while (pos < n) {
        size_t start = pos;
        uint32_t cp = utf8_next(s, n, &pos);
        int cw = wcwidth_u(cp);
        if (used + cw > w) {
            pos = start;
            break;
        }
        used += cw;
    }
    return pos;
}

static void sb_ellipsis(SB *b, const char *s, int w) {
    if (!s) return;
    size_t n = strlen(s);
    if (display_width(s, n) <= w) {
        sb_write(b, s, n);
        return;
    }
    if (w <= 1) {
        sb_puts(b, "\xe2\x80\xa6"); /* … */
        return;
    }
    size_t take = tui_fit(s, n, w - 1);
    sb_write(b, s, take);
    sb_puts(b, "\xe2\x80\xa6");
}

static void sb_ellipsis_pad(SB *b, const char *s, int w) {
    int used = 0;
    if (s) {
        size_t n = strlen(s);
        size_t take = tui_fit(s, n, w);
        bool cut = take < n;
        sb_write(b, s, take);
        if (cut) {
            sb_puts(b, "\xe2\x80\xa6");
            used = display_width(s, take) + 1;
        } else {
            used = display_width(s, n);
        }
    }
    sb_spaces(b, w - used);
}

/* ------------------------------------------------------------------ */
/* banner / box                                                        */
/* ------------------------------------------------------------------ */
void tui_banner(SB *sb, const char *title, const char *subtitle) {
    const char *t = title ? title : "";
    const char *sub = subtitle ? subtitle : "";
    int tw = dwidth(t);
    int sw = dwidth(sub);
    int maxw = tty_columns() - 1;
    if (maxw < 20) maxw = 20;

    int inner = tw + 6;
    if (sw + 4 > inner) inner = sw + 4;
    if (inner + 2 > maxw) inner = maxw - 2;

    int rule = inner - tw - 4;
    if (rule < 1) rule = 1;

    sb_puts(sb, sty_accent());
    sb_puts(sb, "\xe2\x95\xad\xe2\x94\x80 "); /* ╭─ */
    sb_puts(sb, sty_reset());
    sb_puts(sb, sty_bold());
    sb_puts(sb, sty_accent());
    sb_puts(sb, t);
    sb_puts(sb, sty_reset());
    sb_puts(sb, sty_accent());
    sb_puts(sb, " ");
    sb_rule(sb, "\xe2\x94\x80", rule);
    sb_puts(sb, "\xe2\x95\xae\n"); /* ╮ */
    sb_puts(sb, sty_reset());

    sb_puts(sb, sty_accent());
    sb_puts(sb, "\xe2\x94\x82"); /* │ */
    sb_puts(sb, sty_reset());
    sb_puts(sb, "  ");
    sb_puts(sb, sty_dim());
    sb_ellipsis(sb, sub, inner - 2);
    sb_puts(sb, sty_reset());
    int fill = inner - 2 - (sw <= inner - 2 ? sw : inner - 2);
    sb_spaces(sb, fill);
    sb_puts(sb, sty_accent());
    sb_puts(sb, "\xe2\x94\x82\n"); /* │ */
    sb_puts(sb, sty_reset());

    sb_puts(sb, sty_accent());
    sb_puts(sb, "\xe2\x95\xb0"); /* ╰ */
    sb_rule(sb, "\xe2\x94\x80", inner);
    sb_puts(sb, "\xe2\x95\xaf"); /* ╯ */
    sb_puts(sb, sty_reset());
    sb_puts(sb, "\n");
}

/* ------------------------------------------------------------------ */
/* table                                                               */
/* ------------------------------------------------------------------ */
void tui_table(SB *sb, const TuiCol *cols, size_t ncol, const char *const *cells, size_t nrow) {
    if (!ncol) return;
    int maxw = tty_columns() - 1;
    if (maxw < 20) maxw = 20;

    int *w = (int *)calloc(ncol, sizeof(int));
    if (!w) return;
    for (size_t c = 0; c < ncol; c++) {
        w[c] = dwidth(cols[c].head);
        for (size_t r = 0; r < nrow; r++) {
            int cw = dwidth(cells[r * ncol + c]);
            if (cw > w[c]) w[c] = cw;
        }
        if (w[c] < 1) w[c] = 1;
    }

    /* Shrink the widest column until the table fits the terminal. */
    for (;;) {
        int total = 1 + (int)(ncol - 1) * 2;
        for (size_t c = 0; c < ncol; c++) total += w[c];
        if (total <= maxw) break;
        size_t widest = 0;
        for (size_t c = 1; c < ncol; c++) {
            if (w[c] > w[widest]) widest = c;
        }
        if (w[widest] <= 6) break;
        w[widest]--;
    }

    /* header */
    sb_puts(sb, " ");
    for (size_t c = 0; c < ncol; c++) {
        if (c) sb_puts(sb, "  ");
        int used = 0;
        sb_puts(sb, sty_bold());
        sb_puts(sb, sty_accent());
        const char *h = cols[c].head ? cols[c].head : "";
        int hw = dwidth(h);
        if (hw > w[c]) {
            size_t take = tui_fit(h, strlen(h), w[c]);
            sb_write(sb, h, take);
            used = display_width(h, take);
        } else {
            sb_puts(sb, h);
            used = hw;
        }
        sb_puts(sb, sty_reset());
        sb_spaces(sb, w[c] - used);
    }
    sb_puts(sb, "\n");

    /* rule */
    sb_puts(sb, sty_dim());
    sb_puts(sb, " ");
    for (size_t c = 0; c < ncol; c++) {
        if (c) sb_puts(sb, "  ");
        sb_rule(sb, "\xe2\x94\x80", w[c]);
    }
    sb_puts(sb, sty_reset());
    sb_puts(sb, "\n");

    /* rows */
    for (size_t r = 0; r < nrow; r++) {
        sb_puts(sb, " ");
        for (size_t c = 0; c < ncol; c++) {
            if (c) sb_puts(sb, "  ");
            const char *v = cells[r * ncol + c] ? cells[r * ncol + c] : "";
            size_t vn = strlen(v);
            bool cut = display_width(v, vn) > w[c];
            size_t take = cut ? tui_fit(v, vn, w[c] - 1) : vn;
            int used = display_width(v, take);
            int extra = w[c] - used - (cut ? 1 : 0);
            if (cols[c].right) sb_spaces(sb, extra);
            sb_write(sb, v, take);
            if (cut) sb_puts(sb, "\xe2\x80\xa6");
            if (!cols[c].right) sb_spaces(sb, extra);
        }
        sb_puts(sb, "\n");
    }

    free(w);
}

/* ------------------------------------------------------------------ */
/* fuzzy picker                                                        */
/* ------------------------------------------------------------------ */
typedef struct Sel {
    const TuiItem *items;
    size_t n;
    const char *title;
    const char *hint;
    char query[80];
    size_t qn;
    size_t cur;
    size_t off;
    size_t *idx;
    int *score;
    size_t hits;
} Sel;

static uint32_t cp_lower(uint32_t cp) {
    return (cp >= 'A' && cp <= 'Z') ? cp + 32 : cp;
}

/* 0..N for a subsequence match, -1 when the needle does not match. */
static int fuzzy_score(const char *label, const char *q) {
    if (!*q) return 0;
    size_t ln = strlen(label), qn = strlen(q);
    if (str_starts_ci(label, q)) {
        return 100000 - (int)qn;
    }
    size_t li = 0, qi = 0;
    int score = 0;
    int prev_end = -1;
    while (qi < qn) {
        uint32_t qc = cp_lower(utf8_next(q, qn, &qi));
        bool found = false;
        int at = 0;
        while (li < ln) {
            uint32_t lc = cp_lower(utf8_next(label, ln, &li));
            if (lc == qc) {
                found = true;
                break;
            }
            at++;
        }
        if (!found) return -1;
        score += 20;
        if (prev_end >= 0 && at == 0) score += 15; /* contiguous run */
        else score -= at > 4 ? 4 : at;
        prev_end = at == 0 ? (prev_end >= 0 ? prev_end + 1 : 0) : at;
    }
    return score;
}

static void sel_refilter(Sel *s) {
    size_t out = 0;
    for (size_t i = 0; i < s->n; i++) {
        int sc = fuzzy_score(s->items[i].label ? s->items[i].label : "", s->query);
        if (sc < 0) continue;
        s->idx[out] = i;
        s->score[out] = sc;
        out++;
    }
    /* insertion sort by score desc, stable on original order */
    for (size_t i = 1; i < out; i++) {
        size_t j = i;
        while (j > 0 && s->score[j - 1] < s->score[j]) {
            int ts = s->score[j - 1];
            s->score[j - 1] = s->score[j];
            s->score[j] = ts;
            size_t ti = s->idx[j - 1];
            s->idx[j - 1] = s->idx[j];
            s->idx[j] = ti;
            j--;
        }
    }
    s->hits = out;
    if (s->cur >= out) s->cur = out ? out - 1 : 0;
    if (s->off > s->cur) s->off = s->cur;
}

static int visible_rows(void) {
    int rows = tty_rows() - 7;
    if (rows < 3) rows = 3;
    if (rows > 18) rows = 18;
    return rows;
}

/* End of line: erase the tail so a shrinking line leaves no residue.
   Using per-line erase (instead of a full-screen clear) keeps the redraw
   flicker-free on both conhost and Windows Terminal. */
static void sel_eol(SB *b) { sb_puts(b, "\x1b[K\n"); }

static void sel_draw(const Sel *s) {
    int maxw = tty_columns() - 1;
    if (maxw < 20) maxw = 20;
    int vis = visible_rows();
    SB b;
    sb_init(&b);

    sb_puts(&b, "\x1b[H");
    sb_puts(&b, sty_accent());
    sb_puts(&b, sty_bold());
    sb_ellipsis(&b, s->title, maxw);
    sb_puts(&b, sty_reset());
    sel_eol(&b);

    if (s->hint) {
        sb_puts(&b, sty_dim());
        sb_ellipsis(&b, s->hint, maxw);
        sb_puts(&b, sty_reset());
        sel_eol(&b);
    }

    sb_puts(&b, sty_dim());
    sb_puts(&b, "\xe7\xad\x9b\xe9\x80\x89: "); /* 筛选: */
    sb_puts(&b, sty_reset());
    sb_puts(&b, sty_bold());
    int qspace = maxw - 8;
    if (qspace < 4) qspace = 4;
    if ((size_t)dwidth(s->query) > (size_t)qspace) {
        size_t skip = tui_fit(s->query, s->qn, (int)(dwidth(s->query) - qspace));
        sb_puts(&b, s->query + skip);
    } else {
        sb_puts(&b, s->query);
    }
    sb_puts(&b, "\xe2\x96\x8f"); /* ▏ */
    sb_puts(&b, sty_reset());
    sel_eol(&b);

    sb_puts(&b, sty_dim());
    sb_rule(&b, "\xe2\x94\x80", maxw);
    sb_puts(&b, sty_reset());
    sel_eol(&b);

    if (!s->hits) {
        sb_puts(&b, "  ");
        sb_puts(&b, sty_dim());
        sb_puts(&b, "\xe6\x97\xa0\xe5\x8c\xb9\xe9\x85\x8d\xe9\xa1\xb9\xe7\x9b\xae"); /* 无匹配项目 */
        sb_puts(&b, sty_reset());
        sel_eol(&b);
        for (int i = 1; i < vis; i++) sel_eol(&b);
    } else {
        for (int row = 0; row < vis; row++) {
            size_t k = s->off + (size_t)row;
            if (k >= s->hits) {
                sel_eol(&b);
                continue;
            }
            const TuiItem *it = &s->items[s->idx[k]];
            bool on = k == s->cur;
            if (on) {
                sb_puts(&b, sty_sel());
                sb_puts(&b, sty_bold());
            }
            sb_puts(&b, on ? " \xe2\x96\xb8 " : "   "); /* ▸ */
            int room = maxw - 4;
            const char *label = it->label ? it->label : "";
            const char *detail = it->detail ? it->detail : "";
            int lw = dwidth(label);
            int dw = dwidth(detail);
            if (dw > 0 && lw + dw + 2 <= room) {
                sb_puts(&b, label);
                sb_spaces(&b, room - lw - dw);
                sb_puts(&b, sty_reset());
                if (on) sb_puts(&b, sty_sel());
                sb_puts(&b, sty_dim());
                sb_puts(&b, detail);
            } else {
                sb_ellipsis_pad(&b, label, room);
            }
            sb_puts(&b, sty_reset());
            if (on) {
                /* keep the highlight running to the right edge */
                sb_puts(&b, sty_sel());
                sb_puts(&b, " ");
            }
            sb_puts(&b, sty_reset());
            sel_eol(&b);
        }
    }

    sb_puts(&b, sty_dim());
    sb_rule(&b, "\xe2\x94\x80", maxw);
    sb_puts(&b, sty_reset());
    sel_eol(&b);

    sb_puts(&b, " ");
    sb_puts(&b, sty_dim());
    sb_puts(&b, "\xe2\x86\x91\xe2\x86\x93 \xe7\xa7\xbb\xe5\x8a\xa8   Enter \xe9\x80\x89\xe6\x8b\xa9   Esc \xe5\x8f\x96\xe6\xb6\x88");
    sb_puts(&b, sty_reset());
    if (s->hits) {
        char count[64];
        snprintf(count, sizeof(count), "   %zu/%zu", s->cur + 1, s->hits);
        int used = dwidth(" \xe2\x86\x91\xe2\x86\x93 \xe7\xa7\xbb\xe5\x8a\xa8   Enter \xe9\x80\x89\xe6\x8b\xa9   Esc \xe5\x8f\x96\xe6\xb6\x88") + dwidth(count);
        sb_spaces(&b, maxw - used);
        sb_puts(&b, sty_accent());
        sb_puts(&b, count);
        sb_puts(&b, sty_reset());
    }
    sel_eol(&b);
    sb_puts(&b, "\x1b[0J"); /* clear anything left by a taller previous frame */

    tty_write(b.p, b.n);
    sb_free(&b);
}

int tui_select(const char *title, const char *hint, const TuiItem *items, size_t n) {
    if (!n) return -1;
    Sel s;
    memset(&s, 0, sizeof(s));
    s.items = items;
    s.n = n;
    s.title = title;
    s.hint = hint;
    s.idx = (size_t *)malloc(n * sizeof(size_t));
    s.score = (int *)malloc(n * sizeof(int));
    if (!s.idx || !s.score) {
        free(s.idx);
        free(s.score);
        return -1;
    }
    sel_refilter(&s);

    tty_enter_raw();
    tty_write("\x1b[?1049h\x1b[?25l", 14);

    int result = -1;
    for (;;) {
        int vis = visible_rows();
        if (s.cur < s.off) s.off = s.cur;
        if (s.cur >= s.off + (size_t)vis) s.off = s.cur - (size_t)vis + 1;
        sel_draw(&s);

        Key k = tty_read_key();
        if (k.kind == KEY_CTRL_C || k.kind == KEY_ESC || k.kind == KEY_EOF) break;
        if (k.kind == KEY_ENTER) {
            if (s.hits) result = (int)s.idx[s.cur];
            break;
        }
        if (k.kind == KEY_UP) {
            if (s.cur > 0) s.cur--;
        } else if (k.kind == KEY_DOWN) {
            if (s.cur + 1 < s.hits) s.cur++;
        } else if (k.kind == KEY_PGUP) {
            s.cur = s.cur > (size_t)vis ? s.cur - (size_t)vis : 0;
        } else if (k.kind == KEY_PGDN) {
            if (s.hits) {
                s.cur += (size_t)vis;
                if (s.cur >= s.hits) s.cur = s.hits - 1;
            }
        } else if (k.kind == KEY_HOME) {
            s.cur = 0;
        } else if (k.kind == KEY_END) {
            s.cur = s.hits ? s.hits - 1 : 0;
        } else if (k.kind == KEY_BACKSPACE) {
            if (s.qn) {
                size_t p = s.qn - 1;
                while (p > 0 && ((unsigned char)s.query[p] & 0xC0) == 0x80) p--;
                s.qn = p;
                s.query[s.qn] = '\0';
                s.cur = 0;
                s.off = 0;
                sel_refilter(&s);
            }
        } else if (k.kind == KEY_CHAR) {
            size_t add = strlen(k.text);
            if (s.qn + add < sizeof(s.query) - 1) {
                memcpy(s.query + s.qn, k.text, add);
                s.qn += add;
                s.query[s.qn] = '\0';
                s.cur = 0;
                s.off = 0;
                sel_refilter(&s);
            }
        }
    }

    tty_write("\x1b[?25h\x1b[?1049l", 14);
    tty_leave_raw();
    free(s.idx);
    free(s.score);
    return result;
}
