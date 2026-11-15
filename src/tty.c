/* tty.c - Win32 console control (UTF-8, VT styling, raw key input). */
#include "tty.h"

#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "utf8.h"

static bool g_color = false;
static bool g_stdout_tty = false;
static bool g_stdin_tty = false;
static bool g_raw = false;
static bool g_vt = false;
static DWORD g_saved_in = 0;
static DWORD g_saved_out = 0;
static HANDLE g_in = INVALID_HANDLE_VALUE;
static HANDLE g_out = INVALID_HANDLE_VALUE;
static uint32_t g_pending_high = 0;

/* ------------------------------------------------------------------ */
/* init / shutdown                                                     */
/* ------------------------------------------------------------------ */
static bool env_truthy(const char *name) {
    const char *v = getenv(name);
    return v && *v && !(v[0] == '0' && v[1] == '\0');
}

void tty_init(bool no_color) {
    g_in = GetStdHandle(STD_INPUT_HANDLE);
    g_out = GetStdHandle(STD_OUTPUT_HANDLE);
    g_stdout_tty = _isatty(_fileno(stdout)) != 0;
    g_stdin_tty = _isatty(_fileno(stdin)) != 0;

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    if (g_out != INVALID_HANDLE_VALUE && GetConsoleMode(g_out, &g_saved_out)) {
        DWORD want = g_saved_out | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        g_vt = SetConsoleMode(g_out, want) != 0;
    }
    if (g_in != INVALID_HANDLE_VALUE) GetConsoleMode(g_in, &g_saved_in);

    /* Colour policy mirrors the old chalk-based CLI: FORCE_COLOR wins, then NO_COLOR. */
    if (no_color || env_truthy("NO_COLOR")) {
        g_color = false;
    } else if (getenv("FORCE_COLOR")) {
        const char *fc = getenv("FORCE_COLOR");
        g_color = !(fc[0] == '0' && fc[1] == '\0');
    } else {
        g_color = g_stdout_tty && g_vt;
    }
    if (g_color && !g_vt) g_color = false;
}

void tty_shutdown(void) {
    if (g_raw) tty_leave_raw();
    if (g_out != INVALID_HANDLE_VALUE && g_saved_out) SetConsoleMode(g_out, g_saved_out);
}

bool tty_color_enabled(void) { return g_color; }
bool tty_stdout_is_tty(void) { return g_stdout_tty; }
bool tty_stdin_is_tty(void) { return g_stdin_tty; }

bool tty_supports_ui(void) {
    return g_stdout_tty && g_stdin_tty && g_vt && g_in != INVALID_HANDLE_VALUE && g_out != INVALID_HANDLE_VALUE;
}

int tty_columns(void) {
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (g_out != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(g_out, &info)) {
        int w = (int)(info.srWindow.Right - info.srWindow.Left + 1);
        if (w > 0) return w;
    }
    return 80;
}

int tty_rows(void) {
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (g_out != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(g_out, &info)) {
        int h = (int)(info.srWindow.Bottom - info.srWindow.Top + 1);
        if (h > 0) return h;
    }
    return 25;
}

void tty_write(const void *data, size_t n) {
    if (!n) return;
    fwrite(data, 1, n, stdout);
    fflush(stdout);
}

/* ------------------------------------------------------------------ */
/* raw mode                                                            */
/* ------------------------------------------------------------------ */
void tty_enter_raw(void) {
    if (g_raw || g_in == INVALID_HANDLE_VALUE) return;
    DWORD mode;
    if (!GetConsoleMode(g_in, &mode)) return;
    g_saved_in = mode;
    DWORD want = mode;
    want &= ~(DWORD)(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_MOUSE_INPUT);
    want |= ENABLE_EXTENDED_FLAGS;
    want &= ~(DWORD)ENABLE_QUICK_EDIT_MODE;
    if (SetConsoleMode(g_in, want)) {
        g_raw = true;
        g_pending_high = 0;
        FlushConsoleInputBuffer(g_in);
    }
}

