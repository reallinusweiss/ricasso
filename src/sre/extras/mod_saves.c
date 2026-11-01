/* mod_saves.c — libsre-extras (GNU GPLv3 / extras)
 *
 * Per-mod save file isolation, adapted from Kiwi Lawncher's assets.c.
 * Redirects Documents/ reads/writes to per-mod save directories so each
 * mod has its own independent save state.
 *
 * On-disk layout:
 *   <data_dir>/mods/<mod_id>/saves/     — mod's save directory
 *   <data_dir>/Documents/               — vanilla save directory (source for seeding)
 *
 * When a mod is active:
 *   - Reads from Documents/ are redirected to mods/<id>/saves/
 *   - Writes to Documents/ go to mods/<id>/saves/
 *   - On first access, vanilla saves are seeded into the mod's save dir
 *
 * Hooked engine functions (via SRE host ABI):
 *   - Caver::NewByteBufferFromFile
 *   - Caver::SaveByteBufferToFile
 *   - Caver::FileExistsAtPath
 *   - Caver::GetFilesWithExtension
 *   - Caver::DeleteFileAtPath
 *
 * Adapted from Kiwi Lawncher (GPL, permission granted by dev team).
 */

#include "sre_extras.h"
#include "sre_lua.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

/* ------------------------------------------------------------------ */
/* Configuration — set by host during extras init                      */
/* ------------------------------------------------------------------ */

static char g_saves_dir[512] = {0};
static char g_documents_dir[512] = {0};
static int g_saves_seeded = 0;

void sre_extras_saves_init(const char* mod_saves_dir, const char* documents_dir) {
    if (mod_saves_dir) {
        size_t len = strlen(mod_saves_dir);
        if (len >= sizeof(g_saves_dir)) len = sizeof(g_saves_dir) - 1;
        memcpy(g_saves_dir, mod_saves_dir, len);
        g_saves_dir[len] = '\0';
    }
    if (documents_dir) {
        size_t len = strlen(documents_dir);
        if (len >= sizeof(g_documents_dir)) len = sizeof(g_documents_dir) - 1;
        memcpy(g_documents_dir, documents_dir, len);
        g_documents_dir[len] = '\0';
    }
    g_saves_seeded = 0;
}

int sre_extras_saves_active(void) {
    return g_saves_dir[0] != '\0';
}

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static void ensure_dir(const char* path) {
    char buf[512];
    snprintf(buf, sizeof(buf), "%s", path);
    for (char* p = buf + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(buf, 0770);
            *p = '/';
        }
    }
    mkdir(buf, 0770);
}

static const char* path_basename(const char* path) {
    const char* slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static int is_documents_path(const char* path) {
    return path && strstr(path, "/Documents/") != NULL;
}

static int copy_file(const char* src, const char* dst) {
    FILE* in = fopen(src, "rb");
    if (!in) return 0;

    FILE* out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return 0;
    }

    char buf[8192];
    size_t n;
    int ok = 1;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            ok = 0;
            break;
        }
    }

    fclose(in);
    fclose(out);
    if (!ok) remove(dst);
    return ok;
}

static void seed_saves_from_vanilla(void) {
    if (g_saves_seeded || g_documents_dir[0] == '\0' || g_saves_dir[0] == '\0')
        return;

    DIR* dir = opendir(g_documents_dir);
    if (!dir) {
        g_saves_seeded = 1;
        return;
    }

    int copied = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char src[512], dst[512];
        snprintf(src, sizeof(src), "%s/%s", g_documents_dir, entry->d_name);
        snprintf(dst, sizeof(dst), "%s/%s", g_saves_dir, entry->d_name);

        struct stat st;
        if (stat(src, &st) != 0 || !S_ISREG(st.st_mode)) continue;

        if (copy_file(src, dst)) copied++;
    }
    closedir(dir);
    g_saves_seeded = 1;
}

