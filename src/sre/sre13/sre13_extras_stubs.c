/*
 * sre13_extras_stubs.c — stub Lua API for the optional libsre-extras addon.
 *
 * Parity with libsre12 (sre/sre12/sre_extras_stubs.c): when
 * libsre-extras.so is NOT loaded (or not built), SRE13 still exposes the
 * same safe Lua surface — Mini.MemoryAddress, Mini.GetAddress/Dlsym/Malloc/
 * GetComponentAddress, and ffi.call_sig — so mods written against the
 * extras API keep running:
 *   - reads return 0 / nil
 *   - writes are no-ops
 *   - Dlsym / GetComponentAddress / ffi.call_sig return nil (and log once)
 *   - Malloc / GetAddress still work (pure memory ops)
 *
 * When the host DOES load libsre-extras.so, it writes the extras'
 * miniLL_open_memory guest address into g_sre_extras_miniLL_open_memory and
 * sre13_extras_inject() calls the real implementation instead of these
 * stubs. The extras module is ABI-aware (SreExtrasInit::abi, filled per
 * engine version by the host) and is served to both 1.4.12 and 1.4.13.
 *
 * Unlike sre12, SRE13 compiles the vendored Lua 5.1 into the library, so
 * these stubs use the plain lua_* C API (lua.h / lauxlib.h) directly on
 * the engine's lua_State — same pattern as sre13_scene_shifter.c and
 * sre13_button_controller.c.
 */

#include "sre13.h"
#include "core/hook.h"
#include "core/log.h"
#include "core/stdstring.h"
#include "caver/Scene.h"
#include "caver/GameViewController.h"
#include "caver/GameSceneController.h"
#include "caver/PlayerProfile.h"
#include "caver/Component.h"
#include "caver/Component/CharControllerComponent.h"
#include <lua.h>
#include <lauxlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Set by the host to the guest address of the extras' miniLL_open_memory()
 * when libsre-extras.so is present. 0 => use stubs. */
__attribute__((visibility("default")))
void* g_sre_extras_miniLL_open_memory = 0;

/* Mini game-API host-cooperative globals (lawncher mini.c parity; declared
 * extern in sre13.h). Ownership mirrors sre12's g_sre_coin_limit: the SRE
 * library owns the storage, the desktop host reads it via get_symbol_vaddr
 * to apply the clamp / debug flag without rewriting engine .text from the
 * guest. -1 / 0 mean "unset" so the host can distinguish "never called". */
__attribute__((visibility("default")))
volatile int g_sre13_coin_limit = -1;
__attribute__((visibility("default")))
volatile int g_sre13_debug_active = 0;

#define STUB_MT "Mini.MemoryAddress"

static int sre13_extras_log_once(void) {
    static int s_logged = 0;
    if (!s_logged) {
        s_logged = 1;
        LOGE("[SRE13/Extras] libsre-extras.so not loaded — using stub memory/ffi API.");
    }
    return 1;
}

/* =========================================================================
 * Stub MemoryAddress userdata — read/write methods are safe no-ops.
 * ========================================================================= */
typedef struct {
    void* ptr;
} StubAddr;

static StubAddr* sre13_stub_check_addr(lua_State* L, int i) {
    if (!lua_isuserdata(L, i)) return NULL;
    void* ud = lua_touserdata(L, i);
    if (lua_getmetatable(L, i)) {
        lua_getfield(L, LUA_REGISTRYINDEX, STUB_MT);
        int eq = lua_rawequal(L, -1, -2);
        lua_settop(L, -2);
        if (!eq) return NULL;
    } else {
        return NULL;
    }
    return (StubAddr*)ud;
}

static StubAddr* sre13_stub_push_addr(lua_State* L, void* ptr) {
    StubAddr* addr = (StubAddr*)lua_newuserdata(L, sizeof(*addr));
    if (!addr) return NULL;
    addr->ptr = ptr;
    lua_getfield(L, LUA_REGISTRYINDEX, STUB_MT);
    lua_setmetatable(L, -2);
    return addr;
}

/* read* -> nil (unknown), write* -> no-op */
#define STUB_READ(name) \
    static int sre13_stub_read##name(lua_State* L) { (void)L; lua_pushnil(L); return 1; }
#define STUB_WRITE(name) \
    static int sre13_stub_write##name(lua_State* L) { (void)L; return 0; }
#define STUB_RW(name) STUB_READ(name) STUB_WRITE(name)

STUB_RW(Bool)
STUB_RW(Int8)  STUB_RW(Int16)  STUB_RW(Int32)  STUB_RW(Int64)
STUB_RW(UInt8) STUB_RW(UInt16) STUB_RW(UInt32) STUB_RW(UInt64)
STUB_RW(Float) STUB_RW(Double)
STUB_RW(Pointer) STUB_RW(CString) STUB_RW(CppString) STUB_RW(Vector3)

/* call — Raijin signature FFI: unavailable without the extras. */
static int sre13_stub_call(lua_State* L) {
    sre13_extras_log_once();
    lua_pushnil(L);
    return 1;
}

static int sre13_stub_free(lua_State* L) {
    StubAddr* addr = sre13_stub_check_addr(L, 1);
    if (addr && addr->ptr) {
        free(addr->ptr);
        addr->ptr = NULL;
    }
    return 0;
}

static int sre13_stub_add(lua_State* L) {
    StubAddr* addr = sre13_stub_check_addr(L, 1);
    if (!addr) { lua_pushnil(L); return 1; }
    sre13_stub_push_addr(L, (char*)addr->ptr + (ptrdiff_t)(int64_t)lua_tonumber(L, 2));
    return 1;
}

