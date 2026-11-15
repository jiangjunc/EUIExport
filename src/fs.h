/* fs.h - UTF-8 path handling and Win32 filesystem access. */
#ifndef EUI_FS_H
#define EUI_FS_H

#include "util.h"

typedef struct DirEntry {
    char *name;   /* UTF-8 file name */
    bool is_dir;
} DirEntry;

typedef struct DirList {
    DirEntry *items;
    size_t n;
} DirList;

bool fs_exists(const char *path);
bool fs_is_dir(const char *path);
bool fs_is_file(const char *path);
/* Milliseconds since Unix epoch; 0 when the path cannot be stat'ed. */
int64_t fs_mtime_ms(const char *path);

char *fs_read_all(Arena *a, const char *path, size_t *out_len, Err *e);
bool fs_write_all(const char *path, const void *data, size_t n, Err *e);
bool fs_mkdir_p(const char *path, Err *e);
bool fs_list_dir(Arena *a, const char *path, DirList *out, Err *e);

char *path_norm(Arena *a, const char *p);
char *path_resolve(Arena *a, const char *base, const char *p);
char *path_join(Arena *a, const char *base, const char *part);
char *path_join3(Arena *a, const char *x, const char *y, const char *z);
const char *path_basename(const char *path);
char *path_dirname(Arena *a, const char *path);
bool path_is_abs(const char *path);
bool path_ends_with_ci(const char *path, const char *suffix);
char *fs_cwd(Arena *a);
char *fs_home(Arena *a);
/* 当前用户桌面目录（SHGetFolderPathW，失败时退 %USERPROFILE%\Desktop）。 */
char *fs_desktop(Arena *a);
char *fs_exe_path(Arena *a);
/* UTF-8 environment variable; NULL when unset. */
char *env_get(Arena *a, const char *name);

#endif /* EUI_FS_H */
