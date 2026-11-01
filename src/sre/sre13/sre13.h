/*
 * sre13.h — Swordigo Runtime Engine for ABI 1.4.13
 * 
 * ARM64 shared library (libsre13.so) providing modern symbol-driven hooks,
 * Lua error boundary recovery, and runtime stability for Swordigo 1.4.13.
 */

#ifndef SRE13_H
#define SRE13_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

/* Freestanding types */
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long      uint64_t;
typedef signed int         int32_t;
typedef signed long        int64_t;

struct lua_State;   /* forward — Lua API hooks take the engine's lua_State */

/* Hook table entry */
typedef struct {
    uint64_t    target_offset;  /* 0 if resolved dynamically via symbol */
    const char* symbol_name;    /* symbol name in libsre13.so */
    uint64_t    orig_func;      /* relay stub address */
} SreHookEntry;

/* Forward declarations */
void sre_init(uint64_t swordigo_base, uint64_t sentinel);
void sre_init_lua(uint64_t* funcs);

/* Guest base of libswordigo.so (set by sre_init; used by ffi.base/at). */
extern uint64_t g_swordigo_base;

/* Exported hooks — Lua */
void sre_ProgramState_Execute(void* self, int stackIndex);
int  sre_ProgramState_Resume(void* self, int narg);
void sre_ProgramState_ProgramPanic(void* L);
void sre_lua_call_safe(void* L, int nargs, int nresults);
int  sre_lua_resume_safe(void* L, int narg);
void sre_init_lua_ext(uint64_t* funcs);

/* Exported hooks — Lua error boundary (1.4.13)
 *
 * 1.4.13's Lua (like 1.4.12's) signals errors with C++ exceptions:
 * luaD_throw (internal, IDA sub_569BA8) sets errorJmp->status then calls
 * __cxa_throw(lua_longjmp*). The emulator cannot run the engine's EHABI
 * unwind (custom ELF loader never registers .eh_frame with the host
 * linker), so the exception escapes to std::terminate -> abort -> the
 * abort-recovery longjmps into garbage -> stuck-yield spin at boot.
 *
 * Fix (mirrors libsre12): hook the throw root by offset (luaD_throw has no
 * exported symbol in 1.4.13), hook ProgramPanic, hook __cxa_throw as the
 * backstop, and arm a guest setjmp recovery stack around every Lua entry
 * point we wrap. A Lua error then longjmps back to our wrapper instead of
 * reaching the broken C++ unwinder.
 *
 * Verified 1.4.13 engine lua_State layout (IDA): status@0x0A,
 * errorJmp@0xA8 (luaD_throw reads a1+168), nCcalls@0x60 — identical to
 * the 1.4.12 layout used by libsre12, so LUA_ERRORJMP_OFFSET (0xA8)
 * applies here too.
 */
void sre_luaD_throw(void* L, int errcode);
int  sre_ProgramPanic(void* L);
void sre_cxa_throw(void* thrown_exception, void* tinfo, void(*dest)(void*));
typedef void (*pfn_cxa_throw13)(void*, void*, void(*)(void*));
extern pfn_cxa_throw13 g_original_cxa_throw;
extern unsigned long long g_sre_cxa_throw_caller;
extern int g_sre_cxa_throw_unrecovered;

/* Recovery stack — shared between sre13_lua.c wrappers (push/pop + setjmp)
 * and sre13_recovery.c (sre_cxa_throw / sre_luaD_throw longjmp targets).
 * Also consumed by the host: main.cpp resolves g_sre_recovery_depth /
 * g_sre_recovery_stack / g_sre_recovery_stack_bytes and hands them to
 * configure_recovery_context() so Dynarmic's abort-recovery can unwinding
 * through saved guest SP/LR instead of mis-landing. */
int  sre13_recovery_push(void* L);
void sre13_recovery_pop(int depth);

/* Host-visible Lua error diagnostics (read via get_symbol_vaddr) */
extern volatile int  g_sre13_lua_error_count;
extern char          g_sre13_last_lua_error[256];

/* Exported hooks — Safety & System */
void* sre_AudioSystem_EndAudioInterruptionIfNecessary(void* self);
void sre_stack_chk_fail(void);
void sre_GameOverViewController_ShowAdMaybe(void* self);

