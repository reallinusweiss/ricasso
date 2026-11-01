#include "sre13_console.h"
#include "sre13.h"
#include "sre13_scene_shifter.h"
#include "ui/sre13_button_controller.h"
#include "caver/GameSceneController.h"
#include "caver/GameViewController.h"
#include "caver/CameraController.h"
#include "caver/Scene.h"
#include "caver/CharacterState.h"
#define LOG_TAG "SRE13Console"
#include "core/log.h"
#include "lauxlib.h"
#include "lstate.h"     /* lua_State internals: CallInfo/StkId VM save/restore */
#include "lfunc.h"      /* luaF_close for recovery unwind */
#include "sre_setjmp.h"
#include <stdio.h>
#include <string.h>

/* Engine Lua API pointers — shared with sre13_lua.c (filled by sre_init_lua) */
typedef const char* (*pfn_console_lua_tolstring)(lua_State* L, int idx, size_t* len);
extern pfn_console_lua_tolstring   g_lua_tolstring;
extern void sre13_arm_timeout(lua_State* L);
extern void sre13_disarm_timeout(lua_State* L);

char         g_lua_console_buf[CONSOLE_BUF_SIZE] = {0};
char         g_lua_console_result[CONSOLE_BUF_SIZE] = {0};
char         g_lua_console_print_buf[CONSOLE_PRINT_SIZE] = {0};
volatile int g_lua_console_pending = 0;
volatile int g_lua_console_status = 0;

/* Host-visible diagnostics (SwordfareGUI / main.cpp read these) */
volatile uint64_t g_sre_console_runs = 0;
volatile uint64_t g_sre_last_lua_state = 0;

/* ── Live-console coroutine state ────────────────────────────────────────── */
static lua_State* g_sre_root = NULL;       /* newest live game lua_State */
static lua_State* g_sre_thread = NULL;      /* active console coroutine */
static int        g_sre_thread_ref = LUA_NOREF;  /* registry ref → keep GC alive */
static int        g_sre_wait_mode  = 0;      /* 0=none 1=frames 2=seconds */
static int        g_sre_wait_frames = 0;
static float      g_sre_wait_deadline = 0.0f;
static float      g_sre_console_clock = 0.0f;

/* ProgramState & ProgramTable layout for ARM64 (1.4.13) */
typedef struct CaverProgramTable {
    lua_State *L;
    int table_index;
    int pad;
} CaverProgramTable;

typedef struct CaverProgramState {
    lua_State *L;                      /* +0x00: lua_State* */
    struct CaverProgramState *parent;  /* +0x08: parent ProgramState* */
    void *child_list[2];               /* +0x10, +0x18: circular child list head */
    void *pad20[2];                    /* +0x20, +0x28 */
    CaverProgramTable globals;         /* +0x30 (48): ProgramTable { L, -10002 } */
    CaverProgramTable registry;        /* +0x40 (64): ProgramTable { L, -10000 } */
    void *pad50;                       /* +0x50 (80) */
    uint8_t b88;                       /* +0x58 (88) */
    uint8_t b89;                       /* +0x59 (89) = 1 */
    uint8_t b90;                       /* +0x5A (90) = 0 */
    uint8_t b91;                       /* +0x5B (91) = 0 */
    float timeScale;                   /* +0x5C (92) = 1.0f */
} CaverProgramState;

static CaverProgramState g_console_program_state;
static void *g_sre_root_ps = NULL;

static void sre13_console_cleanup(void);
static void sre13_console_resume(void);