static int sre13_stub_sub(lua_State* L) {
    StubAddr* addr = sre13_stub_check_addr(L, 1);
    if (!addr) { lua_pushnil(L); return 1; }
    sre13_stub_push_addr(L, (char*)addr->ptr - (ptrdiff_t)(int64_t)lua_tonumber(L, 2));
    return 1;
}

static int sre13_stub_eq(lua_State* L) {
    StubAddr* a = sre13_stub_check_addr(L, 1);
    StubAddr* b = sre13_stub_check_addr(L, 2);
    lua_pushboolean(L, (a && b) && a->ptr == b->ptr);
    return 1;
}

static int sre13_stub_tostring(lua_State* L) {
    StubAddr* addr = sre13_stub_check_addr(L, 1);
    char buf[32];
    snprintf(buf, sizeof(buf), "%p", addr ? addr->ptr : NULL);
    lua_pushstring(L, buf);
    return 1;
}

static int sre13_stub_offset(lua_State* L) {
    StubAddr* addr = sre13_stub_check_addr(L, 1);
    if (!addr) { lua_pushnil(L); return 1; }
    sre13_stub_push_addr(L, (char*)addr->ptr + (ptrdiff_t)(int64_t)lua_tonumber(L, 2));
    return 1;
}

static int sre13_stub_getAddress(lua_State* L) {
    StubAddr* addr = sre13_stub_check_addr(L, 1);
    lua_pushlightuserdata(L, addr ? addr->ptr : NULL);
    return 1;
}

static int sre13_stub_isNull(lua_State* L) {
    StubAddr* addr = sre13_stub_check_addr(L, 1);
    lua_pushboolean(L, !addr || addr->ptr == NULL);
    return 1;
}

static int sre13_stub_gc(lua_State* L) { (void)L; return 0; }

/* =========================================================================
 * Mini.* library stubs
 * ========================================================================= */
static int sre13_stub_get_address(lua_State* L) {
    const void* ptr = lua_topointer(L, 1);
    if (!ptr) { lua_pushnil(L); return 1; }
    sre13_stub_push_addr(L, (void*)ptr);
    return 1;
}

static int sre13_stub_malloc(lua_State* L) {
    size_t size = (size_t)(int64_t)lua_tonumber(L, 1);
    void* ptr = NULL;
    if (size > 0) {
        ptr = malloc(size);
        if (ptr) memset(ptr, 0, size);
    }
    sre13_stub_push_addr(L, ptr);
    return 1;
}

static int sre13_stub_dlsym(lua_State* L) {
    sre13_extras_log_once();
    lua_pushnil(L);
    return 1;
}

static int sre13_stub_get_component_address(lua_State* L) {
    sre13_extras_log_once();
    lua_pushnil(L);
    return 1;
}

/* ffi.call_sig(addr, sig, ...) — unavailable without the extras. */
static int sre13_stub_ffi_call_sig(lua_State* L) {
    sre13_extras_log_once();
    lua_pushnil(L);
    return 1;
}

/* =========================================================================
 * Stub _G.ffi — registered by sre13_extras_inject(). When the extras are
 * present, the extras' sre_ffi_register_lua() overwrites this table with
 * the real libffi-backed one. Pure-math / introspection functions work for
 * real; everything needing libffi/allocation/mprotect/dlopen is a safe stub.
 * ========================================================================= */

static uint64_t sffi13_addr(lua_State* L, int idx) {
    return (uint64_t)(int64_t)lua_tonumber(L, idx);
}

