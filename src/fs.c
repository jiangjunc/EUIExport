#define _CRT_SECURE_NO_WARNINGS
#include "fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#error "EUIExport targets Windows (single-file exe)."
#endif

#include <windows.h>
#include <shlobj.h>

/* ------------------------------------------------------------------ */
/* UTF-8 <-> UTF-16                                                    */
/* ------------------------------------------------------------------ */
static wchar_t *widen(Arena *a, const char *s) {
    if (!s) return NULL;
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (n <= 0) return NULL;
    wchar_t *w = (wchar_t *)arena_alloc(a, (size_t)n * sizeof(wchar_t));
    if (!w) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}

static char *narrow(Arena *a, const wchar_t *w, int wlen) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w, wlen, NULL, 0, NULL, NULL);
    if (n <= 0) return arena_strdup(a, "");
    char *s = (char *)arena_alloc(a, (size_t)n + 1);
    if (!s) return NULL;
    WideCharToMultiByte(CP_UTF8, 0, w, wlen, s, n, NULL, NULL);
    s[n] = '\0';
    return s;
}

/* ------------------------------------------------------------------ */
/* path utilities                                                      */
/* ------------------------------------------------------------------ */
bool path_is_abs(const char *p) {
    if (!p || !*p) return false;
    if (p[0] == '\\' || p[0] == '/') return true;
    if (((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) && p[1] == ':') return true;
    return false;
}

char *path_norm(Arena *a, const char *p) {
    if (!p || !*p) return arena_strdup(a, "");
    const char *s = p;
    bool unc = false;
    char drive[3] = {0, 0, 0};
    bool rooted = false;

    if ((s[0] == '\\' || s[0] == '/') && (s[1] == '\\' || s[1] == '/')) {
        unc = true;
        s += 2;
    } else if (((s[0] >= 'A' && s[0] <= 'Z') || (s[0] >= 'a' && s[0] <= 'z')) && s[1] == ':') {
        drive[0] = (char)((s[0] >= 'a') ? s[0] - 32 : s[0]);
        drive[1] = ':';
        s += 2;
        if (*s == '\\' || *s == '/') {
            rooted = true;
            s++;
        }
    } else if (*s == '\\' || *s == '/') {
        rooted = true;
        s++;
    }

    /* component stack of offsets into a scratch copy */
    char *scratch = arena_strdup(a, s);
    size_t cap = strlen(scratch) + 1;
    size_t *offs = (size_t *)arena_alloc(a, sizeof(size_t) * (cap + 1));
    size_t *lens = (size_t *)arena_alloc(a, sizeof(size_t) * (cap + 1));
    size_t ncomp = 0;

    size_t i = 0;
    while (scratch[i]) {
        while (scratch[i] == '\\' || scratch[i] == '/') i++;
        size_t start = i;
        while (scratch[i] && scratch[i] != '\\' && scratch[i] != '/') i++;
        size_t len = i - start;
        if (len == 0) break;
        if (len == 1 && scratch[start] == '.') continue;
        if (len == 2 && scratch[start] == '.' && scratch[start + 1] == '.') {
            if (ncomp > 0) ncomp--;
            continue;
        }
        offs[ncomp] = start;
        lens[ncomp] = len;
        ncomp++;
    }

    SB sb;
    sb_init(&sb);
    if (unc) sb_puts(&sb, "\\\\");
    if (drive[0]) {
        sb_puts(&sb, drive);
        if (rooted) sb_putc(&sb, '\\');
    } else if (rooted) {
        sb_putc(&sb, '\\');
    }
    for (size_t k = 0; k < ncomp; k++) {
        if (k) sb_putc(&sb, '\\');
        sb_write(&sb, scratch + offs[k], lens[k]);
    }
    if (sb.n == 0) sb_putc(&sb, '.');
    char *out = arena_strdup(a, sb_cstr(&sb));
    sb_free(&sb);
    return out;
}

char *path_join(Arena *a, const char *base, const char *part) {
    if (!base || !*base) return path_norm(a, part ? part : "");
    if (!part || !*part) return path_norm(a, base);
    if (path_is_abs(part)) return path_norm(a, part);
    SB sb;
    sb_init(&sb);
    sb_puts(&sb, base);
    sb_putc(&sb, '\\');
    sb_puts(&sb, part);
    char *joined = arena_strdup(a, sb_cstr(&sb));
    sb_free(&sb);
    return path_norm(a, joined);
}

char *path_join3(Arena *a, const char *x, const char *y, const char *z) {
    char *t = path_join(a, x, y);
    return path_join(a, t, z);
}

char *path_resolve(Arena *a, const char *base, const char *p) {
    if (path_is_abs(p)) return path_norm(a, p);
    char *joined = path_join(a, base, p);
    if (!path_is_abs(joined)) return path_norm(a, joined);
    return joined;
}

const char *path_basename(const char *path) {
    static _Thread_local char buf[1024];
    if (!path) return "";
    size_t n = strlen(path);
    while (n > 0 && (path[n - 1] == '\\' || path[n - 1] == '/')) n--;
    size_t start = n;
    while (start > 0 && path[start - 1] != '\\' && path[start - 1] != '/') start--;
    size_t len = n - start;
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, path + start, len);
    buf[len] = '\0';
    return buf;
}

