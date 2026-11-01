#include "sre13_scene_shifter.h"
#include "caver/GameViewController.h"
#include "core/stdstring.h"
#define LOG_TAG "SRE13SceneShifter"
#include "core/log.h"
#include "lauxlib.h"
#include <stdio.h>
#include <string.h>

int          g_sre_scene_list_count = 0;
char         g_sre_scene_list[SRE_SCENE_MAX][SRE_SCENE_NAME_LEN] = {{0}};
volatile int g_sre_scene_shift_pending = 0;
char         g_sre_scene_shift_target[SRE_SCENE_NAME_LEN] = {0};
char         g_sre_scene_shift_spawn[SRE_SCENE_NAME_LEN] = {0};
char         g_sre_scene_shift_last_error[256] = {0};
volatile int g_sre_scene_shift_active = 0;
char         g_sre_current_scene_name[SRE_SCENE_NAME_LEN] = {0};

extern uint64_t g_swordigo_base;

#define OFF_CaverShell_globalptr    0x00651700ULL
#define CAVERSHELL_OFF_NAV_CTRL     0x90
#define NAVCTRL_OFF_CURRENT_VC_PX   0x50
#define GVC_VTABLE_OFFSET           0x62a928ULL
#define OFF_GotoLevel               0x0043195cULL

static inline int is_plausible_guest_obj(uint64_t p) {
    return p >= 0x20000000ULL && p < 0xE0000000ULL;
}

void* sre13_get_active_gvc(void) {
    if (!g_swordigo_base) return NULL;

    void** shell_slot = (void**)(g_swordigo_base + OFF_CaverShell_globalptr);
    void*  shell      = *shell_slot;
    if (!shell || !is_plausible_guest_obj((uint64_t)(uintptr_t)shell)) {
        return NULL;
    }

    void** nav_slot = (void**)((char*)shell + CAVERSHELL_OFF_NAV_CTRL);
    void*  nav_ctrl = *nav_slot;
    if (!nav_ctrl || !is_plausible_guest_obj((uint64_t)(uintptr_t)nav_ctrl)) {
        return NULL;
    }

    void** gvc_slot = (void**)((char*)nav_ctrl + NAVCTRL_OFF_CURRENT_VC_PX);
    void*  gvc      = *gvc_slot;
    if (!gvc || !is_plausible_guest_obj((uint64_t)(uintptr_t)gvc)) {
        return NULL;
    }

    /* Verify vtable points to GameViewController */
    if (*(uint64_t*)gvc != (g_swordigo_base + GVC_VTABLE_OFFSET)) {
        return NULL;
    }

    return gvc;
}

static int s_scene_shift_wait_frames = 0;

int sre13_scene_shifter_shift(const char* level, const char* spawn, int forced) {
    (void)forced;
    if (!level || !*level) {
        snprintf(g_sre_scene_shift_last_error, sizeof(g_sre_scene_shift_last_error), "Empty level name");
        return 0;
    }

    void *gvc = sre13_get_active_gvc();
    if (!gvc) {
        snprintf(g_sre_scene_shift_last_error, sizeof(g_sre_scene_shift_last_error),
                 "No active GameViewController");
        g_sre_scene_shift_active = 0;
        return 0;
    }

    String level_str;
    String spawn_str;
    String_create(&level_str, level);
    String_create(&spawn_str, (spawn && *spawn) ? spawn : "start");

    LOGI("Transitioning to '%s' at '%s' via GameViewController=%p",
         level, (spawn && *spawn) ? spawn : "start", gvc);

    g_sre_scene_shift_active = 1;
    s_scene_shift_wait_frames = 0;
    strncpy(g_sre_scene_shift_target, level, SRE_SCENE_NAME_LEN - 1);
    g_sre_scene_shift_target[SRE_SCENE_NAME_LEN - 1] = '\0';

    typedef void (*pfn_GotoLevel)(void* self, const String* level, const String* spawn);
    pfn_GotoLevel fn_goto = (pfn_GotoLevel)(void*)(g_swordigo_base + OFF_GotoLevel);
    fn_goto(gvc, &level_str, &spawn_str);

    String_destroy(&level_str);
    String_destroy(&spawn_str);

    snprintf(g_sre_scene_shift_last_error, sizeof(g_sre_scene_shift_last_error), "OK");
    return 1;
}