static void sre13_strcopy(char *dst, const char *src, size_t maxlen) {
    if (!dst || !src || maxlen == 0) return;
    size_t i = 0;
    while (src[i] && i < maxlen - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

/* Intercept print(...) calls during console command */
static int sre13_console_print(lua_State *L) {
    int n = lua_gettop(L);
    int pos = 0;
    while (pos < CONSOLE_PRINT_SIZE - 1 && g_lua_console_print_buf[pos]) pos++;

    for (int i = 1; i <= n; i++) {
        if (i > 1 && pos < CONSOLE_PRINT_SIZE - 2) {
            g_lua_console_print_buf[pos++] = '\t';
        }
        const char *s = lua_tostring(L, i);
        if (!s) {
            int t = lua_type(L, i);
            s = lua_typename(L, t);
        }
        for (int j = 0; s[j] && pos < CONSOLE_PRINT_SIZE - 2; j++) {
            g_lua_console_print_buf[pos++] = s[j];
        }
    }
    if (pos < CONSOLE_PRINT_SIZE - 1) {
        g_lua_console_print_buf[pos++] = '\n';
    }
    g_lua_console_print_buf[pos] = '\0';
    return 0;
}

static int l_console_help(lua_State *L) {
    const char *help_str =
        "[SRE13 Console Help]\n"
        "  Game             - Native Game library (e.g. Game.ShowNotification(\"text\", [dur]))\n"
        "  Scene            - Native Scene library\n"
        "  Character        - Native Character library\n"
        "  Program          - Native Program library (Wait, PlayEffect, etc.)\n"
        "  hero             - active SceneObject for hero\n"
        "  camera           - active Camera instance\n"
        "  cameraController - active CameraController\n"
        "  scene            - active Scene instance\n"
        "  gvc / gameController - GameViewController instance\n"
        "  gsc              - GameSceneController instance\n"
        "  characterState   - CharacterState instance\n"
        "  SceneShifter     - Shift(scene, spawn) / GetScenes()\n"
        "  ButtonController - UI buttons and overlays\n"
        "  wait(seconds)    - yield console coroutine for duration\n"
        "  waitFrames(n)    - yield console coroutine for n frames\n";
    printf("%s", help_str);
    lua_pushstring(L, help_str);
    return 1;
}

static void sre13_inject_console_globals(lua_State *L) {
    GameSceneController *gsc = gsc_get();
    if (gsc) {
        lua_pushlightuserdata(L, gsc);
        lua_setglobal(L, "gsc");
        if (gsc->hero) {
            lua_pushlightuserdata(L, gsc->hero);
            lua_setglobal(L, "hero");
        }
        if (gsc->CharacterState) {
            lua_pushlightuserdata(L, gsc->CharacterState);
            lua_setglobal(L, "characterState");
        }
    }

    GameViewController *gvc = gvc_get();
    if (!gvc && L) {
        lua_getglobal(L, "gameController");
        if (lua_islightuserdata(L, -1)) {
            gvc = (GameViewController *)lua_topointer(L, -1);
        }
        lua_pop(L, 1);
    }
    if (gvc) {
        lua_pushlightuserdata(L, gvc);
        lua_setglobal(L, "gvc");
        lua_pushlightuserdata(L, gvc);
        lua_setglobal(L, "gameController");
    }

    CameraController *cc = cameraController_get();
    if (cc) {
        lua_pushlightuserdata(L, cc);
        lua_setglobal(L, "cameraController");
        if (cc->camera) {
            lua_pushlightuserdata(L, cc->camera);
            lua_setglobal(L, "camera");
        }
    }

    Scene *scene = scene_get();
    if (!scene && L) {
        lua_getglobal(L, "scene");
        if (lua_islightuserdata(L, -1)) {
            scene = (Scene *)lua_topointer(L, -1);
        }
        lua_pop(L, 1);
    }
    if (scene) {
        lua_pushlightuserdata(L, scene);
        lua_setglobal(L, "scene");
    }

    lua_pushcfunction(L, l_console_help);
    lua_setglobal(L, "help");
}

static void sre13_serialize_value(lua_State *L, int idx, char *out, int *pos, int maxlen, int depth) {
    if (*pos >= maxlen - 32) return;
    int t = lua_type(L, idx);
    switch (t) {
        case LUA_TNIL:
            *pos += snprintf(out + *pos, maxlen - *pos, "nil");
            break;
        case LUA_TBOOLEAN:
            *pos += snprintf(out + *pos, maxlen - *pos, "%s", lua_toboolean(L, idx) ? "true" : "false");
            break;
        case LUA_TNUMBER:
            *pos += snprintf(out + *pos, maxlen - *pos, "%g", lua_tonumber(L, idx));
            break;
        case LUA_TSTRING:
            *pos += snprintf(out + *pos, maxlen - *pos, "\"%s\"", lua_tostring(L, idx));
            break;
        case LUA_TTABLE: {
            if (depth >= 2) {
                *pos += snprintf(out + *pos, maxlen - *pos, "{...}");
                return;
            }
            *pos += snprintf(out + *pos, maxlen - *pos, "{ ");
            lua_pushnil(L);
            int count = 0;
            while (lua_next(L, idx < 0 ? idx - 1 : idx) != 0) {
                if (count++ > 0) *pos += snprintf(out + *pos, maxlen - *pos, ", ");
                if (count > 16) {
                    *pos += snprintf(out + *pos, maxlen - *pos, "...");
                    lua_pop(L, 2);
                    break;
                }
                /* key */
                if (lua_type(L, -2) == LUA_TSTRING) {
                    *pos += snprintf(out + *pos, maxlen - *pos, "%s = ", lua_tostring(L, -2));
                }
                /* value */
                sre13_serialize_value(L, -1, out, pos, maxlen, depth + 1);
                lua_pop(L, 1);
            }
            *pos += snprintf(out + *pos, maxlen - *pos, " }");
            break;
        }
        case LUA_TFUNCTION:
            *pos += snprintf(out + *pos, maxlen - *pos, "<function:%p>", lua_topointer(L, idx));
            break;
        case LUA_TLIGHTUSERDATA:
        case LUA_TUSERDATA:
            *pos += snprintf(out + *pos, maxlen - *pos, "<userdata:%p>", lua_topointer(L, idx));
            break;
        default:
            *pos += snprintf(out + *pos, maxlen - *pos, "<%s>", lua_typename(L, t));
            break;
    }
}

static void sre13_collect_returns(lua_State *L, int start_idx) {
    int total = lua_gettop(L);
    int pos = 0;
    g_lua_console_result[0] = '\0';

    for (int i = start_idx + 1; i <= total; i++) {
        if (i > start_idx + 1 && pos < CONSOLE_BUF_SIZE - 2) {
            g_lua_console_result[pos++] = '\t';
        }
        sre13_serialize_value(L, i, g_lua_console_result, &pos, CONSOLE_BUF_SIZE, 0);
    }
    g_lua_console_result[pos] = '\0';
}
/* ============================================================================
 * Live console - per-frame service on a dedicated coroutine thread
 * ============================================================================
 * The 1.4.13 console used to be serviced inside sre_ProgramState_Execute /
 * sre_ProgramState_Resume: it only ran while the game happened to execute a
 * Lua program, ran synchronously inside the game's coroutine context, so no
 * yield/wait was possible and it stalled wheneverthe game wasn't driving scripts
 * (menu, loading, cutscenes). Nowthe console is attached to the frame
 * master tick CaverShell::Update (0x323944, hooked in hooks/CaverShell.c). the
 * exact per-frame entry the host's updateApplication() dispatches through every
 * frame in every game state. Scripts run on their own coroutine created on
 * the live game lua_State (lua_newthread, so they can wait/yield without
 * touching any of the game's own coroutine stacks - a true "live" console.

/* wait / waitFrames / yield - yieldthe CONSOLE thread only */
static int sre13_lua_wait(lua_State *L) {
    if (L != g_sre_thread) return 0;
    double t = luaL_checknumber(L, 1);
    if (t < 0.0) t =   0.0;
    g_sre_wait_mode =   2;
    g_sre_wait_deadline = g_sre_console_clock + (float)t;
    return lua_yield(L, 0);
}

static int sre13_lua_wait_frames(lua_State *L) {
    if (L != g_sre_thread) return   0;
    int n = (int)luaL_checknumber(L, 1);
    if (n < 1) n =   1;
    g_sre_wait_mode =   1;
    g_sre_wait_frames = n;
    return lua_yield(L, 0);
}

static int sre13_lua_yield_fn(lua_State *L) {
    if (L != g_sre_thread) return   0;
    g_sre_wait_mode =   0;
    return lua_yield(L, 0);
}

/* Game-readiness probe (mirrors sre12's sre_state_is_valid_game_state):
 * only the main game state carries the Scene API; child/UI states must not
 * hijack the console target. tt@8 == LUA_TTHREAD, l_G@32 must be a plausible
 * guest pointer (vendored lua_State layout, verified against the engine). */
static int sre13_console_state_is_game_ready(lua_State *L) {
    if (!L) return 0;
    if (((unsigned char *)L)[8] != LUA_TTHREAD) return 0;
    uint64_t lg = (uint64_t)*(void **)((char *)L + 32);
    if (lg < 0x1000000ULL || lg >= 0x80000000ULL) return 0;
    int top = lua_gettop(L);
    lua_getglobal(L, "Scene");
    int t = lua_type(L, -1);
    lua_settop(L, top);
    return t == LUA_TTABLE;
}

/* Capture the live game lua_State (called from sre13_lua.c wrappers) */
void sre13_console_set_root(lua_State *L) {
    if (!L) return;
    g_sre_last_lua_state = (uint64_t)(uintptr_t)L;
    if (g_sre_root) {
        /* Same global VM (thread/child of the state we already own)? No-op:
         * this keeps the console coroutine alive across object-program churn. */
        void *root_g = *(void **)((char *)g_sre_root + 32);
        void *new_g  = *(void **)((char *)L + 32);
        if (root_g == new_g) return;
    }
    /* Different VM: adopt it only if it is a game-ready main state */
    if (!sre13_console_state_is_game_ready(L)) {
        static int s_rejects = 0;
        if (s_rejects < 3) {
            s_rejects++;
            LOGI("Console root rejected (not game-ready)");
        }
        return;
    }
    sre13_console_cleanup();          /* drop thread/print state of the OLD state */
    g_sre_root = L;
    sre13_ensure_injected(L);
    LOGI("Console root adopted");
    g_sre_console_clock = 0.0f;
}

void sre13_console_set_root_and_ps(lua_State *L, void *ps) {
    if (ps) {
        while (ps && *(void **)((char *)ps + 8)) {
            ps = *(void **)((char *)ps + 8);
        }
        g_sre_root_ps = ps;
    }
    sre13_console_set_root(L);
}

static void *sre13_resolve_root_program_state(lua_State *root) {
    if (g_sre_root_ps) return g_sre_root_ps;
    if (root) {
        lua_pushlightuserdata(root, root);
        lua_gettable(root, LUA_GLOBALSINDEX);
        if (lua_islightuserdata(root, -1)) {
            g_sre_root_ps = (void *)lua_touserdata(root, -1);
        }
        lua_pop(root, 1);
    }
    if (!g_sre_root_ps) {
        Scene *sc = scene_get();
        if (sc) {
            g_sre_root_ps = (char *)sc + 40;
        }
    }
    return g_sre_root_ps;
}

static void sre13_setup_console_program_state(lua_State *T, lua_State *root) {
    void *root_ps = sre13_resolve_root_program_state(root);

    memset(&g_console_program_state, 0, sizeof(g_console_program_state));
    g_console_program_state.L = T;
    g_console_program_state.parent = (CaverProgramState *)root_ps;
    g_console_program_state.child_list[0] = (char *)&g_console_program_state + 16;
    g_console_program_state.child_list[1] = (char *)&g_console_program_state + 16;

    /* globals ProgramTable: L = T, table_index = -10002 */
    g_console_program_state.globals.L = T;
    g_console_program_state.globals.table_index = -10002;

    /* registry ProgramTable: L = T, table_index = -10000 */
    g_console_program_state.registry.L = T;
    g_console_program_state.registry.table_index = -10000;

    g_console_program_state.b89 = 1;
    g_console_program_state.timeScale = 1.0f;

    /* 1. Map (lightuserdata) T -> &g_console_program_state in T's globals table (env)
     * so that Caver::ProgramState::FromLuaState(T) succeeds! */
    lua_pushlightuserdata(T, T);
    lua_pushlightuserdata(T, &g_console_program_state);
    lua_rawset(T, LUA_GLOBALSINDEX);

    /* 2. Map (lightuserdata) T -> &g_console_program_state in root's globals table */
    if (root) {
        lua_pushlightuserdata(root, T);
        lua_pushlightuserdata(root, &g_console_program_state);
        lua_rawset(root, LUA_GLOBALSINDEX);

        /* 3. Also map child state in registry as ProgramState constructor does */
        lua_pushlightuserdata(root, &g_console_program_state);
        lua_pushthread(T);
        lua_xmove(T, root, 1);
        lua_rawset(root, LUA_REGISTRYINDEX);
    }

    /* 4. Ensure engine objects are directly accessible in T's environment */
    GameViewController *gvc = gvc_get();
    if (!gvc && root) {
        lua_getglobal(root, "gameController");
        if (lua_islightuserdata(root, -1)) {
            gvc = (GameViewController *)lua_topointer(root, -1);
        }
        lua_pop(root, 1);
    }
    if (gvc) {
        lua_pushlightuserdata(T, gvc);
        lua_setglobal(T, "gameController");
    }

    Scene *sc = scene_get();
    if (!sc && root) {
        lua_getglobal(root, "scene");
        if (lua_islightuserdata(root, -1)) {
            sc = (Scene *)lua_topointer(root, -1);
        }
        lua_pop(root, 1);
    }
    if (sc) {
        lua_pushlightuserdata(T, sc);
        lua_setglobal(T, "scene");
    }
}

/* Names never merged back into the game's _G (console-owned or refreshed
 * per command) */
static const char *s_env_skip_keys[] = {
    "print", "wait", "waitFrames", "yield", "Console", "Program", "help",
    "gsc", "hero", "characterState", "gvc", "gameController", "cameraController", "camera",
    "scene", "ButtonController", "Button", "OverlayController",
    "SceneShifter", "Shifter", NULL
};

/* Copy the console env's own keys back into the game's _G so user
 * assignments persist across commands (env reads fall through to _G via
 * __index; writes land in env - we merge them home when a command ends). */
static void sre13_console_merge_globals(lua_State *T, lua_State *root) {
    if (!T || !root) return;
    int top_t = lua_gettop(T);
    lua_pushvalue(T, LUA_GLOBALSINDEX);            /* env */
    if (!lua_istable(T, -1)) { lua_settop(T, top_t); return; }
    lua_pushnil(T);
    while (lua_next(T, -2) != 0) {
        /* stack: env, key, value */
        if (lua_type(T, -2) == LUA_TSTRING) {
            const char *k = lua_tostring(T, -2);
            int skip = 0;
            int i;
            for (i = 0; s_env_skip_keys[i]; i++) {
                if (strcmp(k, s_env_skip_keys[i]) == 0) { skip = 1; break; }
            }
            if (!skip) {
                int top_r = lua_gettop(root);
                lua_pushvalue(root, LUA_GLOBALSINDEX);     /* game _G */
                lua_pushlstring(root, k, strlen(k));       /* key */
                lua_pushvalue(T, -1);                      /* dup value */
                lua_xmove(T, root, 1);                     /* move to root */
                lua_rawset(root, -3);                      /* _G[k] = v */
                lua_settop(root, top_r);
            }
        }
        lua_pop(T, 1);   /* pop value, keep key */
    }
    lua_settop(T, top_t);
}

/* Tear down the active console coroutine. Merges the env's user globals
 * back into the game's _G so assignments persist across commands. Lua
 * calls are only issued while the root still probes alive; on a dead
 * state (scene change) we bare-reset the pointers instead. */
static void sre13_console_cleanup(void) {
    lua_State *root = g_sre_root;
    lua_State *T = g_sre_thread;
    int alive = root && T && sre13_console_state_is_game_ready(root);
    if (alive) {
        sre13_console_merge_globals(T, root);
        lua_pushlightuserdata(root, T);
        lua_pushnil(root);
        lua_rawset(root, LUA_GLOBALSINDEX);
        lua_pushlightuserdata(root, &g_console_program_state);
        lua_pushnil(root);
        lua_rawset(root, LUA_REGISTRYINDEX);
    }
    if (g_sre_thread_ref != LUA_NOREF && alive) {
        luaL_unref(root, LUA_REGISTRYINDEX, g_sre_thread_ref);
    }
    g_sre_thread_ref = LUA_NOREF;
    g_sre_thread = NULL;
    g_sre_wait_mode = 0;
    g_sre_wait_frames = 0;
    g_sre_wait_deadline = 0.0f;
}
/* Run the console chunk to completion or until it yields */
static void sre13_console_resume(void) {
    lua_State *T = g_sre_thread;
    if (!T) return;


    CallInfo* saved_ci = T->ci;
    StkId saved_top = T->top;
    StkId saved_base = T->base;
    unsigned short saved_nCcalls = T->nCcalls;

    int my_depth = sre13_recovery_push(T);
    if (my_depth >=   0 && sre_setjmp(g_sre_recovery_stack[my_depth].buf) !=   0) {
        sre13_recovery_pop(my_depth);
        g_sre13_lua_error_count++;
        if (g_lua_tolstring) {
            const char *err = g_lua_tolstring(T, -1, NULL);
            if (err && *err) sre13_strcopy(g_lua_console_result, err, CONSOLE_BUF_SIZE);
        }
        if (!g_lua_console_result[0])
            sre13_strcopy(g_lua_console_result, "Exception intercepted in console script", CONSOLE_BUF_SIZE);
        if (saved_ci && saved_top) {
            luaF_close(T, saved_top);
            T->ci = saved_ci;
            T->top = saved_top;
            T->base = saved_base;
            T->nCcalls = saved_nCcalls;
        }
        g_lua_console_status =   2;
        sre13_console_cleanup();
        return;
    }

    int res;
    sre13_arm_timeout(T);
    /* VENDORED lua_resume: longjmp-consistent with the vendored lua_yield. */
    /* (The engine lua_resume is C++-exception based: its lua_longjmp has no
    /* jmp_buf, so a vendored yield would longjmp into garbage. Engine-code
    /* errors still hit the sre_luaD_throw / sre_cxa_throw hooks and land in
    /* the SRE recovery frame armed above.) */
    res = lua_resume(T, 0);
    sre13_disarm_timeout(T);
    if (my_depth >=   0) sre13_recovery_pop(my_depth);

    if (res ==   1 /* LUA_YIELD */) {
        return;
    }

    g_sre_wait_mode =   0;

    if (res !=   0) {
        const char *err = g_lua_tolstring ? g_lua_tolstring(T, -1, NULL) : NULL;
        sre13_strcopy(g_lua_console_result, err ? err : "runtime error", CONSOLE_BUF_SIZE);
        g_lua_console_status =   2;
    } else {
        sre13_collect_returns(T,   0);

        if (!g_lua_console_result[0] && g_lua_console_print_buf[0]) {
            size_t len = strlen(g_lua_console_print_buf);
            if (len >   0 && g_lua_console_print_buf[len -   1] == '\n')
                g_lua_console_print_buf[len -   1] = '\0';
            sre13_strcopy(g_lua_console_result, g_lua_console_print_buf, CONSOLE_BUF_SIZE);
        } else if (g_lua_console_print_buf[0] && g_lua_console_result[0]) {
            char merged[CONSOLE_BUF_SIZE];
            snprintf(merged, sizeof(merged), "%s%s", g_lua_console_print_buf, g_lua_console_result);
            sre13_strcopy(g_lua_console_result, merged, CONSOLE_BUF_SIZE);
        }
        if (!g_lua_console_result[0])
            sre13_strcopy(g_lua_console_result, "OK", CONSOLE_BUF_SIZE);
        g_lua_console_status =   1;
    }

    sre13_console_cleanup();
}
/* Start a new console command on a fresh coroutine */
static void sre13_console_start_command(void) {
    lua_State *root = g_sre_root;
    if (!root) return;
    sre13_ensure_injected(root);

    int root_top = lua_gettop(root);
    g_lua_console_result[0] = 0;
    g_lua_console_print_buf[0] = 0;

    /* A fresh coroutine per command cancels any still-yielded script */
    sre13_console_cleanup();

    /* Create the console coroutine on the live game state */
    lua_State *T = lua_newthread(root);              /* pushed on root */
    if (!T) {
        sre13_strcopy(g_lua_console_result, "Failed to create console coroutine", CONSOLE_BUF_SIZE);
        g_lua_console_status = 2;
        lua_settop(root, root_top);
        return;
    }
    g_sre_thread = T;
    g_sre_thread_ref = luaL_ref(root, LUA_REGISTRYINDEX);   /* pop thread, keep alive */

    /* Per-thread sandbox env: reads fall through to the game's _G via
     * __index, writes land in env (merged back into _G at command end).
     * This isolates the console from the game AND lets us shadow
     * Program.Wait safely - the engine's Wait binding stores through
     * FromLuaState(L) with NO null check and a console coroutine is not in
     * the ProgramTable map, so the real Program.Wait would fault here.
     * Delegating is not an option either: the engine yield is routed by our
     * luaD_throw hook into the SRE recovery stack, so console scripts must
     * yield through the vendored lua_yield on their own thread. */
    lua_newtable(T);                                  /* env */
    lua_newtable(T);                                  /* metatable */
    lua_pushvalue(T, LUA_GLOBALSINDEX);               /* game _G */
    lua_setfield(T, -2, "__index");
    lua_setmetatable(T, -2);                          /* setmetatable(env, mt) */

    lua_pushcfunction(T, sre13_console_print);
    lua_setfield(T, -2, "print");                     /* env-scoped print capture */
    lua_pushcfunction(T, sre13_lua_wait);        lua_setfield(T, -2, "wait");
    lua_pushcfunction(T, sre13_lua_wait_frames); lua_setfield(T, -2, "waitFrames");
    lua_pushcfunction(T, sre13_lua_yield_fn);    lua_setfield(T, -2, "yield");

    lua_newtable(T);                                  /* Console.* namespace */
    lua_pushcfunction(T, sre13_lua_wait);        lua_setfield(T, -2, "Wait");
    lua_pushcfunction(T, sre13_lua_wait_frames); lua_setfield(T, -2, "WaitFrames");
    lua_pushcfunction(T, sre13_lua_yield_fn);    lua_setfield(T, -2, "Yield");
    lua_setfield(T, -2, "Console");

    lua_newtable(T);                                  /* Program shadow */
    lua_pushcfunction(T, sre13_lua_wait);
    lua_setfield(T, -2, "Wait");                      /* Program.Wait(s) -> ours */
    lua_newtable(T);                                  /* shadow metatable */
    lua_getglobal(T, "Program");                      /* game's Program table */
    if (lua_istable(T, -1)) {
        lua_setfield(T, -2, "__index");               /* fall through: Print, PlayEffect */
    } else {
        lua_pop(T, 1);
    }
    lua_setmetatable(T, -2);
    lua_setfield(T, -2, "Program");

    lua_replace(T, LUA_GLOBALSINDEX);                 /* T->l_gt = env */

    /* World globals + SRE Lua modules land in env (no game pollution) */
    sre13_inject_console_globals(T);
    sre13_register_button_controller(T);
    sre13_register_scene_shifter_lua(T);
    sre13_ensure_injected(T);

    /* Connect console thread T to native ProgramState architecture */
    sre13_setup_console_program_state(T, root);

    /* Try expression first: "return <code>", then plain statement */
    lua_settop(T, 0);
    char wrapped[CONSOLE_BUF_SIZE + 16];
    snprintf(wrapped, sizeof(wrapped), "return %s", g_lua_console_buf);
    int r = luaL_loadstring(T, wrapped);
    if (r != 0) {
        lua_pop(T, 1);
        r = luaL_loadstring(T, g_lua_console_buf);
    }
    if (r != 0) {
        const char *err = lua_tostring(T, -1);
        sre13_strcopy(g_lua_console_result, err ? err : "syntax error", CONSOLE_BUF_SIZE);
        g_lua_console_status = 2;
        sre13_console_cleanup();
        lua_settop(root, root_top);
        return;
    }

    /* Bind the chunk to the env (5.1 GETGLOBAL uses the closure's env) */
    lua_pushvalue(T, LUA_GLOBALSINDEX);               /* env */
    lua_setfenv(T, -2);

    g_sre_console_runs++;
    sre13_console_resume();
    lua_settop(root, root_top);
}

/* Per-frame service. Called from the ProgramState hooks (where game Lua
 * runs in valid scene context), potentially several times per frame — the
 * static guard ensures we actually service only ONCE per frame. The guard
 * is reset by sre13_console_frame_guard_reset() from CaverShell::Update
 * (which fires exactly once per frame). Service must happen from the
 * ProgramState context, not the shell tail: game C++ APIs called from
 * console scripts (Camera.*, Scene.*, etc.) require the valid scene/render
 * context that only exists during ProgramState execution. */
static int s_console_serviced_this_frame = 0;
static float s_console_frame_dt = 0.016f;

void sre13_console_set_frame_dt(float dt) {
    if (dt > 0.0f) s_console_frame_dt = dt;
}


void sre13_console_frame_guard_reset(void) {
    s_console_serviced_this_frame = 0;
}

void sre13_console_frame_tick(float dt) {
    (void)dt;
    if (s_console_serviced_this_frame) return;
    s_console_serviced_this_frame = 1;

    g_sre_console_clock += s_console_frame_dt;
    if (!g_sre_root) {
        Scene *sc = scene_get();
        if (sc) {
            void *ps = (char *)sc + 40;
            lua_State *root = *(lua_State **)ps;
            if (root && sre13_console_state_is_game_ready(root)) {
                sre13_console_set_root_and_ps(root, ps);
            }
        }
    }
    if (!g_sre_root) return;
    if (!sre13_console_state_is_game_ready(g_sre_root)) {
        /* Root died (scene transition) - bare-drop and wait for the new
         * state to be captured by the ProgramState wrappers. */
        g_sre_root = NULL;
        g_sre_root_ps = NULL;
        g_sre_thread = NULL;
        g_sre_thread_ref = LUA_NOREF;
        g_sre_wait_mode = 0;
        g_sre_wait_frames = 0;
        g_sre_wait_deadline = 0.0f;
        return;
    }
    static int s_tick_announced = 0;
    if (!s_tick_announced) {
        s_tick_announced = 1;
        LOGI("Console frame tick active (per-frame service running)");
    }

    if (g_sre_console_clock > 100000.0f) g_sre_console_clock =   0.0f;

    if (g_lua_console_pending) {
        g_lua_console_pending = 0;
        sre13_console_start_command();
        return;
    }

    if (!g_sre_thread) return;

    if (g_sre_wait_mode == 1) {
        if (--g_sre_wait_frames > 0) return;
        g_sre_wait_mode =   0;
    } else if (g_sre_wait_mode == 2) {
        if (g_sre_console_clock < g_sre_wait_deadline) return;
        g_sre_wait_mode =   0;
    }

    sre13_console_resume();
}

/* Legacy single-shot service point (kept so existing callers keep working) */
void sre13_console_tick(lua_State *L) {
    sre13_console_set_root(L);
    sre13_console_frame_tick(0.0f);
}
