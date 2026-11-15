/* tty.h - Windows console control: UTF-8 output, VT styling, key input. */
#ifndef EUI_TTY_H
#define EUI_TTY_H

#include "util.h"

typedef enum KeyKind {
    KEY_NONE = 0,
    KEY_CHAR,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_HOME,
    KEY_END,
    KEY_PGUP,
    KEY_PGDN,
    KEY_DELETE,
    KEY_BACKSPACE,
    KEY_ENTER,
    KEY_ESC,
    KEY_CTRL_C,
    KEY_RESIZE,
    KEY_EOF
} KeyKind;

typedef struct Key {
    KeyKind kind;
    char text[8]; /* UTF-8 bytes for KEY_CHAR */
} Key;

void tty_init(bool no_color);
void tty_shutdown(void);
bool tty_color_enabled(void);
bool tty_stdout_is_tty(void);
bool tty_stdin_is_tty(void);
int tty_columns(void);
int tty_rows(void);
void tty_enter_raw(void);
void tty_leave_raw(void);
Key tty_read_key(void);
/* False when the console cannot host an interactive full-screen selector. */
bool tty_supports_ui(void);

/* Writes UTF-8 bytes to stdout in one call. */
void tty_write(const void *data, size_t n);

/* SGR helpers return "" when colour is disabled. */
const char *sty_reset(void);
const char *sty_bold(void);
const char *sty_dim(void);
const char *sty_accent(void);
const char *sty_ok(void);
const char *sty_warn(void);
const char *sty_err(void);
const char *sty_sel(void); /* selection bar background */

#endif /* EUI_TTY_H */
