#ifndef SRE13_HOOK_H
#define SRE13_HOOK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "sre_host_abi.h"

#if defined(__aarch64__)
#define archSplit(arm_val, arm64_val) (arm64_val)
#elif defined(__arm__)
#define archSplit(arm_val, arm64_val) (arm_val)
#else
#define archSplit(arm_val, arm64_val) (arm64_val)
#endif

#define OFFSET(off32, off64) ((uintptr_t)archSplit(off32, off64))
#define $(type, base, off32, off64) \
    ((type*)((uintptr_t)(base) + OFFSET(off32, off64)))

/* Hook installer registration */
typedef void (*sre_hook_installer_t)(void);
#define SRE_HOOK_INSTALLER_CAP 512
extern sre_hook_installer_t g_sre_hook_installers[SRE_HOOK_INSTALLER_CAP];
extern int g_sre_hook_installer_count;
void sre_register_hook_installer(sre_hook_installer_t fn);

/* Dynamic symbol resolver registration */
typedef void (*dl_resolver_t)(void);
#define DL_RESOLVER_CAP 512
extern dl_resolver_t g_dl_resolvers[DL_RESOLVER_CAP];
extern int g_dl_resolver_count;
void dl_register_resolver(dl_resolver_t fn);
void dl_resolve_all(void);

void init_hooks(void);

#include "Gloss.h"

/* Macro: HOOK_SYMBOL */
#define HOOK_SYMBOL(name, symbol_str, ret, args)                          \
    typedef ret (*name##_t) args;                                         \
    static name##_t orig_##name = NULL;                                   \
    static ret hook_##name args;                                          \
    static void install_##name(void) {                                    \
        uint64_t target = srehost_get_symbol(symbol_str);                 \
        if (target) {                                                     \
            srehost_install_hook(target, (void*)hook_##name,              \
                                 (void**)&orig_##name, 0);                \
        }                                                                 \
    }                                                                     \
    __attribute__((constructor))                                          \
    static void register_##name(void) {                                   \
        sre_register_hook_installer(install_##name);                      \
    }                                                                     \
    static ret hook_##name args

/* Macro: HOOK_OFFSET */
extern uint64_t g_swordigo_base;
#define HOOK_OFFSET(name, off32, off64, ret, args)                        \
    typedef ret (*name##_t) args;                                         \
    static name##_t orig_##name = NULL;                                   \
    static ret hook_##name args;                                          \
    static void install_##name(void) {                                    \
        uint64_t target = g_swordigo_base + OFFSET(off32, off64);         \
        if (target) {                                                     \
            srehost_install_hook(target, (void*)hook_##name,              \
                                 (void**)&orig_##name, 0);                \
        }                                                                 \
    }                                                                     \
    __attribute__((constructor))                                          \
    static void register_##name(void) {                                   \
        sre_register_hook_installer(install_##name);                      \
    }                                                                     \
    static ret hook_##name args

/* Macro: DL_SYMBOL (single compilation unit) */
#define DL_SYMBOL(name, symbol_str, ret, args)                            \
    typedef ret (*name##_t) args;                                         \
    static name##_t name = NULL;                                          \
    static void resolve_##name(void) {                                    \
        if (name) return;                                                 \
        name = (name##_t)srehost_get_symbol(symbol_str);                  \
    }                                                                     \
    __attribute__((constructor))                                          \
    static void register_##name(void) {                                   \
        dl_register_resolver(resolve_##name);                             \
    }                                                                     \
    struct __dl_symbol_semicolon_##name { int _unused; }

/* Macro: DL_SYMBOL_DECL and G_DL_SYMBOL (cross-file) */
#define DL_SYMBOL_DECL(name, ret, args)                                   \
    typedef ret (*name##_t) args;                                         \
    extern name##_t name;                                                 \
    void resolve_##name(void)

#define G_DL_SYMBOL(name, symbol_str, ret, args)                          \
    typedef ret (*name##_t) args;                                         \
    name##_t name = NULL;                                                 \
    void resolve_##name(void) {                                           \
        if (name) return;                                                 \
        name = (name##_t)srehost_get_symbol(symbol_str);                  \
    }                                                                     \
    __attribute__((constructor))                                          \
    static void register_##name(void) {                                   \
        dl_register_resolver(resolve_##name);                             \
    }

void *swordigo_dlsym(const char *symbol);

#endif /* SRE13_HOOK_H */
