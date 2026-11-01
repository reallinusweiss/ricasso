/*
 * sre13_recovery.c — C++ exception backstop + panic containment for 1.4.13
 *
 * Mirrors libsre12's sre_effects.c __cxa_throw hook, which is the second
 * layer of the Lua error boundary:
 *
 *   Layer 1 (sre13_lua.c): sre_luaD_throw replaces the engine's luaD_throw
 *   (IDA sub_569BA8, no exported symbol — hooked by offset 0x569BA8). Every
 *   normal Lua error is converted into a guest longjmp to the recovery stack
 *   armed around our lua_call/lua_resume wrappers. The engine's
 *   __cxa_throw(lua_longjmp*) is never reached.
 *
 *   Layer 2 (this file): sre_cxa_throw replaces the engine's __cxa_throw
 *   (0x5FC9E0). Any C++ exception that still escapes — ProgramPanic's int
 *   throw, boost::bad_function_call, exceptions from engine code running
 *   outside our wrapped Lua entry points — is caught here BEFORE the broken
 *   EHABI unwind can reach std::terminate -> abort -> stuck-yield spin
 *   (the boot freeze observed during menu.scene load). With a recovery
 *   point armed we longjmp; without one we abort() with diagnostics so the
 *   host's abort-recovery (configured via g_sre_recovery_* below) can unwind
 *   cleanly instead of silently falling through __cxa_throw's [[noreturn]]
 *   tail.
 *
 * The recovery stack itself lives in sre13_lua.c (sre13_recovery_push/pop);
 * the host resolves g_sre_recovery_depth / g_sre_recovery_stack /
 * g_sre_recovery_stack_bytes here via the same symbols and hands them to
 * configure_recovery_context().
 */

#include "sre13.h"
#include "sre_setjmp.h"

/* Fake-libc headers (base/include) — bridge stubs for stdio/abort. */
#include <stdio.h>
#include <stdlib.h>

/* Recovery stack — defined in sre13_lua.c */
extern sre_recovery_entry g_sre_recovery_stack[];
extern int g_sre_recovery_depth;

/* Original __cxa_throw — filled by the host (main.cpp) with the address of a
 * TrampolineMgr relay cave over the engine's __cxa_throw, created before the
 * sre_cxa_throw redirect hook is installed. Used only when no SRE recovery
 * point is armed, so the engine's own try/catch can process the exception. */
pfn_cxa_throw13 g_original_cxa_throw = 0;

/* Diagnostics — host reads these for the unrecovered-throw poll. */
unsigned long long g_sre_cxa_throw_caller = 0;
int g_sre_cxa_throw_unrecovered = 0;

void sre_cxa_throw(void* thrown_exception, void* tinfo, void(*dest)(void*)) {
    const char* type_name = "unknown";
    if (tinfo) {
        struct abi_type_info {
            void* vtable;
            const char* name;
        } *ti = (struct abi_type_info*)tinfo;
        if (ti->name) type_name = ti->name;
    }

    fprintf(stderr, "[SRE13/cxa_throw] Exception of type '%s' thrown! recovery_depth=%d\n",
            type_name, g_sre_recovery_depth);

    if (g_sre_recovery_depth > 0) {
        int target = g_sre_recovery_depth - 1;

        /* Restore L->errorJmp (at +0xA8 in the 1.4.13 engine lua_State) for
         * this recovery level — same as libsre12. */
        sre_recovery_entry* entry = &g_sre_recovery_stack[target];
        if (entry->lua_state) {
            void** ejp = (void**)((char*)entry->lua_state + LUA_ERRORJMP_OFFSET);
            *ejp = entry->saved_errorJmp;
        }

        /* Invoke the exception object destructor before discarding — the ABI
         * requires this; skipping it leaks the block from
         * __cxa_allocate_exception. Do this BEFORE longjmp. */
        if (dest && thrown_exception) dest(thrown_exception);

        /* The setjmp handler pops the recovery entry. */
        sre_longjmp(entry->buf, 1);
        /* never reaches here */
    }

    /* No recovery point — capture caller info for diagnostics. */
    unsigned long long lr;
    asm volatile("mov %0, x30" : "=r"(lr));
    g_sre_cxa_throw_caller = lr;
    g_sre_cxa_throw_unrecovered++;

    /* __cxa_throw is [[noreturn]] — silently returning would fall into the
     * dead code after the engine's BL __cxa_throw (often a vtable dispatch
     * through a zeroed register -> Dynarmic fault -> silent freeze). Call
     * abort(): with the recovery-context globals exported below, the host's
     * abort-recovery unwinds via saved guest SP/LR to the frame loop. */
    fprintf(stderr,
            "[SRE13/cxa_throw] No recovery point for exception '%s' (caller=0x%llx, "
            "unrecovered=%d) — calling abort() for SRE recovery\n",
            type_name, lr, g_sre_cxa_throw_unrecovered);
    fflush(stderr);
    abort();
}

/* ========== ProgramPanic hook ==========
 * Original: Caver::ProgramPanic at 0x544C44 (dynsym-verified). This is the
 * lua_atpanic handler the engine installs on ProgramState's lua_State. It is
 * called by luaD_throw's path B (errorJmp == NULL — error outside any
 * protected call). The original does __cxa_allocate_exception(4) +
 * __cxa_throw(int) which escapes to terminate when no recovery is armed.
 * Replacement: trigger the SRE recovery if armed, else return 0 (the engine
 * treats a return as \"panic handled\"; luaD_throw then calls exit(1), which
 * the host's JNI bridge can intercept — strictly better than a C++ throw
 * through the broken unwinder). */
int g_sre13_panic_count = 0;

int sre_ProgramPanic(void* L) {
    (void)L;
    g_sre13_panic_count++;

    if (g_sre_recovery_depth > 0) {
        /* Trigger recovery — this does NOT return. */
        sre_cxa_throw((void*)0, (void*)0, (void(*)(void*))0);
        /* unreachable */
    }

    /* No recovery available — return 0; the engine's luaD_throw path B then
     * calls exit(1) (intercepted host-side) instead of throwing. */
    return 0;
}