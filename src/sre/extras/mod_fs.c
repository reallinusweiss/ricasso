/* mod_fs.c — libsre-extras (GNU GPLv3 / extras)
 *
 * Complete sandboxed filesystem API for mods, adapted from Kiwi Lawncher's fs.c.
 * Provides ALL fs functions — supersedes SRE's weaker implementations.
 */

#include "sre_extras.h"
#include "sre_lua.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

#define FS_MAX_VPATH 480
#define FS_MAX_REAL  960
#define FS_MAX_FILE  (32 * 1024 * 1024)

typedef enum { FS_DENY = 0, FS_READ = 1, FS_RW = 2 } fs_access_t;

static char g_mod_dir[512] = {0};

void sre_extras_fs_set_mod_dir(const char* dir) {
    if (dir) {
        size_t len = strlen(dir);
        if (len >= sizeof(g_mod_dir)) len = sizeof(g_mod_dir) - 1;
        memcpy(g_mod_dir, dir, len);
        g_mod_dir[len] = '\0';
    } else {
        g_mod_dir[0] = '\0';
    }
}

static void fs_ensure_dir(const char* path) {
    char buf[FS_MAX_REAL];
    snprintf(buf, sizeof(buf), "%s", path);
    for (char* p = buf + 1; *p; p++) {
        if (*p == '/') { *p = '\0'; mkdir(buf, 0770); *p = '/'; }
    }
    mkdir(buf, 0770);
}

static int fs_has_traversal(const char* p) {
    for (const char* c = p; *c; c++) {
        if (c[0] == '.' && c[1] == '.' && (c == p || c[-1] == '/') && (c[2] == '/' || c[2] == '\0'))
            return 1;
    }
    return 0;
}

static int fs_mount_match(const char* vpath, const char* mount, char* rel, size_t rel_sz) {
    size_t mlen = strlen(mount);
    if (strncmp(vpath, mount, mlen) != 0) return 0;
    if (vpath[mlen] == '\0') { rel[0] = '\0'; return 1; }
    if (vpath[mlen] != '/') return 0;
    snprintf(rel, rel_sz, "%s", vpath + mlen);
    return 1;
}

static int fs_resolve(const char* vpath, char* real, size_t real_sz, fs_access_t* acc) {
    if (!vpath || vpath[0] != '/') return 0;
    if (strlen(vpath) >= FS_MAX_VPATH) return 0;
    if (fs_has_traversal(vpath)) return 0;
    if (g_mod_dir[0] == '\0') return 0;
    if (strcmp(vpath, "/properties.toml") == 0) {
        snprintf(real, real_sz, "%s/properties.toml", g_mod_dir); *acc = FS_READ; return 1;
    }
    if (strcmp(vpath, "/icon.png") == 0) {
        snprintf(real, real_sz, "%s/icon.png", g_mod_dir); *acc = FS_READ; return 1;
    }
    char rel[FS_MAX_VPATH];
    if (fs_mount_match(vpath, "/resources", rel, sizeof(rel))) {
        snprintf(real, real_sz, "%s/resources%s", g_mod_dir, rel); *acc = FS_RW; return 1;
    }
    if (fs_mount_match(vpath, "/kiwi", rel, sizeof(rel))) {
        snprintf(real, real_sz, "%s/kiwi%s", g_mod_dir, rel); *acc = FS_RW; return 1;
    }
    return 0;
}

static void fs_ensure_parent(const char* real) {
    char parent[FS_MAX_REAL];
    snprintf(parent, sizeof(parent), "%s", real);
    char* slash = strrchr(parent, '/');
    if (slash) { *slash = '\0'; fs_ensure_dir(parent); }
}