static void make_mod_save_path(char* out, size_t out_size, const char* original) {
    snprintf(out, out_size, "%s/%s", g_saves_dir, path_basename(original));
}

/* ------------------------------------------------------------------ */
/* Hook: Caver::NewByteBufferFromFile                                  */
/* Redirects Documents/ reads to per-mod save dir                      */
/* ------------------------------------------------------------------ */
void* sre_extras_hook_byte_buffer_from_file(void* path_str, unsigned int* out_size) {
    /* path_str is an engine std::string* — extract the C string via the
     * ABI-checked layout (g_sre_extras_abi.cppstring_data_off, filled by
     * the host per engine version). Never a hardcoded offset here. */
    const char* file_path = sre_extras_cppstring_data(path_str);

    if (file_path && is_documents_path(file_path) && sre_extras_saves_active()) {
        seed_saves_from_vanilla();

        char redirected[600];
        make_mod_save_path(redirected, sizeof(redirected), file_path);

        FILE* f = fopen(redirected, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fseek(f, 0, SEEK_SET);

            if (size > 0) {
                void* buffer = malloc(size);
                if (buffer) {
                    size_t read_bytes = fread(buffer, 1, size, f);
                    fclose(f);
                    if (read_bytes == (size_t)size) {
                        if (out_size) *out_size = (unsigned int)size;
                        return buffer;
                    }
                    free(buffer);
                } else {
                    fclose(f);
                }
            } else {
                fclose(f);
            }
        }
        /* No mod-specific save — return NULL to let engine handle */
        return NULL;
    }

    return NULL; /* Let SRE's normal path handle it */
}

/* ------------------------------------------------------------------ */
/* Hook: Caver::SaveByteBufferToFile                                  */
/* Redirects Documents/ writes to per-mod save dir                     */
/* ------------------------------------------------------------------ */
int sre_extras_hook_save_byte_buffer_to_file(void* data, unsigned int size, void* path_str) {
    const char* file_path = sre_extras_cppstring_data(path_str);

    if (file_path && is_documents_path(file_path) && sre_extras_saves_active()) {
        char redirected[600];
        make_mod_save_path(redirected, sizeof(redirected), file_path);

        char tmp[620];
        snprintf(tmp, sizeof(tmp), "%s.tmp", redirected);

        ensure_dir(redirected); /* ensure parent dir exists */

        FILE* f = fopen(tmp, "wb");
        if (f) {
            size_t written = fwrite(data, 1, size, f);
            fclose(f);

            if (written == size) {
                if (rename(tmp, redirected) == 0) {
                    return 1;
                }
                remove(tmp);
            }
        }
    }

    return -1; /* Let SRE's normal path handle it */
}

/* ------------------------------------------------------------------ */
/* Hook: Caver::FileExistsAtPath                                      */
/* ------------------------------------------------------------------ */
int sre_extras_hook_file_exists_at_path(void* path_str) {
    const char* file_path = sre_extras_cppstring_data(path_str);

    if (file_path && is_documents_path(file_path) && sre_extras_saves_active()) {
        char redirected[600];
        make_mod_save_path(redirected, sizeof(redirected), file_path);

        struct stat st;
        return (stat(redirected, &st) == 0 && S_ISREG(st.st_mode)) ? 1 : 0;
    }

    return -1; /* Let SRE's normal path handle it */
}

/* ------------------------------------------------------------------ */
/* Hook: Caver::DeleteFileAtPath                                       */
/* ------------------------------------------------------------------ */
void sre_extras_hook_delete_file_at_path(void* path_str) {
    const char* file_path = sre_extras_cppstring_data(path_str);

    if (file_path && is_documents_path(file_path) && sre_extras_saves_active()) {
        char redirected[600];
        make_mod_save_path(redirected, sizeof(redirected), file_path);
        remove(redirected);
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void sre_extras_init_saves(void) {
    if (g_saves_dir[0]) {
        ensure_dir(g_saves_dir);
    }
}