char *path_dirname(Arena *a, const char *path) {
    if (!path || !*path) return arena_strdup(a, ".");
    size_t n = strlen(path);
    while (n > 0 && (path[n - 1] == '\\' || path[n - 1] == '/')) n--;
    while (n > 0 && path[n - 1] != '\\' && path[n - 1] != '/') n--;
    while (n > 0 && (path[n - 1] == '\\' || path[n - 1] == '/')) n--;
    if (n == 0) return arena_strdup(a, path_is_abs(path) ? "\\" : ".");
    return arena_strndup(a, path, n);
}

bool path_ends_with_ci(const char *path, const char *suffix) {
    if (!path || !suffix) return false;
    size_t a = strlen(path), b = strlen(suffix);
    if (b > a) return false;
    const char *p = path + (a - b);
    for (size_t i = 0; i < b; i++) {
        char x = p[i], y = suffix[i];
        if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
        if (x != y) return false;
    }
    return true;
}

/* ------------------------------------------------------------------ */
/* queries                                                             */
/* ------------------------------------------------------------------ */
static bool stat_w(const wchar_t *w, WIN32_FILE_ATTRIBUTE_DATA *out) {
    return GetFileAttributesExW(w, GetFileExInfoStandard, out) != 0;
}

bool fs_exists(const char *path) {
    Arena a;
    arena_init(&a);
    wchar_t *w = widen(&a, path);
    WIN32_FILE_ATTRIBUTE_DATA d;
    bool ok = w && stat_w(w, &d);
    arena_free(&a);
    return ok;
}