static void sre_scene_shifter_poll_complete(void) {
    if (!g_sre_scene_shift_active) return;
    s_scene_shift_wait_frames++;

    if (g_sre_current_scene_name[0] &&
        strcmp(g_sre_current_scene_name, g_sre_scene_shift_target) == 0) {
        g_sre_scene_shift_active = 0;
        s_scene_shift_wait_frames = 0;
        snprintf(g_sre_scene_shift_last_error, sizeof(g_sre_scene_shift_last_error), "OK");
        return;
    }

    if (s_scene_shift_wait_frames > 3600) {
        snprintf(g_sre_scene_shift_last_error, sizeof(g_sre_scene_shift_last_error), "Scene load timed out");
        g_sre_scene_shift_active = 0;
        s_scene_shift_wait_frames = 0;
    }
}

void sre13_scene_shifter_tick(void) {
    sre_scene_shifter_poll_complete();

    if (g_sre_scene_shift_pending != 0) {
        if (g_sre_scene_shift_active) {
            snprintf(g_sre_scene_shift_last_error, sizeof(g_sre_scene_shift_last_error),
                     "A scene load is already active");
            g_sre_scene_shift_pending = 0;
            return;
        }

        int mode = g_sre_scene_shift_pending;
        g_sre_scene_shift_pending = 0;
        sre13_scene_shifter_shift(g_sre_scene_shift_target, g_sre_scene_shift_spawn, mode == 2);
    }
}

void sre_scene_shifter_tick(void) {
    sre13_scene_shifter_tick();
}

void sre_scene_shifter_scan_scenes(void) {
    /* Scanned by host GUI directly into g_sre_scene_list */
}

/* --- Lua API --- */

static int l_shifter_shift(lua_State *L) {
    const char *level = luaL_checkstring(L, 1);
    const char *spawn = (lua_gettop(L) >= 2) ? luaL_optstring(L, 2, "start") : "start";
    int forced = (lua_gettop(L) >= 3) ? lua_toboolean(L, 3) : 0;
    int ok = sre13_scene_shifter_shift(level, spawn, forced);
    lua_pushboolean(L, ok);
    if (!ok) {
        lua_pushstring(L, g_sre_scene_shift_last_error);
        return 2;
    }
    return 1;
}

static int l_shifter_get_current_scene(lua_State *L) {
    lua_pushstring(L, g_sre_current_scene_name);
    return 1;
}

static int l_shifter_get_scenes(lua_State *L) {
    lua_newtable(L);
    for (int i = 0; i < g_sre_scene_list_count; i++) {
        lua_pushstring(L, g_sre_scene_list[i]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

static int l_shifter_is_busy(lua_State *L) {
    lua_pushboolean(L, g_sre_scene_shift_active);
    return 1;
}

static const luaL_Reg s_shifter_methods[] = {
    { "Shift",           l_shifter_shift },
    { "shift",           l_shifter_shift },
    { "GotoLevel",       l_shifter_shift },
    { "gotoLevel",       l_shifter_shift },
    { "GetCurrentScene", l_shifter_get_current_scene },
    { "getCurrentScene", l_shifter_get_current_scene },
    { "GetScenes",       l_shifter_get_scenes },
    { "getScenes",       l_shifter_get_scenes },
    { "IsBusy",          l_shifter_is_busy },
    { "isBusy",          l_shifter_is_busy },
    { NULL, NULL }
};

void sre13_register_scene_shifter_lua(lua_State *L) {
    if (!L) return;
    int top = lua_gettop(L);

    lua_getglobal(L, "SceneShifter");
    int exists = !lua_isnil(L, -1);
    lua_pop(L, 1);
    if (exists) {
        lua_settop(L, top);
        return;
    }

    luaL_register(L, "SceneShifter", s_shifter_methods);
    /* Alias Shifter to SceneShifter */
    lua_getglobal(L, "SceneShifter");
    lua_setglobal(L, "Shifter");

    /* Ensure stack top is completely restored to what it was */
    lua_settop(L, top);
}
