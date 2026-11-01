#define LOG_TAG "GlossCompat"
#include "Gloss.h"
#include "sre_host_abi.h"
#include "hook.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern uint64_t g_swordigo_base;
extern const char* sre13_get_active_mod(void);

#define MOD_HOOK_CAP 512
static GHook g_mod_hooks[MOD_HOOK_CAP];
static int g_mod_hook_count = 0;
static int g_capturing_mod_hooks = 0;

void hook_track(GHook h) {
    if (!h || g_mod_hook_count >= MOD_HOOK_CAP) return;
    g_mod_hooks[g_mod_hook_count++] = h;
}

void hook_begin_mod_capture(void) {
    g_capturing_mod_hooks = 1;
}

void hook_end_mod_capture(void) {
    g_capturing_mod_hooks = 0;
}

void hook_delete_mod_hooks(void) {
    LOGI("Deleting %d tracked mod hook(s)...", g_mod_hook_count);
    for (int i = g_mod_hook_count - 1; i >= 0; i--) {
        if (g_mod_hooks[i]) {
            GlossHookDelete(g_mod_hooks[i]);
            g_mod_hooks[i] = NULL;
        }
    }
    g_mod_hook_count = 0;
}

GLOSS_API void GlossInit(bool is_init_linker) {
    (void)is_init_linker;
    LOGI("GlossInit called (SRE host hooking active)");
}

GLOSS_API void GlossEnableLog(bool enable) {
    (void)enable;
}

GLOSS_API GHandle GlossOpen(const char* lib_name) {
    (void)lib_name;
    return (GHandle)1;
}

GLOSS_API int GlossClose(GHandle handle, bool is_dlclose) {
    (void)handle;
    (void)is_dlclose;
    return 0;
}

GLOSS_API uintptr_t GlossGetLibBias(const char* lib_name) {
    (void)lib_name;
    return (uintptr_t)g_swordigo_base;
}

GLOSS_API uintptr_t GlossSymbol(GHandle handle, const char* symbol, size_t* sym_size) {
    (void)handle;
    if (!symbol || !*symbol) return 0;
    uint64_t addr = srehost_get_symbol(symbol);
    if (sym_size) *sym_size = 0;
    return (uintptr_t)addr;
}

GLOSS_API uintptr_t GlossGetLibSection(const char* lib_name, const char* sec_name, size_t* sec_size) {
    (void)lib_name;
    (void)sec_name;
    if (sec_size) *sec_size = 0;
    return (uintptr_t)g_swordigo_base;
}

GLOSS_API GHook GlossHook(void* sym_addr, void* new_func, void** old_func) {
    if (!sym_addr || !new_func) {
        LOGE("GlossHook: null argument (addr=%p, proxy=%p)", sym_addr, new_func);
        return NULL;
    }
    SREHost_HookHandle h = srehost_install_hook((uint64_t)sym_addr, new_func, old_func, 0);
    if (h) {
        if (g_capturing_mod_hooks) hook_track((GHook)h);
        LOGI("GlossHook installed at %p -> %p", sym_addr, new_func);
    } else {
        LOGE("GlossHook failed for target %p", sym_addr);
    }
    return (GHook)h;
}

GLOSS_API GHook GlossHookByName(const char* lib_name, const char* sym_name, void* new_func, void** old_func, GlossHookCallback cb) {
    if (!sym_name || !new_func) {
        LOGE("GlossHookByName: null symbol or proxy");
        return NULL;
    }
    uint64_t target = srehost_get_symbol(sym_name);
    if (!target) {
        LOGE("GlossHookByName: symbol \"%s\" not found", sym_name);
        return NULL;
    }
    SREHost_HookHandle h = srehost_install_hook(target, new_func, old_func, 0);
    if (h) {
        if (g_capturing_mod_hooks) hook_track((GHook)h);
        if (cb) {
            struct GlossHookCallback_t info;
            info.hook = (GHook)h;
            info.path = lib_name;
            info.addr = (void*)target;
            info.size = 0;
            cb(info);
        }
        LOGI("GlossHookByName(\"%s\") installed at 0x%llx -> %p", sym_name, (unsigned long long)target, new_func);
    } else {
        LOGE("GlossHookByName(\"%s\") failed to hook at 0x%llx", sym_name, (unsigned long long)target);
    }
    return (GHook)h;
}

GLOSS_API GHook GlossHookAddr(void* func_addr, void* new_func, void** old_func, bool is_4_byte_hook, i_set mode) {
    (void)is_4_byte_hook;
    (void)mode;
    return GlossHook(func_addr, new_func, old_func);
}

GLOSS_API GHook GlossHookAddrByName(const char* lib_name, uintptr_t offset_addr, void* new_func, void** old_func, bool is_4_byte_hook, i_set mode, GlossHookCallback cb) {
    (void)is_4_byte_hook;
    (void)mode;
    if (!new_func) return NULL;
    uint64_t target = g_swordigo_base + offset_addr;
    SREHost_HookHandle h = srehost_install_hook(target, new_func, old_func, 0);
    if (h) {
        if (g_capturing_mod_hooks) hook_track((GHook)h);
        if (cb) {
            struct GlossHookCallback_t info;
            info.hook = (GHook)h;
            info.path = lib_name;
            info.addr = (void*)target;
            info.size = 0;
            cb(info);
        }
        LOGI("GlossHookAddrByName offset 0x%llx -> 0x%llx", (unsigned long long)offset_addr, (unsigned long long)target);
    }
    return (GHook)h;
}

GLOSS_API void GlossHookDelete(GHook hook) {
    if (!hook) return;
    srehost_remove_hook((SREHost_HookHandle)hook);
}

GLOSS_API void GlossHookDisable(GHook hook) {
    (void)hook;
    LOGI("GlossHookDisable called (no-op in trampoline mode)");
}

GLOSS_API void GlossHookEnable(GHook hook) {
    (void)hook;
    LOGI("GlossHookEnable called (no-op in trampoline mode)");
}

/* Java environment shims for Lawncher mod compatibility */
GLOSS_API const char* java_current_mod_id(void) {
    return sre13_get_active_mod();
}

GLOSS_API const char* java_resource_path(const char* res) {
    static char s_res_buf[512];
    const char* id = sre13_get_active_mod();
    if (id && *id) {
        snprintf(s_res_buf, sizeof(s_res_buf), "mods/%s/resources/%s", id, res ? res : "");
    } else {
        snprintf(s_res_buf, sizeof(s_res_buf), "%s", res ? res : "");
    }
    return s_res_buf;
}

GLOSS_API const char* java_internal_files(void) {
    return "save";
}

GLOSS_API const char* java_internal_cache(void) {
    return "cache";
}

GLOSS_API const char* java_external_files(void) {
    return ".";
}

GLOSS_API void java_resolve(void) {
    /* No-op on PC — already resolved statically */
}

/* Dynamic symbol resolver shims */
GLOSS_API void* dlopen(const char* filename, int flags) {
    (void)flags;
    LOGI("dlopen(\"%s\") -> handle 1", filename ? filename : "NULL");
    return (void*)1;
}

GLOSS_API void* dlsym(void* handle, const char* symbol) {
    (void)handle;
    if (!symbol || !*symbol) return NULL;
    uint64_t addr = srehost_get_symbol(symbol);
    if (addr) return (void*)addr;
    return NULL;
}

GLOSS_API int dlclose(void* handle) {
    (void)handle;
    return 0;
}

GLOSS_API char* dlerror(void) {
    return NULL;
}