/* fs.read(path) -> string | nil, err */
static int l_fs_extra_read(lua_State* L) {
    const char* vpath = lua_tostring(L, 1);
    if (!vpath) { lua_pushnil(L); lua_pushstring(L, "fs: missing path"); return 2; }
    char real[FS_MAX_REAL]; fs_access_t acc;
    if (!fs_resolve(vpath, real, sizeof(real), &acc)) {
        lua_pushnil(L); lua_pushfstring(L, "fs: invalid path '%s'", vpath); return 2;
    }
    FILE* fp = fopen(real, "rb");
    if (!fp) { lua_pushnil(L); lua_pushfstring(L, "fs: cannot open '%s'", vpath); return 2; }
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz <= 0 || (size_t)sz > FS_MAX_FILE) { fclose(fp); lua_pushnil(L); lua_pushstring(L, "fs: empty/too large"); return 2; }
    size_t size = (size_t)sz;
    char* buf = (char*)malloc(size);
    if (!buf) { fclose(fp); lua_pushnil(L); lua_pushstring(L, "fs: OOM"); return 2; }
    size_t total = 0;
    while (total < size) { size_t n = fread(buf + total, 1, size - total, fp); if (n == 0) break; total += n; }
    fclose(fp);
    if (total != size) { free(buf); lua_pushnil(L); lua_pushstring(L, "fs: short read"); return 2; }
    if (g_lua_pushlstring) g_lua_pushlstring(L, buf, size);
    else lua_pushlstring(L, buf, size);
    free(buf);
    return 1;
}

/* fs.write(path, data) -> bool | nil, err */
static int l_fs_extra_write(lua_State* L) {
    const char* vpath = lua_tostring(L, 1);
    size_t len = 0;
    const char* data = lua_tolstring(L, 2, &len);
    if (!vpath || !data) { lua_pushnil(L); lua_pushstring(L, "fs.write: bad args"); return 2; }
    char real[FS_MAX_REAL]; fs_access_t acc;
    if (!fs_resolve(vpath, real, sizeof(real), &acc)) {
        lua_pushnil(L); lua_pushfstring(L, "fs: invalid path '%s'", vpath); return 2;
    }
    if (acc != FS_RW) { lua_pushnil(L); lua_pushfstring(L, "fs: '%s' read-only", vpath); return 2; }
    if (len > FS_MAX_FILE) { lua_pushnil(L); lua_pushstring(L, "fs.write: too large"); return 2; }
    fs_ensure_parent(real);
    char tmp[FS_MAX_REAL + 8];
    snprintf(tmp, sizeof(tmp), "%s.tmp", real);
    FILE* fp = fopen(tmp, "wb");
    if (!fp) { lua_pushnil(L); lua_pushfstring(L, "fs: cannot write '%s'", vpath); return 2; }
    size_t w = fwrite(data, 1, len, fp);
    fclose(fp);
    if (w != len) { remove(tmp); lua_pushnil(L); lua_pushstring(L, "fs: short write"); return 2; }
    if (rename(tmp, real) != 0) { remove(tmp); lua_pushnil(L); lua_pushstring(L, "fs: commit failed"); return 2; }
    lua_pushboolean(L, 1);
    return 1;
}

/* fs.append(path, data) -> bool | nil, err */
static int l_fs_extra_append(lua_State* L) {
    const char* vpath = lua_tostring(L, 1);
    size_t len = 0;
    const char* data = lua_tolstring(L, 2, &len);
    if (!vpath || !data) { lua_pushnil(L); lua_pushstring(L, "fs.append: bad args"); return 2; }
    char real[FS_MAX_REAL]; fs_access_t acc;
    if (!fs_resolve(vpath, real, sizeof(real), &acc)) {
        lua_pushnil(L); lua_pushfstring(L, "fs: invalid path '%s'", vpath); return 2;
    }
    if (acc != FS_RW) { lua_pushnil(L); lua_pushfstring(L, "fs: '%s' read-only", vpath); return 2; }
    fs_ensure_parent(real);
    FILE* fp = fopen(real, "ab");
    if (!fp) { lua_pushnil(L); lua_pushfstring(L, "fs: cannot append '%s'", vpath); return 2; }
    size_t w = fwrite(data, 1, len, fp);
    fclose(fp);
    if (w != len) { lua_pushnil(L); lua_pushstring(L, "fs: short append"); return 2; }
    lua_pushboolean(L, 1);
    return 1;
}

