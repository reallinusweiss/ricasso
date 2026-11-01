#ifndef SRE13_GLOSS_H
#define SRE13_GLOSS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#if (defined __clang__) || (defined __GNUC__)
#define GLOSS_API __attribute__((visibility("default")))
#else
#define GLOSS_API
#endif

typedef enum { I_NONE = 0, I_THUMB, I_ARM, I_ARM64 } i_set;
typedef void* GHandle;
typedef void* GHook;

struct GlossHookCallback_t {
    GHook hook;
    const char* path;
    void* addr;
    size_t size;
};
typedef void (*GlossHookCallback)(const struct GlossHookCallback_t info);

GLOSS_API void GlossInit(bool is_init_linker);
GLOSS_API void GlossEnableLog(bool enable);
GLOSS_API GHandle GlossOpen(const char* lib_name);
GLOSS_API int GlossClose(GHandle handle, bool is_dlclose);
GLOSS_API uintptr_t GlossGetLibBias(const char* lib_name);
GLOSS_API uintptr_t GlossSymbol(GHandle handle, const char* symbol, size_t* sym_size);
GLOSS_API uintptr_t GlossGetLibSection(const char* lib_name, const char* sec_name, size_t* sec_size);

GLOSS_API GHook GlossHook(void* sym_addr, void* new_func, void** old_func);
GLOSS_API GHook GlossHookAddr(void* func_addr, void* new_func, void** old_func, bool is_4_byte_hook, i_set mode);
GLOSS_API GHook GlossHookByName(const char* lib_name, const char* sym_name, void* new_func, void** old_func, GlossHookCallback call_back_func);
GLOSS_API GHook GlossHookAddrByName(const char* lib_name, uintptr_t offset_addr, void* new_func, void** old_func, bool is_4_byte_hook, i_set mode, GlossHookCallback call_back_func);
GLOSS_API void GlossHookDelete(GHook hook);
GLOSS_API void GlossHookDisable(GHook hook);
GLOSS_API void GlossHookEnable(GHook hook);

/* Mod hook tracking & capture (Lawncher parity) */
void hook_begin_mod_capture(void);
void hook_end_mod_capture(void);
void hook_delete_mod_hooks(void);
void hook_track(GHook h);

/* Java & Lawncher environment shims exported for mods */
GLOSS_API const char* java_current_mod_id(void);
GLOSS_API const char* java_resource_path(const char* res);
GLOSS_API const char* java_internal_files(void);
GLOSS_API const char* java_internal_cache(void);
GLOSS_API const char* java_external_files(void);
GLOSS_API void java_resolve(void);

#ifdef __cplusplus
}
#endif

#endif /* SRE13_GLOSS_H */