/* Exported hooks — Death screen / respawn (no ad SDK on desktop)
 *
 * 1.4.13 (Clang) inlines the body of ShowAdMaybe into
 * GameOverViewController::Update — IDA shows ShowAdMaybe has ZERO callers.
 * So we hook BOTH: the exported ShowAdMaybe symbol (direct paths) and
 * GameOverViewController::Update (the actual runtime timer gate). Each one
 * raises the one-shot flag at this+0x50 and queues the controller for a
 * deferred respawn; the host calls sre_gameover_respawn_tick() after
 * updateApplication returns, which invokes GameOverViewDidContinue.
 */
void sre_GameOverViewController_Update(void* self, float dt);
void sre_gameover_respawn_tick(void);
typedef void (*pfn_GameOverVC_DidContinue)(void* self, void* view);
extern pfn_GameOverVC_DidContinue g_sre_GameOverVC_DidContinue;
extern volatile uint64_t g_sre_gameover_respawn_controller;

/* Exported hooks — Scene Loading & Transition */
void sre_SceneLoadingView_InitWithGameState(void* self, void* gameState, void* mapNode);
void sre_SceneLoadingView_Update(void* self, float dt);
void sre_Scene_FinishLoad(void* self);
void sre_SceneObjectGroup_FinishLoad(void* self);
int  sre_ComponentOutletBase_Connect(void* self, void* component);
uint64_t sre_SceneObject_ComponentWithInterface(void* self, int64_t interface_id);
void sre_GameData_Clear(void* self);
void sre_Proto_SceneObject_Clear(void* self);

/* Exported hooks — Audio & Music */
void sre_PlayMusicWithName(void* self, const void* name_str, int restart);
void sre_MusicPlayer_FadeIn(void* self, float duration);
void sre_MusicPlayer_FadeOut(void* self, float duration);
void sre_MusicPlayer_Update(void* self, float dt);
void sre_AudioSystem_SetMusicVolume(void* self, float vol);
void sre_MusicPlayer_SetEnabled(void* self, int enabled);
void sre_MusicPlayer_SetSuspended(void* self, int suspended);

/* Exported UI & Input */
void sre_StartTextInputWithDelegate(void* delegate, void* text);
void sre_StopTextInputWithDelegate(void* delegate);
void sre_textInputTextDidChange(void* env, void* cls, void* jstr);
void sre_textInputDidFinish(void* env, void* cls);

/* Host Camera Override & Controls */
extern int   g_sre_cam_active;
extern float g_sre_cam_off_x;
extern float g_sre_cam_off_y;
extern float g_sre_cam_off_z;
extern float g_sre_cam_aspect;
extern int   g_sre_cam_pov_mode;
extern float g_sre_cam_pov_facing;

/* Mod System */
void sre13_set_active_mod(const char *mod_id);
const char* sre13_get_active_mod(void);

/* ProgramState time-scale registry (lawncher 32patch.c parity, portable half).
 * The host resolves these via get_symbol_vaddr to pause/dampen an individual
 * running script by its ProgramState pointer. ps_remove is called from the
 * ~ProgramState hook automatically. */
void ps_setTimeScaleEnabled(void *ps, bool enabled);
bool ps_isTimeScaleEnabled(void *ps);
void ps_remove(void *ps);

/* Mini game-API host-cooperative globals (lawncher mini.c parity):
 * SetCoinLimit stores the clamp value here for the host to apply (mirrors
 * sre12's host-cooperative coin limit — rewriting engine .text from the guest
 * is unsafe under the emulator); ToggleDebug mirrors the sre12 debug flag. */
extern volatile int g_sre13_coin_limit;
extern volatile int g_sre13_debug_active;

/* Optional libsre-extras.so addon (ABI-aware, served to both 1.4.12/1.4.13) */
extern void* g_sre_extras_miniLL_open_memory;  /* set by the host to the
                                                  extras' miniLL_open_memory
                                                  guest vaddr (0 => stubs) */
void sre13_extras_inject(struct lua_State* L);  /* Mini + ffi stub/real merge */
void sre13_ensure_injected(struct lua_State* L); /* Idempotent module injector for live states */

/* Scene Shifter */
int  sre13_scene_shifter_shift(const char* level, const char* spawn, int forced);
void sre13_scene_shifter_tick(void);

/* Lua Console */
struct lua_State;
void sre13_console_tick(struct lua_State* L);

/* Host-triggered developer overlays & debug toggles (hooks/GameViewController.c) */
extern volatile int g_sre_request_toggle_debug_info;
extern volatile int g_sre_request_toggle_collision_shapes;
extern volatile int g_sre_request_toggle_combat_wireframe;

#endif /* SRE13_H */