/* --- real (safe) --- */
static int sffi13_offset(lua_State* L) {
    int n = lua_gettop(L);
    uint64_t a = (uint64_t)(int64_t)lua_tonumber(L, 1);
    for (int i = 2; i <= n; i++) a += (uint64_t)(int64_t)lua_tonumber(L, i);
    lua_pushnumber(L, (double)a);
    return 1;
}
static int sffi13_null(lua_State* L) { lua_pushnumber(L, 0.0); return 1; }
static int sffi13_base(lua_State* L) { lua_pushnumber(L, (double)g_swordigo_base); return 1; }
static int sffi13_at(lua_State* L) {
    uint64_t off = (uint64_t)(int64_t)lua_tonumber(L, 1);
    lua_pushnumber(L, (double)(g_swordigo_base + off));
    return 1;
}
static int sffi13_addr_math(lua_State* L) {
    uint64_t p = sffi13_addr(L, 1);
    int64_t  o = (int64_t)lua_tonumber(L, 2);
    lua_pushnumber(L, (double)(uint64_t)(p + o));
    return 1;
}
static int sffi13_deref(lua_State* L) {
    uint64_t a = sffi13_addr(L, 1);
    if (!a) { lua_pushnumber(L, 0.0); return 1; }
    lua_pushnumber(L, (double)(*(uint64_t*)(uintptr_t)a));
    return 1;
}
static int sffi13_typeof(lua_State* L) {
    switch (lua_type(L, 1)) {
        case LUA_TNUMBER:        lua_pushstring(L, "number");   break;
        case LUA_TSTRING:        lua_pushstring(L, "string");   break;
        case LUA_TBOOLEAN:       lua_pushstring(L, "bool");     break;
        case LUA_TTABLE:         lua_pushstring(L, "table");    break;
        case LUA_TFUNCTION:      lua_pushstring(L, "function"); break;
        case LUA_TLIGHTUSERDATA: lua_pushstring(L, "ptr");      break;
        default:                 lua_pushstring(L, "nil");      break;
    }
    return 1;
}
static int sffi13_tonumber(lua_State* L) { lua_pushnumber(L, lua_tonumber(L, 1)); return 1; }
static int sffi13_tobool(lua_State* L)   { lua_pushboolean(L, lua_toboolean(L, 1)); return 1; }
static int sffi13_errno(lua_State* L)    { lua_pushnumber(L, 0.0); return 1; }
static int sffi13_abi(lua_State* L) {
    const char* q = lua_tostring(L, 1);
    if (!q) {
#if defined(__aarch64__)
        lua_pushstring(L, "arm64");
#else
        lua_pushstring(L, "unknown");
#endif
        return 1;
    }
    int r = 0;
    if (!strcmp(q, "64bit")) r = (sizeof(void*) == 8);
    else if (!strcmp(q, "le")) { unsigned short x = 1; r = (*(unsigned char*)&x == 1); }
    else if (!strcmp(q, "be")) { unsigned short x = 1; r = (*(unsigned char*)&x == 0); }
    lua_pushboolean(L, r);
    return 1;
}
static int sffi13_sizeof(lua_State* L) {
    const char* n = lua_tostring(L, 1);
    if (!n) { lua_pushnil(L); return 1; }
    int w = 0;
    if (!strcmp(n,"i8")||!strcmp(n,"u8")||!strcmp(n,"bool")) w = 1;
    else if (!strcmp(n,"i16")||!strcmp(n,"u16")) w = 2;
    else if (!strcmp(n,"i32")||!strcmp(n,"u32")||!strcmp(n,"f32")||!strcmp(n,"float")||!strcmp(n,"int")) w = 4;
    else if (!strcmp(n,"i64")||!strcmp(n,"u64")||!strcmp(n,"f64")||!strcmp(n,"double")||!strcmp(n,"ptr")||!strcmp(n,"pointer")) w = 8;
    else if (!strcmp(n,"Vector2")) w = 8;
    else if (!strcmp(n,"Vector3")) w = 12;
    else if (!strcmp(n,"Quaternion")||!strcmp(n,"FloatColor")||!strcmp(n,"Rectangle")) w = 16;
    else if (!strcmp(n,"Matrix4")) w = 64;
    if (w) lua_pushnumber(L, (double)w); else lua_pushnil(L);
    return 1;
}

/* --- safe stubs: reads -> nil --- */
static int sffi13_nil(lua_State* L)   { lua_pushnil(L); return 1; }
/* --- safe stubs: writes / dangerous -> no-op --- */
static int sffi13_noop(lua_State* L)  { (void)L; return 0; }
/* --- safe stubs: dangerous -> false --- */
static int sffi13_false(lua_State* L) { lua_pushboolean(L, 0); return 1; }
static int sffi13_risky_mode(lua_State* L) { (void)L; lua_pushboolean(L, 0); return 1; }

static void sffi13_set(lua_State* L, const char* name, int (*fn)(lua_State*)) {
    lua_pushcclosure(L, fn, 0);
    lua_setfield(L, -2, name);
}

static void sre13_extras_stub_register_ffi(lua_State* L) {
    lua_createtable(L, 0, 48);

    sffi13_set(L, "offset",    sffi13_offset);
    sffi13_set(L, "null",      sffi13_null);
    sffi13_set(L, "base",      sffi13_base);
    sffi13_set(L, "at",        sffi13_at);
    sffi13_set(L, "addr",      sffi13_addr_math);
    sffi13_set(L, "deref",     sffi13_deref);
    sffi13_set(L, "typeof",    sffi13_typeof);
    sffi13_set(L, "tonumber",  sffi13_tonumber);
    sffi13_set(L, "tobool",    sffi13_tobool);
    sffi13_set(L, "errno",     sffi13_errno);
    sffi13_set(L, "abi",       sffi13_abi);
    sffi13_set(L, "sizeof",    sffi13_sizeof);
    sffi13_set(L, "alignment", sffi13_sizeof);

    sffi13_set(L, "call",  sffi13_nil);
    sffi13_set(L, "bind",  sffi13_nil);
    sffi13_set(L, "cast",  sffi13_nil);
    sffi13_set(L, "new",   sffi13_nil);
    sffi13_set(L, "alloc", sffi13_nil);
    sffi13_set(L, "load",  sffi13_nil);

    sffi13_set(L, "peek8",   sffi13_nil);
    sffi13_set(L, "peek16",  sffi13_nil);
    sffi13_set(L, "peek32",  sffi13_nil);
    sffi13_set(L, "peek64",  sffi13_nil);
    sffi13_set(L, "peekf",   sffi13_nil);
    sffi13_set(L, "peekstr", sffi13_nil);
    sffi13_set(L, "readf32", sffi13_nil);
    sffi13_set(L, "readf64", sffi13_nil);
    sffi13_set(L, "readi32", sffi13_nil);
    sffi13_set(L, "readi64", sffi13_nil);
    sffi13_set(L, "string",  sffi13_nil);
    sffi13_set(L, "tostring",sffi13_nil);
    sffi13_set(L, "search",  sffi13_nil);

    sffi13_set(L, "poke8",    sffi13_noop);
    sffi13_set(L, "poke16",   sffi13_noop);
    sffi13_set(L, "poke32",   sffi13_noop);
    sffi13_set(L, "poke64",   sffi13_noop);
    sffi13_set(L, "pokef",    sffi13_noop);
    sffi13_set(L, "writef32", sffi13_noop);
    sffi13_set(L, "writei32", sffi13_noop);
    sffi13_set(L, "memcpy",   sffi13_noop);
    sffi13_set(L, "memset",   sffi13_noop);
    sffi13_set(L, "copy",     sffi13_noop);
    sffi13_set(L, "fill",     sffi13_noop);
    sffi13_set(L, "free",     sffi13_noop);
    sffi13_set(L, "dump",     sffi13_noop);

    sffi13_set(L, "risky_mode", sffi13_risky_mode);
    sffi13_set(L, "patch",      sffi13_false);
    sffi13_set(L, "seal",       sffi13_false);
    sffi13_set(L, "unseal",     sffi13_false);

    lua_setfield(L, LUA_GLOBALSINDEX, "ffi");
}

