/*
 * sre_host_abi.h — Guest-Side SRE Host ABI for SRE13
 *
 * Provides inline helper macros for ARM64 guest code (libsre13.so)
 * to call SREHost_* services via SVC #0x5352 without linking to any host libs.
 */

#ifndef SRE13_HOST_ABI_H
#define SRE13_HOST_ABI_H

#include <stdint.h>
#include <stddef.h>

#define SRE_SVC_NUMBER 0x5352

/* Syscall IDs */
#define SRE_SVC_INSTALL_HOOK     0x01
#define SRE_SVC_REMOVE_HOOK      0x02
#define SRE_SVC_GET_SYMBOL       0x03
#define SRE_SVC_INVALIDATE_RANGE 0x04
#define SRE_SVC_LOG              0x05
#define SRE_SVC_PROFILE_BEGIN    0x06
#define SRE_SVC_PROFILE_END      0x07
#define SRE_SVC_GET_HOST_BLOCK   0x08
#define SRE_SVC_INSTALL_PC_HOOK  0x09
#define SRE_SVC_REMOVE_PC_HOOK   0x0A

typedef void* SREHost_HookHandle;

#ifdef __aarch64__

static inline uint64_t srehost_syscall0(uint64_t id) {
    register uint64_t x8 __asm__("x8") = id;
    register uint64_t x0 __asm__("x0");
    __asm__ volatile("svc #0x5352" : "=r"(x0) : "r"(x8) : "memory");
    return x0;
}

static inline uint64_t srehost_syscall2(uint64_t id, uint64_t a0, uint64_t a1) {
    register uint64_t x8 __asm__("x8") = id;
    register uint64_t x0 __asm__("x0") = a0;
    register uint64_t x1 __asm__("x1") = a1;
    __asm__ volatile("svc #0x5352"
                     : "+r"(x0), "+r"(x1)
                     : "r"(x8)
                     : "memory");
    return x0;
}

static inline uint64_t srehost_syscall4(uint64_t id,
                                        uint64_t a0, uint64_t a1,
                                        uint64_t a2, uint64_t a3) {
    register uint64_t x8 __asm__("x8") = id;
    register uint64_t x0 __asm__("x0") = a0;
    register uint64_t x1 __asm__("x1") = a1;
    register uint64_t x2 __asm__("x2") = a2;
    register uint64_t x3 __asm__("x3") = a3;
    __asm__ volatile("svc #0x5352"
                     : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3)
                     : "r"(x8)
                     : "memory");
    return x0;
}

static inline SREHost_HookHandle srehost_install_hook(
    uint64_t target_vaddr,
    void*    proxy_func,
    void**   orig_func_out,
    int      backend
) {
    return (SREHost_HookHandle)srehost_syscall4(
        SRE_SVC_INSTALL_HOOK,
        target_vaddr,
        (uint64_t)proxy_func,
        (uint64_t)orig_func_out,
        (uint64_t)(unsigned int)backend
    );
}

static inline int srehost_remove_hook(SREHost_HookHandle handle) {
    return (int)srehost_syscall2(SRE_SVC_REMOVE_HOOK, (uint64_t)handle, 0);
}

static inline uint64_t srehost_get_symbol(const char* symbol_name) {
    return srehost_syscall2(SRE_SVC_GET_SYMBOL, (uint64_t)symbol_name, 0);
}

static inline void srehost_invalidate_range(uint64_t vaddr, uint64_t size) {
    srehost_syscall2(SRE_SVC_INVALIDATE_RANGE, vaddr, size);
}

static inline void srehost_log(int level, const char* tag, const char* msg) {
    srehost_syscall4(SRE_SVC_LOG, (uint64_t)(unsigned int)level,
                     (uint64_t)tag, (uint64_t)msg, 0);
}

#else

static inline SREHost_HookHandle srehost_install_hook(uint64_t t, void* p, void** o, int b) {
    (void)t; (void)p; (void)o; (void)b; return 0;
}
static inline int     srehost_remove_hook(SREHost_HookHandle h) { (void)h; return 0; }
static inline uint64_t srehost_get_symbol(const char* s) { (void)s; return 0; }
static inline void    srehost_invalidate_range(uint64_t v, uint64_t sz) { (void)v; (void)sz; }
static inline void    srehost_log(int l, const char* t, const char* m) { (void)l;(void)t;(void)m; }

#endif /* __aarch64__ */

#endif /* SRE13_HOST_ABI_H */
