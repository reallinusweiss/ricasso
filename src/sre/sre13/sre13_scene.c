/*
 * sre13_scene.c — Scene loading & level transition safety hooks for Swordigo 1.4.13
 */

#include "sre13.h"

/* Scene loading state */
volatile int g_sre_scene_loading = 0;
volatile int g_sre_audio_suppressed_frames = 0;
volatile int g_sre_suppress_hud_frames = 0;

/* Relays filled by TrampolineMgr */
uint64_t g_orig_SceneLoadingView_InitWithGameState = 0;
uint64_t g_orig_SceneLoadingView_Update = 0;
uint64_t g_orig_Scene_FinishLoad = 0;
uint64_t g_orig_SceneObjectGroup_FinishLoad = 0;
uint64_t g_orig_ComponentOutletBase_Connect = 0;
uint64_t g_orig_GameData_Clear = 0;
uint64_t g_orig_Proto_SceneObject_Clear = 0;
/* Death screen: original GameOverViewController::Update (relayed by host) */
uint64_t g_orig_GameOverViewController_Update = 0;

/* SceneLoadingView::InitWithGameState hook */
void sre_SceneLoadingView_InitWithGameState(void* self, void* gameState, void* mapNode) {
    g_sre_scene_loading = 1;
    g_sre_audio_suppressed_frames = 60;
    g_sre_suppress_hud_frames = 60;

    if (g_orig_SceneLoadingView_InitWithGameState) {
        typedef void (*fn_init)(void*, void*, void*);
        ((fn_init)g_orig_SceneLoadingView_InitWithGameState)(self, gameState, mapNode);
    }

    /* Clear scene_loading immediately after init so coroutines can run */
    g_sre_scene_loading = 0;
}

/* SceneLoadingView::Update hook */
void sre_SceneLoadingView_Update(void* self, float dt) {
    if (g_orig_SceneLoadingView_Update) {
        typedef void (*fn_update)(void*, float);
        ((fn_update)g_orig_SceneLoadingView_Update)(self, dt);
    }
}

/* GameOverViewController::Update hook — death-screen freeze fix.
 *
 * v1.4.13 Clang INLINED GameOverViewController::ShowAdMaybe into Update()
 * (IDA decompile of 0x424B34 contains the full ad body; ShowAdMaybe @0x424C38
 * reports zero callers). Hooking only the ShowAdMaybe symbol therefore never
 * intercepts the death timer gate. This hook replicates the engine's exact
 * gate so we can suppress the missing ad SDK and queue the respawn:
 *
 *   IDA 1.4.13 Update():
 *     v3 = this[+0x78] + dt; this[+0x78] = v3;      // elapsed timer
 *     if (!this[+0x50] && v3 > 4.5) {               // one-shot ad flag
 *         this[+0x50] = 1;
 *         OnlineController::SharedController()->vfunc[264/8]("game_over",60,0.4)
 *     }
 *     GUIViewController::Update(this, dt)
 *
 * We raise the same +0x50 flag when the gate trips and queue the controller
 * (g_sre_gameover_respawn_controller). The original then sees the flag set,
 * skips the ad vfunc call, and just runs GUIViewController::Update. The host
 * services the deferred respawn via sre_gameover_respawn_tick after the frame.
 */
void sre_GameOverViewController_Update(void* self, float dt) {
    if (self) {
        volatile uint8_t* fired = (volatile uint8_t*)((char*)self + 0x50);
        float elapsed = *(float*)((char*)self + 0x78);
        if (!*fired && (elapsed + dt) > 4.5f &&
            g_sre_gameover_respawn_controller == 0) {
            *fired = 1;
            g_sre_gameover_respawn_controller = (uint64_t)(uintptr_t)self;
        }
    }

    if (g_orig_GameOverViewController_Update) {
        typedef void (*fn_update)(void*, float);
        ((fn_update)g_orig_GameOverViewController_Update)(self, dt);
    }
}