/* =========================================================================
 * Registration — Mini is expected at the TOP of the Lua stack.
 * ========================================================================= */
static void sre13_stub_set(lua_State* L, const char* name, int (*fn)(lua_State*)) {
    lua_pushcclosure(L, fn, 0);
    lua_setfield(L, -2, name);
}

extern volatile float g_sre_cam_x;
extern volatile float g_sre_cam_y;
extern volatile float g_sre_cam_z;
extern volatile float g_sre_cam_zoom;
extern volatile int   g_sre_cam_follow;
extern volatile int   g_sre_cam_set_pending;
extern int            g_sre_cam_active;
extern float          g_sre_cam_fov;
extern float          g_sre_cam_yaw;
extern float          g_sre_cam_pitch;
extern float          g_sre_cam_roll;

static int sre13_cam_get_pos(lua_State* L) {
    lua_createtable(L, 0, 3);
    lua_pushnumber(L, (lua_Number)g_sre_cam_x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, (lua_Number)g_sre_cam_y);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, (lua_Number)g_sre_cam_z);
    lua_setfield(L, -2, "z");
    return 1;
}

static int sre13_cam_set_pos(lua_State* L) {
    g_sre_cam_x = (float)lua_tonumber(L, 1);
    g_sre_cam_y = (float)lua_tonumber(L, 2);
    g_sre_cam_z = (float)lua_tonumber(L, 3);
    g_sre_cam_set_pending = 1;
    return 0;
}

static int sre13_cam_get_zoom(lua_State* L) {
    lua_pushnumber(L, (lua_Number)g_sre_cam_zoom);
    return 1;
}

static int sre13_cam_set_zoom(lua_State* L) {
    float z = (float)lua_tonumber(L, 1);
    if (z > 0.001f && z <= 100.0f) g_sre_cam_zoom = z;
    return 0;
}

static int sre13_cam_get_follow(lua_State* L) {
    lua_pushboolean(L, g_sre_cam_follow);
    return 1;
}

static int sre13_cam_set_follow(lua_State* L) {
    g_sre_cam_follow = lua_toboolean(L, 1);
    return 0;
}

static int sre13_cam_get_fov(lua_State* L) {
    lua_pushnumber(L, (lua_Number)g_sre_cam_fov);
    return 1;
}

static int sre13_cam_set_fov(lua_State* L) {
    g_sre_cam_fov = (float)lua_tonumber(L, 1);
    return 0;
}

static int sre13_cam_get_rot(lua_State* L) {
    lua_createtable(L, 0, 3);
    lua_pushnumber(L, (lua_Number)g_sre_cam_yaw);
    lua_setfield(L, -2, "yaw");
    lua_pushnumber(L, (lua_Number)g_sre_cam_pitch);
    lua_setfield(L, -2, "pitch");
    lua_pushnumber(L, (lua_Number)g_sre_cam_roll);
    lua_setfield(L, -2, "roll");
    return 1;
}

static int sre13_cam_set_rot(lua_State* L) {
    g_sre_cam_yaw = (float)lua_tonumber(L, 1);
    g_sre_cam_pitch = (float)lua_tonumber(L, 2);
    g_sre_cam_roll = (float)lua_tonumber(L, 3);
    return 0;
}

static int sre13_cam_set_active(lua_State* L) {
    g_sre_cam_active = lua_toboolean(L, 1);
    return 0;
}

/* =========================================================================
 * Mini game-API (lawncher api/mini/mini.c + mini_character.c parity,
 * desktop-native — no JNI, no Android views).
 *
 * The lawncher injects these from a RegisterProgramLibrary hook; on desktop
 * the host deliberately does NOT hook that symbol (SRE13 injects through
 * sre13_extras_inject instead), so the same surface is registered here with
 * sre13's vendored-Lua pattern. Every DL_SYMBOL target below was verified
 * present in the 1.4.13 arm64 binary. Raw-offset writes are faithful to the
 * lawncher originals and flagged where they land inside the recovered
 * layout's pad regions / disagree with the named fields.
 * ========================================================================= */

DL_SYMBOL(
    GOV_SetControlsHidden,
    "_ZN5Caver15GameOverlayView17SetControlsHiddenEb",
    void, (void *gov, bool hidden)
);

DL_SYMBOL(
    GSC_CreateHeroObjectAt,
    "_ZN5Caver19GameSceneController18CreateHeroObjectAtERKNS_7Vector3Eib",
    void, (GameSceneController *gsc, const Vector3 *pos, int facing_dir, bool add_to_scene)
);

static int sre13_mini_arch(lua_State *L) {
    lua_pushstring(L, archSplit("armeabi-v7a", "arm64-v8a"));
    return 1;
}

