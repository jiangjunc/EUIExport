/* main.c - process entry point.
 *
 * The CRT's argv is decoded with the ANSI code page, which garbles non-ASCII
 * draft names (e.g. 中文地图名). Rebuild argv from the wide command line and
 * convert it to UTF-8 so the rest of the program only deals with UTF-8 bytes.
 */
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <shellapi.h>

#include "cmd.h"
#include "selfreg.h"

static char *w2u(const wchar_t *w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    if (n <= 0) return NULL;
    char *out = (char *)malloc((size_t)n);
    if (!out) return NULL;
    WideCharToMultiByte(CP_UTF8, 0, w, -1, out, n, NULL, NULL);
    return out;
}

int main(void) {
    /* 双击启动时独占一个新控制台窗口：标题友好一些，结束时等按键。 */
    bool owned = selfreg_owns_console();
    if (owned) SetConsoleTitleW(L"蛋仔 EUI 导出器");

    /* 每次运行都确保自身目录在用户 PATH 中（幂等，无变化时不写注册表）。 */
    selfreg_ensure_path();

    int argc = 0;
    LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!wargv || argc <= 0) return 1;

    char **argv = (char **)calloc((size_t)argc + 1, sizeof(char *));
    if (!argv) {
        LocalFree(wargv);
        return 1;
    }
    for (int i = 0; i < argc; i++) {
        argv[i] = w2u(wargv[i]);
        if (!argv[i]) argv[i] = (char *)"";
    }

    int code = cmd_main(argc, argv);

    for (int i = 0; i < argc; i++) free(argv[i]);
    free(argv);
    LocalFree(wargv);

    if (owned) selfreg_wait_key();
    return code;
}