/* fs.exists(path) -> bool */
static int l_fs_extra_exists(lua_State* L) {
    const char* vpath = lua_tostring(L, 1);
    if (!vpath) { lua_pushboolean(L, 0); return 1; }
    char real[FS_MAX_REAL]; fs_access_t acc;
    if (!fs_resolve(vpath, real, sizeof(real), &acc)) { lua_pushboolean(L, 0); return 1; }
    struct stat st;
    lua_pushboolean(L, stat(real, &st) == 0);
    return 1;
}

/* fs.remove(path) -> bool | nil, err */
static int l_fs_extra_remove(lua_State* L) {
    const char* vpath = lua_tostring(L, 1);
    if (!vpath) { lua_pushnil(L); lua_pushstring(L, "fs.remove: no path"); return 2; }
    char real[FS_MAX_REAL]; fs_access_t acc;
    if (!fs_resolve(vpath, real, sizeof(real), &acc)) {
        lua_pushnil(L); lua_pushfstring(L, "fs: invalid path '%s'", vpath); return 2;
    }
    if (acc != FS_RW) { lua_pushnil(L); lua_pushfstring(L, "fs: '%s' read-only", vpath); return 2; }
    int rc = remove(real);
    if (rc != 0) { lua_pushnil(L); lua_pushfstring(L, "fs: cannot remove '%s'", vpath); return 2; }
    lua_pushboolean(L, 1);
    return 1;
}

/* fs.mkdir(path) -> bool | nil, err */
static int l_fs_extra_mkdir(lua_State* L) {
    const char* vpath = lua_tostring(L, 1);
    if (!vpath) { lua_pushnil(L); lua_pushstring(L, "fs.mkdir: no path"); return 2; }
    char real[FS_MAX_REAL]; fs_access_t acc;
    if (!fs_resolve(vpath, real, sizeof(real), &acc)) {
        lua_pushnil(L); lua_pushfstring(L, "fs: invalid path '%s'", vpath); return 2;
    }
    if (acc != FS_RW) { lua_pushnil(L); lua_pushfstring(L, "fs: '%s' read-only", vpath); return 2; }
    fs_ensure_dir(real);
    lua_pushboolean(L, 1);
    return 1;
}

/* fs.rmdir(path) -> bool | nil, err */
static int l_fs_extra_rmdir(lua_State* L) {
    const char* vpath = lua_tostring(L, 1);
    if (!vpath) { lua_pushnil(L); lua_pushstring(L, "fs.rmdir: no path"); return 2; }
    char real[FS_MAX_REAL]; fs_access_t acc;
    if (!fs_resolve(vpath, real, sizeof(real), &acc)) {
        lua_pushnil(L); lua_pushfstring(L, "fs: invalid path '%s'", vpath); return 2;
    }
    if (acc != FS_RW) { lua_pushnil(L); lua_pushfstring(L, "fs: '%s' read-only", vpath); return 2; }
    int rc = rmdir(real);
    if (rc != 0) { lua_pushnil(L); lua_pushfstring(L, "fs: cannot rmdir '%s'", vpath); return 2; }
    lua_pushboolean(L, 1);
    return 1;
}

/* fs.dir(path) -> iterator function */
static int l_fs_extra_dir_iter(lua_State* L) {
    DIR* dp = (DIR*)lua_touserdata(L, lua_upvalueindex(1));
    if (!dp) { lua_pushnil(L); return 1; }
    struct dirent* e;
    while ((e = readdir(dp)) != NULL) {
        if (e->d_name[0] == '.') continue;
        lua_pushstring(L, e->d_name);
        return 1;
    }
    closedir(dp);
    lua_pushnil(L);
    return 1;
}

static int l_fs_extra_dir(lua_State* L) {
    const char* vpath = lua_tostring(L, 1);
    if (!vpath) { lua_pushnil(L); return 1; }
    char real[FS_MAX_REAL]; fs_access_t acc;
    if (!fs_resolve(vpath, real, sizeof(real), &acc)) { lua_pushnil(L); return 1; }
    DIR* dp = opendir(real);
    if (!dp) { lua_pushnil(L); return 1; }
    lua_pushlightuserdata(L, dp);
    lua_pushcclosure(L, l_fs_extra_dir_iter, 1);
    return 1;
}