/* The lawncher writes a raw Scene+0x2d0 bool ("hitboxes should be at 0x1c4"
 * — upstream comment, unverified). sre13's recovered Scene.h names the flag
 * debugHitboxes, so set the named field instead of a guessed offset. */
static int sre13_mini_toggle_debug(lua_State *L) {
    Scene *scene = scene_from_L(L);
    if (!scene) return 0;
    scene->debugHitboxes = true;
    g_sre13_debug_active = 1;
    return 0;
}

static int sre13_mini_set_controls_hidden(lua_State *L) {
    bool hidden = lua_toboolean(L, 1);
    GameViewController *gvc = gvc_from_L(L);
    if (!gvc || !gvc->GameSceneView) return 0;
    void *gsv = gvc->GameSceneView;
    void *gov = *$(void *, gsv, 0xd4, 0x110);
    if (!gov) return 0;
    *$(bool, gov, 0x11c, 0x198) = !hidden;
    *$(bool, gov, 0xc4, 0xf4) = hidden;
    if (GOV_SetControlsHidden) {
        GOV_SetControlsHidden(gov, hidden);
    }
    return 0;
}

static int sre13_mini_get_profile_id(lua_State *L) {
    GameViewController *gvc = gvc_from_L(L);
    if (!gvc || !gvc->PlayerProfile) return 0;
    PlayerProfile *pp = (PlayerProfile *)gvc->PlayerProfile;
    const char *id = String_get(&pp->Identifier);
    if (id) {
        lua_pushstring(L, id);
        return 1;
    }
    return 0;
}

static int sre13_mini_recreate_hero(lua_State *L) {
    GameSceneController *gsc = gsc_from_L(L);
    if (!gsc || !gsc->hero) return 0;
    SceneObject *hero = gsc->hero;
    Vector3 pos = hero->Position;
    Component *entity = component_fetch(hero, "EntityComponent");
    if (!entity) {
        LOGE("[Mini.RecreateHero] SceneObject '%s' has no EntityComponent.",
             String_get(&hero->Identifier));
        return 0;
    }
    int dir = *$(int, entity, 0x38, 0x68);
    if (GSC_CreateHeroObjectAt) {
        GSC_CreateHeroObjectAt(gsc, &pos, dir, 1);
    }
    return 0;
}

static int sre13_mini_set_coin_limit(lua_State *L) {
    g_sre13_coin_limit = (int)lua_tointeger(L, 1);
    return 0;
}

/* ---- Mini.Character (lawncher api/mini/mini_character.c) ---- */

static CharControllerComponent *sre13_mini_fetch_cc(lua_State *L) {
    GameSceneController *gsc = gsc_from_L(L);
    if (!gsc || !gsc->hero) return NULL;
    return (CharControllerComponent *)component_fetch(gsc->hero, "CharControllerComponent");
}

#define SRE13_CC_GET_F(NAME, FIELD) \
    static int sre13_mini_##NAME(lua_State *L) { \
        CharControllerComponent *cc = sre13_mini_fetch_cc(L); \
        if (!cc) return 0; \
        lua_pushnumber(L, (lua_Number)cc->FIELD); \
        return 1; \
    }
#define SRE13_CC_SET_F(NAME, FIELD) \
    static int sre13_mini_##NAME(lua_State *L) { \
        CharControllerComponent *cc = sre13_mini_fetch_cc(L); \
        if (!cc) return 0; \
        cc->FIELD = (float)lua_tonumber(L, 1); \
        return 0; \
    }
#define SRE13_CC_GET_I(NAME, FIELD) \
    static int sre13_mini_##NAME(lua_State *L) { \
        CharControllerComponent *cc = sre13_mini_fetch_cc(L); \
        if (!cc) return 0; \
        lua_pushinteger(L, cc->FIELD); \
        return 1; \
    }
#define SRE13_CC_SET_I(NAME, FIELD) \
    static int sre13_mini_##NAME(lua_State *L) { \
        CharControllerComponent *cc = sre13_mini_fetch_cc(L); \
        if (!cc) return 0; \
        cc->FIELD = (int)lua_tointeger(L, 1); \
        return 0; \
    }

SRE13_CC_GET_F(GetWalkSpeed, runSpeed)
SRE13_CC_SET_F(SetWalkSpeed, runSpeed)
SRE13_CC_GET_F(GetRunSpeed, fastRunSpeed)
SRE13_CC_SET_F(SetRunSpeed, fastRunSpeed)
SRE13_CC_GET_F(GetJumpSpeed, jumpSpeed)
SRE13_CC_SET_F(SetJumpSpeed, jumpSpeed)
SRE13_CC_GET_I(GetAirJumpUsed, airJumpCount)
SRE13_CC_SET_I(SetAirJumpUsed, airJumpCount)

/* Caver CharControllerComponent method wrappers — same VOID/BOOL/DIR pattern
 * as the lawncher's mini_character.c, with sre13-style NULL guards. */
#define SRE13_CC_VOID(NAME, SYM) \
    DL_SYMBOL(NAME, SYM, void, (CharControllerComponent *cc)); \
    static int sre13_mini_##NAME(lua_State *L) { \
        CharControllerComponent *cc = sre13_mini_fetch_cc(L); \
        if (!cc) return 0; \
        if (NAME) NAME(cc); \
        return 0; \
    }
#define SRE13_CC_BOOL(NAME, SYM) \
    DL_SYMBOL(NAME, SYM, bool, (CharControllerComponent *cc)); \
    static int sre13_mini_##NAME(lua_State *L) { \
        CharControllerComponent *cc = sre13_mini_fetch_cc(L); \
        if (!cc) return 0; \
        lua_pushboolean(L, NAME ? NAME(cc) : false); \
        return 1; \
    }
