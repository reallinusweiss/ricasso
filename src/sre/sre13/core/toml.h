#ifndef SRE13_TOML_H
#define SRE13_TOML_H

#include "map.h"

/* Minimal TOML subset parser (flat key = "string" pairs, # comments, \\u
 * escapes, basic + multiline strings) — ported from the lawncher reference.
 * Loads kv pairs into a strmap. Returns pair count, or -1 on I/O error. */
int toml_load_string_map(const char *path, Map *out);

#endif /* SRE13_TOML_H */