/* fs.attributes(path) -> table {mode, size, modification, isDir} or nil */
static int l_fs_extra_attributes(lua_State* L) {
    const char* vpath = lua_tostring(L, 1);
    if (!vpath) { lua_newtable(L); return 1; }
    char real[FS_MAX_REAL]; fs_access_t acc;
    if (!fs_resolve(vpath, real, sizeof(real), &acc)) { lua_newtable(L); return 1; }
    struct stat st;
    if (stat(real, &st) != 0) { lua_newtable(L); return 1; }
    lua_newtable(L);
    lua_pushinteger(L, (lua_Integer)st.st_size); lua_setfield(L, -2, "size");
    lua_pushinteger(L, (lua_Integer)st.st_mode); lua_setfield(L, -2, "mode");
    lua_pushinteger(L, (lua_Integer)st.st_mtime); lua_setfield(L, -2, "modification");
    lua_pushboolean(L, S_ISDIR(st.st_mode)); lua_setfield(L, -2, "isDir");
    return 1;
}

/* fs.list(path) -> table of {name, isDir, size} | nil, err */
static int l_fs_extra_list(lua_State* L) {
    const char* vpath = lua_tostring(L, 1);
    if (!vpath) vpath = "/resources";
    char real[FS_MAX_REAL]; fs_access_t acc;
    if (!fs_resolve(vpath, real, sizeof(real), &acc)) {
        lua_pushnil(L); lua_pushfstring(L, "fs: invalid path '%s'", vpath); return 2;
    }
    if (acc == FS_READ) { lua_pushnil(L); lua_pushfstring(L, "fs: '%s' not a dir", vpath); return 2; }
    DIR* dir = opendir(real);
    lua_newtable(L);
    if (!dir) return 1;
    int i = 1;
    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        char child[FS_MAX_REAL];
        snprintf(child, sizeof(child), "%s/%s", real, ent->d_name);
        struct stat st;
        if (stat(child, &st) != 0) continue;
        lua_newtable(L);
        lua_pushstring(L, ent->d_name); lua_setfield(L, -2, "name");
        lua_pushboolean(L, S_ISDIR(st.st_mode)); lua_setfield(L, -2, "isDir");
        if (S_ISREG(st.st_mode)) { lua_pushinteger(L, (lua_Integer)st.st_size); lua_setfield(L, -2, "size"); }
        lua_rawseti(L, -2, i++);
    }
    closedir(dir);
    return 1;
}

/* Registration — complete fs API, supersedes SRE's weaker versions */
void sre_extras_fs_register(lua_State* L) {
    lua_getfield(L, LUA_GLOBALSINDEX, "fs");
    if (lua_isnil(L, -1) || !lua_istable(L, -1)) { lua_pop(L, 1); lua_newtable(L); }
    lua_pushcfunction(L, l_fs_extra_read);       lua_setfield(L, -2, "read");
    lua_pushcfunction(L, l_fs_extra_write);      lua_setfield(L, -2, "write");
    lua_pushcfunction(L, l_fs_extra_append);     lua_setfield(L, -2, "append");
    lua_pushcfunction(L, l_fs_extra_exists);     lua_setfield(L, -2, "exists");
    lua_pushcfunction(L, l_fs_extra_remove);     lua_setfield(L, -2, "remove");
    lua_pushcfunction(L, l_fs_extra_mkdir);      lua_setfield(L, -2, "mkdir");
    lua_pushcfunction(L, l_fs_extra_rmdir);      lua_setfield(L, -2, "rmdir");
    lua_pushcfunction(L, l_fs_extra_dir);        lua_setfield(L, -2, "dir");
    lua_pushcfunction(L, l_fs_extra_attributes); lua_setfield(L, -2, "attributes");
    lua_pushcfunction(L, l_fs_extra_list);       lua_setfield(L, -2, "list");
    lua_setfield(L, LUA_GLOBALSINDEX, "fs");
}