#define SRE13_CC_DIR(NAME, SYM) \
    DL_SYMBOL(NAME, SYM, void, (CharControllerComponent *cc, int dir)); \
    static int sre13_mini_##NAME(lua_State *L) { \
        int dir = (int)luaL_checkinteger(L, 1); \
        CharControllerComponent *cc = sre13_mini_fetch_cc(L); \
        if (!cc) return 0; \
        if (NAME) NAME(cc, dir == 1 ? 1 : -1); \
        return 0; \
    }

SRE13_CC_VOID(DropQuickly,   "_ZN5Caver23CharControllerComponent11DropQuicklyEv")
SRE13_CC_VOID(StartJumping,  "_ZN5Caver23CharControllerComponent12StartJumpingEv")
SRE13_CC_VOID(StopJumping,   "_ZN5Caver23CharControllerComponent11StopJumpingEv")
SRE13_CC_VOID(CancelCasting, "_ZN5Caver23CharControllerComponent13CancelCastingEv")
SRE13_CC_VOID(FinishCasting, "_ZN5Caver23CharControllerComponent13FinishCastingEv")
SRE13_CC_VOID(Die,           "_ZN5Caver23CharControllerComponent3DieEv")
SRE13_CC_VOID(Use,           "_ZN5Caver23CharControllerComponent3UseEv")
SRE13_CC_VOID(Hurt,          "_ZN5Caver23CharControllerComponent4HurtEv")
SRE13_CC_VOID(Swing,         "_ZN5Caver23CharControllerComponent5SwingEv")
SRE13_CC_VOID(StopSwing,     "_ZN5Caver23CharControllerComponent9StopSwingEv")

SRE13_CC_BOOL(CanDoSomething,   "_ZN5Caver23CharControllerComponent14CanDoSomethingEv")
SRE13_CC_BOOL(CanBeginCasting,  "_ZN5Caver23CharControllerComponent15CanBeginCastingEv")
SRE13_CC_BOOL(CanUse,           "_ZN5Caver23CharControllerComponent6CanUseEv")
SRE13_CC_BOOL(CanJump,          "_ZN5Caver23CharControllerComponent7CanJumpEv")
SRE13_CC_BOOL(CanSwing,         "_ZN5Caver23CharControllerComponent8CanSwingEv")
SRE13_CC_BOOL(CanPickup,        "_ZN5Caver23CharControllerComponent9CanPickupEv")

SRE13_CC_DIR(StartMovingToDirection, "_ZN5Caver23CharControllerComponent22StartMovingToDirectionEi")
SRE13_CC_DIR(StopMovingToDirection,  "_ZN5Caver23CharControllerComponent21StopMovingToDirectionEi")

static int sre13_mini_cc_set_movement_facing_lock(lua_State *L) {
    bool lock = lua_toboolean(L, 1);
    CharControllerComponent *cc = sre13_mini_fetch_cc(L);
    if (!cc) return 0;
    /* Raw 0x2e4 write, faithful to the lawncher. Note the recovered
     * CharControllerComponent.h names this byte isCasting — the lawncher's
     * "movement facing lock" offset and the header field disagree; upstream
     * offset kept as-is, unverified. */
    *$(bool, cc, 0x1c4, 0x2e4) = lock;
    return 0;
}

static int sre13_mini_cc_set_stun_time(lua_State *L) {
    float time = (float)lua_tonumber(L, 1);
    CharControllerComponent *cc = sre13_mini_fetch_cc(L);
    if (!cc) return 0;
    /* Raw 0x220 write (lands inside the recovered layout's unnamed pad
     * region), faithful to the lawncher — unverified offset. */
    *$(float, cc, 0x11c, 0x220) = time;
    return 0;
}