/* Scene::FinishLoad hook */
void sre_Scene_FinishLoad(void* self) {
    if (g_orig_Scene_FinishLoad) {
        typedef void (*fn_finish)(void*);
        ((fn_finish)g_orig_Scene_FinishLoad)(self);
    }
    g_sre_scene_loading = 0;

    extern volatile int g_sre_scene_shift_active;
    extern char g_sre_scene_shift_target[];
    extern char g_sre_current_scene_name[];
    if (g_sre_scene_shift_active && g_sre_scene_shift_target[0]) {
        int i = 0;
        for (; i < 127 && g_sre_scene_shift_target[i]; ++i)
            g_sre_current_scene_name[i] = g_sre_scene_shift_target[i];
        g_sre_current_scene_name[i] = '\0';
        g_sre_scene_shift_active = 0;
    }
}

/* SceneObjectGroup::FinishLoad hook */
void sre_SceneObjectGroup_FinishLoad(void* self) {
    if (g_orig_SceneObjectGroup_FinishLoad) {
        typedef void (*fn_finish)(void*);
        ((fn_finish)g_orig_SceneObjectGroup_FinishLoad)(self);
    }
}

/* ComponentOutletBase::Connect safety guard */
int sre_ComponentOutletBase_Connect(void* self, void* component) {
    if (!self || !component) return 0;

    uint64_t comp_addr = (uint64_t)component;
    if (comp_addr < 0x10000 || comp_addr >= 0x0000800000000000ULL) return 0;

    uint64_t vtable = *(uint64_t*)comp_addr;
    if (!vtable || (vtable & 7) != 0 || vtable < 0x10000 || vtable >= 0x0000800000000000ULL) {
        return 0;
    }

    if (g_orig_ComponentOutletBase_Connect) {
        typedef int (*fn_connect)(void*, void*);
        return ((fn_connect)g_orig_ComponentOutletBase_Connect)(self, component);
    }
    return 1;
}

/* SceneObject::ComponentWithInterface safety guard (1.4.13 ARM64 offset = 0xD0 / 208) */
uint64_t sre_SceneObject_ComponentWithInterface(void* self, int64_t interface_id) {
    if (!self) return 0;

    /* In 1.4.13 Clang/libc++, components vector is at +0xD0 (begin) and +0xD8 (end) */
    uint64_t* begin = *(uint64_t**)((char*)self + 0xD0);
    uint64_t* end   = *(uint64_t**)((char*)self + 0xD8);

    if (!begin || !end || begin >= end) return 0;

    size_t count = (size_t)(end - begin);
    if (count > 256) return 0;

    for (size_t i = 0; i < count; i++) {
        uint64_t comp_addr = begin[i];
        if (!comp_addr || comp_addr < 0x10000 || comp_addr >= 0x0000800000000000ULL) continue;

        uint64_t vtable = *(uint64_t*)comp_addr;
        if (!vtable || (vtable & 7) != 0 || vtable < 0x10000 || vtable >= 0x0000800000000000ULL) continue;

        uint64_t fn_has_interface = *(uint64_t*)(vtable + 160);
        if (!fn_has_interface || (fn_has_interface & 3) != 0 || fn_has_interface < 0x10000) continue;

        typedef int (*pfn_HasInterface)(uint64_t, int64_t);
        pfn_HasInterface fn = (pfn_HasInterface)fn_has_interface;

        if (fn(comp_addr, interface_id) & 1) {
            return comp_addr;
        }
    }
    return 0;
}

#define MAX_PROTO_COUNT 512

/* GameData::Clear crash guard */
void sre_GameData_Clear(void* self) {
    if (self) {
        char* t = (char*)self;
        int* counts[] = {
            (int*)(t + 24), (int*)(t + 80), (int*)(t + 136),
            (int*)(t + 192), (int*)(t + 248)
        };
        for (int i = 0; i < 5; i++) {
            if (*counts[i] < 0 || *counts[i] > MAX_PROTO_COUNT) {
                *counts[i] = 0;
            }
        }
    }
    if (g_orig_GameData_Clear) {
        typedef void (*fn_clear)(void*);
        ((fn_clear)g_orig_GameData_Clear)(self);
    }
}

/* Proto::SceneObject::Clear crash guard */
void sre_Proto_SceneObject_Clear(void* self) {
    if (self) {
        char* t = (char*)self;
        int* comp_count = (int*)(t + 40);
        if (*comp_count < 0 || *comp_count > MAX_PROTO_COUNT) {
            *comp_count = 0;
        }
    }
    if (g_orig_Proto_SceneObject_Clear) {
        typedef void (*fn_clear)(void*);
        ((fn_clear)g_orig_Proto_SceneObject_Clear)(self);
    }
}
