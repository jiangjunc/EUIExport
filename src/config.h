/* config.h - persistent user settings (~/.euiexport.json). */
#ifndef EUI_CONFIG_H
#define EUI_CONFIG_H

#include "json.h"
#include "util.h"

typedef struct Config {
    Arena *a;
    J *root;
    char *path;
} Config;

/* The environment variable EGGY_EUI_CONFIG overrides the default location. */
char *config_path(Arena *a);
void config_load(Config *cfg, Arena *a, Err *e);
bool config_save(const Config *cfg, Err *e);
const char *config_get_str(const Config *cfg, const char *key);

#endif /* EUI_CONFIG_H */