static void sre13_extras_stub_register_memory(lua_State* L) {
    /* ---- Mini.MemoryAddress metatable (registry) ---- */
    lua_getfield(L, LUA_REGISTRYINDEX, STUB_MT);
    if (lua_type(L, -1) == LUA_TNIL) {
        lua_settop(L, -2);
        lua_createtable(L, 0, 32);

        sre13_stub_set(L, "__gc",        sre13_stub_gc);
        sre13_stub_set(L, "__add",       sre13_stub_add);
        sre13_stub_set(L, "__sub",       sre13_stub_sub);
        sre13_stub_set(L, "__eq",        sre13_stub_eq);
        sre13_stub_set(L, "__tostring",  sre13_stub_tostring);

        sre13_stub_set(L, "offset",      sre13_stub_offset);
        sre13_stub_set(L, "getAddress",  sre13_stub_getAddress);
        sre13_stub_set(L, "isNull",      sre13_stub_isNull);
        sre13_stub_set(L, "free",        sre13_stub_free);

        sre13_stub_set(L, "readBool",    sre13_stub_readBool);
        sre13_stub_set(L, "writeBool",   sre13_stub_writeBool);
        sre13_stub_set(L, "readInt8",    sre13_stub_readInt8);
        sre13_stub_set(L, "writeInt8",   sre13_stub_writeInt8);
        sre13_stub_set(L, "readInt16",   sre13_stub_readInt16);
        sre13_stub_set(L, "writeInt16",  sre13_stub_writeInt16);
        sre13_stub_set(L, "readInt32",   sre13_stub_readInt32);
        sre13_stub_set(L, "writeInt32",  sre13_stub_writeInt32);
        sre13_stub_set(L, "readInt64",   sre13_stub_readInt64);
        sre13_stub_set(L, "writeInt64",  sre13_stub_writeInt64);
        sre13_stub_set(L, "readUInt8",   sre13_stub_readUInt8);
        sre13_stub_set(L, "writeUInt8",  sre13_stub_writeUInt8);
        sre13_stub_set(L, "readUInt16",  sre13_stub_readUInt16);
        sre13_stub_set(L, "writeUInt16", sre13_stub_writeUInt16);
        sre13_stub_set(L, "readUInt32",  sre13_stub_readUInt32);
        sre13_stub_set(L, "writeUInt32", sre13_stub_writeUInt32);
        sre13_stub_set(L, "readUInt64",  sre13_stub_readUInt64);
        sre13_stub_set(L, "writeUInt64", sre13_stub_writeUInt64);
        sre13_stub_set(L, "readFloat",   sre13_stub_readFloat);
        sre13_stub_set(L, "writeFloat",  sre13_stub_writeFloat);
        sre13_stub_set(L, "readDouble",  sre13_stub_readDouble);
        sre13_stub_set(L, "writeDouble", sre13_stub_writeDouble);
        sre13_stub_set(L, "readPointer",  sre13_stub_readPointer);
        sre13_stub_set(L, "writePointer", sre13_stub_writePointer);
        sre13_stub_set(L, "readCString",  sre13_stub_readCString);
        sre13_stub_set(L, "writeCString", sre13_stub_writeCString);
        sre13_stub_set(L, "readCppString",  sre13_stub_readCppString);
        sre13_stub_set(L, "writeCppString", sre13_stub_writeCppString);
        sre13_stub_set(L, "readVector3",  sre13_stub_readVector3);
        sre13_stub_set(L, "writeVector3", sre13_stub_writeVector3);
        sre13_stub_set(L, "call",        sre13_stub_call);

        /* mt.__index = mt */
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");

        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, STUB_MT);
        /* Pop the metatable so Mini (below) is the top table again. */
        lua_settop(L, -2);
    } else {
        lua_settop(L, -2);
    }

    /* ---- Mini.* (table at top) ---- */
    sre13_stub_set(L, "GetAddress",          sre13_stub_get_address);
    sre13_stub_set(L, "Malloc",              sre13_stub_malloc);
    sre13_stub_set(L, "Dlsym",               sre13_stub_dlsym);
    sre13_stub_set(L, "GetComponentAddress", sre13_stub_get_component_address);

    /* ---- ffi.call_sig (Raijin signature FFI stub) ---- */
    lua_getfield(L, LUA_GLOBALSINDEX, "ffi");
    if (lua_type(L, -1) == LUA_TTABLE) {
        sre13_stub_set(L, "call_sig", sre13_stub_ffi_call_sig);
    }
    lua_settop(L, -2);
}

/* Game-API + camera surface — ALWAYS registered into Mini, regardless of
 * whether libsre-extras.so is loaded (the extras lib only layers memory/ffi
 * on top; the game API must not depend on which memory layer is active).
 * Mini is expected at the TOP of the Lua stack. */
static void sre13_extras_stub_register_lua(lua_State* L) {
    /* ---- Mini.Camera (SRE12 parity + SRE13 free-look range) ---- */
    lua_createtable(L, 0, 12);
    sre13_stub_set(L, "GetPosition", sre13_cam_get_pos);
    sre13_stub_set(L, "SetPosition", sre13_cam_set_pos);
    sre13_stub_set(L, "GetZoom",     sre13_cam_get_zoom);
    sre13_stub_set(L, "SetZoom",     sre13_cam_set_zoom);
    sre13_stub_set(L, "GetFollow",   sre13_cam_get_follow);
    sre13_stub_set(L, "SetFollow",   sre13_cam_set_follow);
    sre13_stub_set(L, "GetFOV",      sre13_cam_get_fov);
    sre13_stub_set(L, "SetFOV",      sre13_cam_set_fov);
    sre13_stub_set(L, "GetRotation", sre13_cam_get_rot);
    sre13_stub_set(L, "SetRotation", sre13_cam_set_rot);
    sre13_stub_set(L, "SetActive",   sre13_cam_set_active);
    lua_setfield(L, -2, "Camera");

    /* ---- lawncher mini.c parity: game API ---- */
    sre13_stub_set(L, "Arch",              sre13_mini_arch);
    sre13_stub_set(L, "ToggleDebug",       sre13_mini_toggle_debug);
    sre13_stub_set(L, "SetControlsHidden", sre13_mini_set_controls_hidden);
    sre13_stub_set(L, "GetProfileID",      sre13_mini_get_profile_id);
    sre13_stub_set(L, "RecreateHero",      sre13_mini_recreate_hero);
    sre13_stub_set(L, "SetCoinLimit",      sre13_mini_set_coin_limit);

    /* ---- Mini.Character (lawncher mini_character.c parity) ---- */
    lua_createtable(L, 0, 24);
    sre13_stub_set(L, "GetWalkSpeed",   sre13_mini_GetWalkSpeed);
    sre13_stub_set(L, "SetWalkSpeed",   sre13_mini_SetWalkSpeed);
    sre13_stub_set(L, "GetRunSpeed",    sre13_mini_GetRunSpeed);
    sre13_stub_set(L, "SetRunSpeed",    sre13_mini_SetRunSpeed);
    sre13_stub_set(L, "GetJumpSpeed",   sre13_mini_GetJumpSpeed);
    sre13_stub_set(L, "SetJumpSpeed",   sre13_mini_SetJumpSpeed);
    sre13_stub_set(L, "GetAirJumpUsed", sre13_mini_GetAirJumpUsed);
    sre13_stub_set(L, "SetAirJumpUsed", sre13_mini_SetAirJumpUsed);

    sre13_stub_set(L, "DropQuickly",   sre13_mini_DropQuickly);
    sre13_stub_set(L, "StartJumping",  sre13_mini_StartJumping);
    sre13_stub_set(L, "StopJumping",   sre13_mini_StopJumping);
    sre13_stub_set(L, "CancelCasting", sre13_mini_CancelCasting);
    sre13_stub_set(L, "FinishCasting", sre13_mini_FinishCasting);
    sre13_stub_set(L, "Die",           sre13_mini_Die);
    sre13_stub_set(L, "Use",           sre13_mini_Use);
    sre13_stub_set(L, "Hurt",          sre13_mini_Hurt);
    sre13_stub_set(L, "Swing",         sre13_mini_Swing);
    sre13_stub_set(L, "StopSwing",     sre13_mini_StopSwing);

    sre13_stub_set(L, "CanDoSomething",  sre13_mini_CanDoSomething);
    sre13_stub_set(L, "CanBeginCasting", sre13_mini_CanBeginCasting);
    sre13_stub_set(L, "CanUse",          sre13_mini_CanUse);
    sre13_stub_set(L, "CanJump",         sre13_mini_CanJump);
    sre13_stub_set(L, "CanSwing",        sre13_mini_CanSwing);
    sre13_stub_set(L, "CanPickup",       sre13_mini_CanPickup);

    sre13_stub_set(L, "StartMovingToDirection", sre13_mini_StartMovingToDirection);
    sre13_stub_set(L, "StopMovingToDirection",  sre13_mini_StopMovingToDirection);
    sre13_stub_set(L, "SetMovementFacingLock",  sre13_mini_cc_set_movement_facing_lock);
    sre13_stub_set(L, "SetStunTime",            sre13_mini_cc_set_stun_time);
    lua_setfield(L, -2, "Character");
}