void tty_leave_raw(void) {
    if (!g_raw) return;
    SetConsoleMode(g_in, g_saved_in);
    g_raw = false;
    g_pending_high = 0;
}

/* ------------------------------------------------------------------ */
/* key input                                                           */
/* ------------------------------------------------------------------ */
static Key key_of(KeyKind k) {
    Key out;
    memset(&out, 0, sizeof(out));
    out.kind = k;
    return out;
}

static bool ctrl_down(DWORD state) {
    return (state & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
}

Key tty_read_key(void) {
    if (g_in == INVALID_HANDLE_VALUE) return key_of(KEY_EOF);

    for (;;) {
        INPUT_RECORD rec;
        DWORD got = 0;
        if (!ReadConsoleInputW(g_in, &rec, 1, &got) || got == 0) return key_of(KEY_EOF);

        if (rec.EventType == WINDOW_BUFFER_SIZE_EVENT) return key_of(KEY_RESIZE);
        if (rec.EventType != KEY_EVENT) continue;

        const KEY_EVENT_RECORD *k = &rec.Event.KeyEvent;
        if (!k->bKeyDown) continue;

        DWORD vk = k->wVirtualKeyCode;
        if (ctrl_down(k->dwControlKeyState) && vk == 'C') return key_of(KEY_CTRL_C);

        switch (vk) {
            case VK_UP: return key_of(KEY_UP);
            case VK_DOWN: return key_of(KEY_DOWN);
            case VK_LEFT: return key_of(KEY_LEFT);
            case VK_RIGHT: return key_of(KEY_RIGHT);
            case VK_HOME: return key_of(KEY_HOME);
            case VK_END: return key_of(KEY_END);
            case VK_PRIOR: return key_of(KEY_PGUP);
            case VK_NEXT: return key_of(KEY_PGDN);
            case VK_DELETE: return key_of(KEY_DELETE);
            case VK_BACK: return key_of(KEY_BACKSPACE);
            case VK_RETURN: return key_of(KEY_ENTER);
            case VK_ESCAPE: return key_of(KEY_ESC);
            case VK_TAB: return key_of(KEY_CHAR); /* consumed as a literal, callers ignore */
            default: break;
        }

        WCHAR wc = k->uChar.UnicodeChar;
        if (wc == 0) continue;
        if (wc == 0x08 || wc == 0x7F) return key_of(KEY_BACKSPACE);
        if (wc == 0x0D) return key_of(KEY_ENTER);
        if (wc == 0x1B) return key_of(KEY_ESC);
        if (wc == 0x03) return key_of(KEY_CTRL_C);
        if (wc < 0x20) continue;

        uint32_t cp;
        if (wc >= 0xD800 && wc <= 0xDBFF) {
            g_pending_high = wc;
            continue;
        }
        if (wc >= 0xDC00 && wc <= 0xDFFF) {
            if (!g_pending_high) continue;
            cp = 0x10000u + ((g_pending_high - 0xD800u) << 10) + (wc - 0xDC00u);
            g_pending_high = 0;
        } else {
            cp = wc;
            g_pending_high = 0;
        }

        Key out;
        memset(&out, 0, sizeof(out));
        out.kind = KEY_CHAR;
        size_t n = utf8_encode((uint32_t)cp, out.text);
        out.text[n] = '\0';
        return out;
    }
}

/* ------------------------------------------------------------------ */
/* styles                                                              */
/* ------------------------------------------------------------------ */
#define SGR(s) (g_color ? (s) : "")
const char *sty_reset(void) { return g_color ? "\x1b[0m" : ""; }
const char *sty_bold(void) { return SGR("\x1b[1m"); }
const char *sty_dim(void) { return SGR("\x1b[2m"); }
const char *sty_accent(void) { return SGR("\x1b[36m"); }
const char *sty_ok(void) { return SGR("\x1b[32m"); }
const char *sty_warn(void) { return SGR("\x1b[33m"); }
const char *sty_err(void) { return SGR("\x1b[31m"); }
const char *sty_sel(void) { return SGR("\x1b[7m"); }