bool fs_is_dir(const char *path) {
    Arena a;
    arena_init(&a);
    wchar_t *w = widen(&a, path);
    WIN32_FILE_ATTRIBUTE_DATA d;
    bool ok = w && stat_w(w, &d) && (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    arena_free(&a);
    return ok;
}

bool fs_is_file(const char *path) {
    Arena a;
    arena_init(&a);
    wchar_t *w = widen(&a, path);
    WIN32_FILE_ATTRIBUTE_DATA d;
    bool ok = w && stat_w(w, &d) && !(d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    arena_free(&a);
    return ok;
}

int64_t fs_mtime_ms(const char *path) {
    Arena a;
    arena_init(&a);
    wchar_t *w = widen(&a, path);
    WIN32_FILE_ATTRIBUTE_DATA d;
    int64_t out = 0;
    if (w && stat_w(w, &d)) {
        ULARGE_INTEGER u;
        u.LowPart = d.ftLastWriteTime.dwLowDateTime;
        u.HighPart = d.ftLastWriteTime.dwHighDateTime;
        /* 100ns since 1601 -> ms since 1970 */
        out = (int64_t)(u.QuadPart / 10000ull) - 11644473600000ll;
    }
    arena_free(&a);
    return out;
}

char *fs_read_all(Arena *a, const char *path, size_t *out_len, Err *e) {
    Arena tmp;
    arena_init(&tmp);
    wchar_t *w = widen(&tmp, path);
    HANDLE h = w ? CreateFileW(w, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)
                 : INVALID_HANDLE_VALUE;
    if (h == INVALID_HANDLE_VALUE) {
        err_set(e, "无法读取文件：%s（%lu）", path, (unsigned long)GetLastError());
        arena_free(&tmp);
        return NULL;
    }
    LARGE_INTEGER size;
    if (!GetFileSizeEx(h, &size) || size.QuadPart < 0) {
        err_set(e, "无法获取文件大小：%s", path);
        CloseHandle(h);
        arena_free(&tmp);
        return NULL;
    }
    size_t n = (size_t)size.QuadPart;
    char *buf = (char *)arena_alloc(a, n + 1);
    if (!buf) {
        err_set(e, "内存不足：%s", path);
        CloseHandle(h);
        arena_free(&tmp);
        return NULL;
    }
    size_t got = 0;
    while (got < n) {
        DWORD chunk = (DWORD)((n - got) > 0x10000000u ? 0x10000000u : (n - got));
        DWORD read = 0;
        if (!ReadFile(h, buf + got, chunk, &read, NULL)) {
            err_set(e, "读取失败：%s（%lu）", path, (unsigned long)GetLastError());
            CloseHandle(h);
            arena_free(&tmp);
            return NULL;
        }
        if (read == 0) break;
        got += read;
    }
    buf[got] = '\0';
    CloseHandle(h);
    arena_free(&tmp);
    if (out_len) *out_len = got;
    return buf;
}

bool fs_write_all(const char *path, const void *data, size_t n, Err *e) {
    Arena tmp;
    arena_init(&tmp);
    wchar_t *w = widen(&tmp, path);
    HANDLE h = w ? CreateFileW(w, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)
                 : INVALID_HANDLE_VALUE;
    if (h == INVALID_HANDLE_VALUE) {
        err_set(e, "无法写入文件：%s（%lu）", path, (unsigned long)GetLastError());
        arena_free(&tmp);
        return false;
    }
    const char *p = (const char *)data;
    size_t off = 0;
    while (off < n) {
        DWORD chunk = (DWORD)((n - off) > 0x10000000u ? 0x10000000u : (n - off));
        DWORD wrote = 0;
        if (!WriteFile(h, p + off, chunk, &wrote, NULL) || wrote == 0) {
            err_set(e, "写入失败：%s（%lu）", path, (unsigned long)GetLastError());
            CloseHandle(h);
            arena_free(&tmp);
            return false;
        }
        off += wrote;
    }
    CloseHandle(h);
    arena_free(&tmp);
    return true;
}

bool fs_mkdir_p(const char *path, Err *e) {
    if (!path || !*path) return true;
    Arena a;
    arena_init(&a);
    char *p = path_norm(&a, path);
    size_t n = strlen(p);
    /* create each prefix */
    for (size_t i = 1; i <= n; i++) {
        if (i < n && p[i] != '\\' && p[i] != '/') continue;
        char saved = p[i];
        p[i] = '\0';
        bool skip = (i == 2 && p[1] == ':') || (i == 0);
        if (!skip) {
            wchar_t *w = widen(&a, p);
            if (w && !CreateDirectoryW(w, NULL)) {
                DWORD err = GetLastError();
                if (err != ERROR_ALREADY_EXISTS) {
                    err_set(e, "无法创建目录：%s（%lu）", p, (unsigned long)err);
                    p[i] = saved;
                    arena_free(&a);
                    return false;
                }
            }
        }
        p[i] = saved;
    }
    bool ok = fs_is_dir(path);
    if (!ok) err_set(e, "无法创建目录：%s", path);
    arena_free(&a);
    return ok;
}

bool fs_list_dir(Arena *a, const char *path, DirList *out, Err *e) {
    out->items = NULL;
    out->n = 0;
    Arena tmp;
    arena_init(&tmp);
    char *pattern = path_join(&tmp, path, "*");
    wchar_t *w = widen(&tmp, pattern);
    WIN32_FIND_DATAW fd;
    HANDLE h = w ? FindFirstFileW(w, &fd) : INVALID_HANDLE_VALUE;
    if (h == INVALID_HANDLE_VALUE) {
        err_set(e, "无法读取目录：%s", path);
        arena_free(&tmp);
        return false;
    }
    size_t cap = 16;
    DirEntry *items = (DirEntry *)arena_alloc(a, cap * sizeof(DirEntry));
    size_t n = 0;
    do {
        if (fd.cFileName[0] == '.') continue;
        if (n == cap) {
            items = (DirEntry *)arena_grow(a, items, cap * sizeof(DirEntry), cap * 2 * sizeof(DirEntry));
            cap *= 2;
        }
        items[n].name = narrow(a, fd.cFileName, -1);
        items[n].is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        n++;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    arena_free(&tmp);
    /* sort by name (byte order is identical to UTF-16 order for ASCII names) */
    for (size_t i = 1; i < n; i++) {
        DirEntry key = items[i];
        size_t j = i;
        while (j > 0 && strcmp(items[j - 1].name, key.name) > 0) {
            items[j] = items[j - 1];
            j--;
        }
        items[j] = key;
    }
    out->items = items;
    out->n = n;
    return true;
}

char *fs_cwd(Arena *a) {
    wchar_t buf[MAX_PATH * 4];
    DWORD n = GetCurrentDirectoryW((DWORD)(sizeof(buf) / sizeof(buf[0])), buf);
    if (n == 0 || n >= sizeof(buf) / sizeof(buf[0])) return arena_strdup(a, ".");
    return narrow(a, buf, (int)n);
}

char *fs_home(Arena *a) {
    wchar_t buf[MAX_PATH * 4];
    DWORD n = GetEnvironmentVariableW(L"USERPROFILE", buf, (DWORD)(sizeof(buf) / sizeof(buf[0])));
    if (n == 0) return arena_strdup(a, ".");
    return narrow(a, buf, (int)n);
}

char *fs_desktop(Arena *a) {
    wchar_t buf[MAX_PATH * 4];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY | CSIDL_FLAG_CREATE, NULL, 0, buf))) {
        int n = lstrlenW(buf);
        if (n > 0) return narrow(a, buf, n);
    }
    return path_join(a, fs_home(a), "Desktop");
}

char *fs_exe_path(Arena *a) {
    wchar_t buf[MAX_PATH * 4];
    DWORD n = GetModuleFileNameW(NULL, buf, (DWORD)(sizeof(buf) / sizeof(buf[0])));
    if (n == 0) return arena_strdup(a, "");
    return narrow(a, buf, (int)n);
}

char *env_get(Arena *a, const char *name) {
    Arena tmp;
    arena_init(&tmp);
    wchar_t *w = widen(&tmp, name);
    if (!w) {
        arena_free(&tmp);
        return NULL;
    }
    DWORD need = GetEnvironmentVariableW(w, NULL, 0);
    if (need == 0) {
        arena_free(&tmp);
        return NULL;
    }
    wchar_t *buf = (wchar_t *)arena_alloc(&tmp, (size_t)need * sizeof(wchar_t));
    if (!buf) {
        arena_free(&tmp);
        return NULL;
    }
    DWORD got = GetEnvironmentVariableW(w, buf, need);
    char *out = NULL;
    if (got > 0 && got < need) out = narrow(a, buf, (int)got);
    arena_free(&tmp);
    return out;
}