/* =========================================================================
 * Injection — called once per process from sre_ProgramState_Execute's
 * module-injection block (mirrors sre12's RegisterProgramLibrary merge).
 * Stack-safe: saves/restores the top, like sre13_scene_shifter_register.
 * ========================================================================= */
__attribute__((visibility("default")))
void sre13_extras_inject(lua_State* L) {
    if (!L) return;
    int top = lua_gettop(L);

    /* Safe _G.ffi stub — the extras' sre_ffi_register_lua() overwrites it
     * when libsre-extras.so is loaded. */
    sre13_extras_stub_register_ffi(L);

    /* Mini = {} (top of stack) */
    lua_createtable(L, 0, 8);

    /* Game-API + camera (lawncher mini.c / mini_character.c parity) —
     * always present, independent of the memory layer. */
    sre13_extras_stub_register_lua(L);

    /* Memory layer: the extras' real Mini.MemoryAddress when libsre-extras.so
     * is loaded, otherwise the safe stub surface. The merge overwrites the
     * stub GetAddress/Malloc/Dlsym/GetComponentAddress entries. */
    if (g_sre_extras_miniLL_open_memory) {
        ((int (*)(lua_State*))g_sre_extras_miniLL_open_memory)(L);
        /* stack: Mini, lib — merge lib entries into Mini.
         * Idiom: [Mini,lib,key,value] → dup key (-2), dup value (-2 after
         * the key dup), rawset(Mini), pop value, keep key. */
        lua_pushnil(L);
        while (lua_next(L, -2)) {
            /* stack: Mini, lib, key, value */
            lua_pushvalue(L, -2);   /* dup key */
            lua_pushvalue(L, -2);   /* dup value */
            lua_rawset(L, -6);      /* Mini[key] = value */
            lua_settop(L, -2);      /* pop value, keep key */
        }
        lua_settop(L, -2);          /* pop lib — Mini stays on top */
    } else {
        sre13_extras_stub_register_memory(L);
    }

    lua_setglobal(L, "Mini");

    /* Also alias _G.memory = _G.Mini so scripts / consoles expecting memory.* or type(memory) work */
    lua_getglobal(L, "Mini");
    lua_setglobal(L, "memory");

    lua_settop(L, top);
}

/* =========================================================================
 * Stub implementations for extras mod_fs / mod_saves. These are weak
 * symbols — when libsre-extras.so is loaded, the real implementations
 * override them (same arrangement as sre12's sre_extras_stubs.c).
 * ========================================================================= */

/* --- mod_fs.c stubs --- */
void sre_extras_fs_set_mod_dir(const char* dir) { (void)dir; }
void sre_extras_fs_register(lua_State* L) { (void)L; /* no-op: SRE13's own fs stays */ }

/* --- mod_saves.c stubs --- */
void sre_extras_saves_init(const char* a, const char* b) { (void)a; (void)b; }
void sre_extras_init_saves(void) { /* no-op */ }
int  sre_extras_saves_active(void) { return 0; }
void* sre_extras_hook_byte_buffer_from_file(void* p, unsigned int* s) { (void)p; (void)s; return NULL; }
int  sre_extras_hook_save_byte_buffer_to_file(void* d, unsigned int s, void* p) { (void)d; (void)s; (void)p; return -1; }
int  sre_extras_hook_file_exists_at_path(void* p) { (void)p; return -1; }
void sre_extras_hook_delete_file_at_path(void* p) { (void)p; }