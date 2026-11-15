/* selfreg.c - 自注册用户 PATH + 双击运行体验。
 *
 * 设计要点：
 *  - 只读写 HKCU\Environment，绝不碰系统级 PATH，也不需要管理员权限。
 *  - 写入前先读 marker（EUIEXPORT_HOME）判断是否已注册；已注册则零写入。
 *  - PATH 里的其它条目原样保留（含 %USERPROFILE% 这类可展开变量），只增删自己那一项。
 *  - 任何失败都静默返回，导出的主流程不受影响。
 */
#define _CRT_SECURE_NO_WARNINGS
#include "selfreg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define ENV_KEY L"Environment"
#define PATH_NAME L"Path"
#define MARKER_NAME L"EUIEXPORT_HOME"
#define MAX_ENV_CHARS 32000

/* ------------------------------------------------------------------ */
/* 小工具                                                              */
/* ------------------------------------------------------------------ */
static wchar_t lower_w(wchar_t c) {
    return (c >= L'A' && c <= L'Z') ? (wchar_t)(c - L'A' + L'a') : c;
}

/* 比较 PATH 条目：忽略大小写、首尾空格与末尾分隔符。 */
static bool entry_eq(const wchar_t *s, size_t len, const wchar_t *ref) {
    while (len > 0 && (*s == L' ' || *s == L'\t')) {
        s++;
        len--;
    }
    while (len > 0 && (s[len - 1] == L' ' || s[len - 1] == L'\t' || s[len - 1] == L'\\' ||
                       s[len - 1] == L'/')) {
        len--;
    }
    size_t rl = wcslen(ref);
    while (rl > 0 && (ref[rl - 1] == L'\\' || ref[rl - 1] == L'/')) rl--;
    if (len != rl) return false;
    for (size_t i = 0; i < len; i++) {
        if (lower_w(s[i]) != lower_w(ref[i])) return false;
    }
    return true;
}

static bool path_has(const wchar_t *path, const wchar_t *dir) {
    if (!path) return false;
    const wchar_t *s = path;
    while (*s) {
        const wchar_t *e = s;
        while (*e && *e != L';') e++;
        if (entry_eq(s, (size_t)(e - s), dir)) return true;
        s = (*e == L';') ? e + 1 : e;
    }
    return false;
}

static wchar_t *reg_read(HKEY key, const wchar_t *name, DWORD *type) {
    DWORD bytes = 0, t = 0;
    if (RegQueryValueExW(key, name, NULL, &t, NULL, &bytes) != ERROR_SUCCESS) return NULL;
    if (t != REG_SZ && t != REG_EXPAND_SZ) return NULL;
    wchar_t *buf = (wchar_t *)malloc((size_t)bytes + sizeof(wchar_t));
    if (!buf) return NULL;
    DWORD got = bytes;
    if (RegQueryValueExW(key, name, NULL, &t, (LPBYTE)buf, &got) != ERROR_SUCCESS) {
        free(buf);
        return NULL;
    }
    buf[got / sizeof(wchar_t)] = L'\0';
    if (type) *type = t;
    return buf;
}

static void broadcast_env_change(void) {
    DWORD_PTR dummy = 0;
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"Environment",
                        SMTO_ABORTIFHUNG | SMTO_NORMAL, 2000, &dummy);
}

/* ------------------------------------------------------------------ */
/* 自注册                                                              */
/* ------------------------------------------------------------------ */
bool selfreg_ensure_path(void) {
    const char *opt_out = getenv("EUI_NO_PATH");
    if (opt_out && *opt_out && strcmp(opt_out, "0") != 0) return false;

    wchar_t exe[32768];
    DWORD n = GetModuleFileNameW(NULL, exe, 32768);
    if (n == 0 || n >= 32768) return false;

    wchar_t *slash = NULL;
    for (wchar_t *p = exe; *p; p++) {
        if (*p == L'\\' || *p == L'/') slash = p;
    }
    if (!slash) return false;
    *slash = L'\0';
    if (!exe[0]) return false;
    /* "C:" → "C:\" */
    if (wcslen(exe) == 2 && exe[1] == L':') {
        exe[2] = L'\\';
        exe[3] = L'\0';
    }
    const wchar_t *dir = exe;

    HKEY key = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, ENV_KEY, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &key) !=
        ERROR_SUCCESS) {
        return false;
    }

    DWORD path_type = 0;
    wchar_t *old_path = reg_read(key, PATH_NAME, &path_type);
    wchar_t *marker = reg_read(key, MARKER_NAME, NULL);
    bool marker_ok = marker && entry_eq(marker, wcslen(marker), dir);

    if (marker_ok && path_has(old_path, dir)) {
        free(old_path);
        free(marker);
        RegCloseKey(key);
        return false;
    }

    size_t dir_len = wcslen(dir);
    size_t cap = (old_path ? wcslen(old_path) : 0) + dir_len + 4;
    if (cap > MAX_ENV_CHARS) {
        free(old_path);
        free(marker);
        RegCloseKey(key);
        return false;
    }
    wchar_t *out = (wchar_t *)malloc(cap * sizeof(wchar_t));
    if (!out) {
        free(old_path);
        free(marker);
        RegCloseKey(key);
        return false;
    }

    size_t olen = 0;
    bool present = false;
    if (old_path) {
        const wchar_t *s = old_path;
        while (*s) {
            const wchar_t *e = s;
            while (*e && *e != L';') e++;
            size_t len = (size_t)(e - s);
            if (entry_eq(s, len, dir)) {
                present = true;
            } else if (!(marker && entry_eq(s, len, marker))) {
                /* 丢弃旧注册目录，其余条目原样保留 */
                if (olen) out[olen++] = L';';
                memcpy(out + olen, s, len * sizeof(wchar_t));
                olen += len;
            }
            s = (*e == L';') ? e + 1 : e;
        }
    }
    if (!present) {
        if (olen) out[olen++] = L';';
        memcpy(out + olen, dir, dir_len * sizeof(wchar_t));
        olen += dir_len;
    }
    out[olen] = L'\0';

    bool changed = false;
    if (!old_path || wcscmp(old_path, out) != 0) {
        DWORD type = (path_type == REG_EXPAND_SZ) ? REG_EXPAND_SZ : REG_SZ;
        DWORD bytes = (DWORD)((olen + 1) * sizeof(wchar_t));
        if (RegSetValueExW(key, PATH_NAME, 0, type, (const BYTE *)out, bytes) == ERROR_SUCCESS) {
            changed = true;
        }
    }
    if (!marker_ok) {
        DWORD bytes = (DWORD)((dir_len + 1) * sizeof(wchar_t));
        if (RegSetValueExW(key, MARKER_NAME, 0, REG_SZ, (const BYTE *)dir, bytes) == ERROR_SUCCESS) {
            changed = true;
        }
    }

    free(old_path);
    free(marker);
    free(out);
    RegCloseKey(key);

    if (changed) broadcast_env_change();
    return changed;
}

/* ------------------------------------------------------------------ */
/* 双击运行体验                                                        */
/* ------------------------------------------------------------------ */
bool selfreg_owns_console(void) {
    DWORD pids[4];
    DWORD n = GetConsoleProcessList(pids, 4);
    return n == 1;
}

void selfreg_wait_key(void) {
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    if (in == INVALID_HANDLE_VALUE || in == NULL || !GetConsoleMode(in, &mode)) return;

    printf("\n按任意键退出…");
    fflush(stdout);

    INPUT_RECORD rec;
    DWORD got = 0;
    for (;;) {
        if (!ReadConsoleInputW(in, &rec, 1, &got) || got == 0) break;
        if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown) break;
    }
}
