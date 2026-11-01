/*
 * sre13_init.c — Initialization and hook table for libsre13.so
 */

#include "sre13.h"

uint64_t g_swordigo_base = 0;

/* VFS path globals written by main.cpp */
char g_sre_vfs_data_dir[512] = {0};
char g_sre_vfs_path_assets[512] = {0};
char g_sre_vfs_path_files[512] = {0};
char g_sre_vfs_path_external[512] = {0};
char g_sre_vfs_path_cache[512] = {0};
char g_sre_vfs_mod_name[128] = {0};
int  g_sre_vfs_active = 0;
int  g_sre_vfs_hierarchy_enabled = 0;

/* Hook Table for Swordigo 1.4.13
 * All target_offset values are 0 so they are dynamically resolved by symbol name
 * from libswordigo.so's unstripped .dynsym table!
 */
SreHookEntry sre_hook_table[] = {
    /* Lua error boundary & execution safety */
    { 0, "sre_ProgramState_Execute", 0 },
    { 0, "sre_ProgramState_Resume",  0 },

    /* Frame master tick - CaverShell::Update runs every frame (menu, loading,
     * gameplay) and drives the live Lua console (mailbox drain + wait/yield
     * coroutine resumes). Relay-backed so the original still runs. */
    { 0, "sre13_CaverShell_Update", 0 },

    /* ── Lua error containment (THE boot-freeze fix) ─────────────────────
     * 1.4.13's Lua signals errors with C++ exceptions that the emulator
     * cannot unwind (custom loader never registers .eh_frame) → terminate →
     * abort → abort-recovery mis-unwinds → stuck-yield spin during menu
     * scene load. Hook the throw root + panic + __cxa_throw backstop; the
     * wrappers in sre13_lua.c arm a guest setjmp recovery stack so every
     * Lua error longjmps back instead of escaping. Addresses verified
     * against the 1.4.13 binary + IDA dump:
     *   0x569BA8 = luaD_throw (internal — no dynsym symbol, offset hook)
     *   0x544C44 = _ZN5Caver12ProgramPanicEP9lua_State (nm -D)
     *   0x5FC9E0 = __cxa_throw (nm -D; relay for g_original_cxa_throw is
     *              created by main.cpp before this redirect installs) */
    { 0x569ba8, "sre_luaD_throw" },
    { 0x544c44, "sre_ProgramPanic" },
    { 0x5fc9e0, "sre_cxa_throw" },

    /* ── Load-path protection ────────────────────────────────────────────
     * Caver::Program::LoadIntoState / ProgramState::ExecuteString call
     * luaL_loadbuffer/loadstring directly (never through our pcall
     * wrappers); a parse/OOM error there has no recovery frame. Replace
     * with the stock 5.1 loaders compiled into this library: they return
     * error codes instead of throwing lua_longjmp*. Engine symbols exist in
     * sym_hooks_13 (luaL_loadstring/loadbuffer/loadfile); replacement is
     * the vendored copy with the same name. */
    { 0, "luaL_loadstring" },
    { 0, "luaL_loadbuffer" },
    { 0, "luaL_loadfile" },

    /* Audio system interruption safety */
    { 0, "sre_AudioSystem_EndAudioInterruptionIfNecessary", 0 },

    /* Stack canary drift safety */
    { 0, "sre_stack_chk_fail", 0 },

    /* Game Over & Ad Safety
     * 1.4.13 (Clang) INLINED ShowAdMaybe into GameOverViewController::Update
     * (IDA: ShowAdMaybe callers = 0), so the real death-screen timer gate is
     * inside Update. Hook both; each queues the deferred respawn that the
     * host services by calling GameOverViewDidContinue (see sre13_safety.c). */
    { 0, "sre_GameOverViewController_ShowAdMaybe", 0 },
    { 0, "sre_GameOverViewController_Update", 0 },

    /* Scene Loading & Transition Safety */
    { 0, "sre_SceneLoadingView_InitWithGameState", 0 },
    { 0, "sre_SceneLoadingView_Update", 0 },
    /* AnimateIn returns a nontrivial C++ object through AAPCS64 X8. A void
     * C wrapper loses that hidden result pointer and corrupts the view. Leave
     * the original method intact; the old hook only forwarded the call. */
    { 0, "sre_Scene_FinishLoad", 0 },
    { 0, "sre_SceneObjectGroup_FinishLoad", 0 },
    { 0, "sre_ComponentOutletBase_Connect", 0 },
    { 0, "sre_SceneObject_ComponentWithInterface", 0 },
    { 0, "sre_GameData_Clear", 0 },
    { 0, "sre_Proto_SceneObject_Clear", 0 },

    /* OpenAL Music & Audio Controls */
    { 0, "sre_PlayMusicWithName", 0 },
    { 0, "sre_MusicPlayer_FadeIn", 0 },
    { 0, "sre_MusicPlayer_FadeOut", 0 },
    { 0, "sre_MusicPlayer_Update", 0 },
    { 0, "sre_AudioSystem_SetMusicVolume", 0 },
    { 0, "sre_MusicPlayer_SetEnabled", 0 },
    { 0, "sre_MusicPlayer_SetSuspended", 0 },

    /* Desktop UI & Input */
    { 0, "sre_StartTextInputWithDelegate", 0 },
    { 0, "sre_StopTextInputWithDelegate", 0 },
    { 0, "sre_textInputTextDidChange", 0 },
    { 0, "sre_textInputDidFinish", 0 },

    /* End of table */
    { 0, NULL, 0 }
};

int32_t sre_hook_count = sizeof(sre_hook_table) / sizeof(sre_hook_table[0]) - 1;

extern void init_hooks(void);

void sre_init(uint64_t swordigo_base, uint64_t sentinel) {
    g_swordigo_base = swordigo_base;
    (void)sentinel;
    init_hooks();
}
