/* tui.h - table/box rendering and the interactive draft picker. */
#ifndef EUI_TUI_H
#define EUI_TUI_H

#include "util.h"

typedef struct TuiCol {
    const char *head;
    bool right;
} TuiCol;

typedef struct TuiItem {
    const char *label;  /* primary text, plain UTF-8 */
    const char *detail; /* dim suffix, may be NULL */
} TuiItem;

/* Rounded banner box: title + one subtitle line. */
void tui_banner(SB *sb, const char *title, const char *subtitle);
/* Light table: single header rule, CJK-aware column widths. */
void tui_table(SB *sb, const TuiCol *cols, size_t ncol, const char *const *cells, size_t nrow);

/* Longest prefix of s that fits in w display columns. */
size_t tui_fit(const char *s, size_t n, int w);

/* Full-screen fuzzy picker. Returns the chosen index, or -1 when cancelled. */
int tui_select(const char *title, const char *hint, const TuiItem *items, size_t n);

#endif /* EUI_TUI_H */
