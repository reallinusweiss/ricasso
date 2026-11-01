/*
 * sre_extras.h — libsre-extras (GNU GPLv3 / extras)
 *
 * Optional ARM64 guest addon for SwordigoDesktop's SRE. Provides the
 * SwKiwi "Mini.MemoryAddress" API (memory read/write userdata, Dlsym,
 * Malloc, GetAddress, GetComponentAddress) and Raijin's libffi-based
 * signature FFI dispatcher, adapted to the PC port:
 *
 *   - Symbol resolution: engine_dlsym()  -> sre_resolve_address()
 *   - Component interfaces:  resolved by the host into <Class>_Interface
 *     globals (same names as libsre12.so, separate copy in this module)
 *   - CppString:  GNU libstdc++ COW std::string, same layout as SRE
 *   - Lua API:    g_lua_* function-pointer table (filled by the host via
 *     sre_extras_init), compiled through sre_lua_compat.h
 *   - FFI backend: vendored libffi (aarch64) compiled into this module
 *
 * Built standalone (own CMake/Makefile) and loaded by the host ONLY when
 * present. libsre12.so runs fine without it (stub memory/ffi API).
 */

#ifndef SRE_EXTRAS_H
#define SRE_EXTRAS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

/* =========================================================================
 * Caver math types (match SwKiwi caver/types.h)
 * ========================================================================= */
typedef struct { float x, y; }    Vector2;
typedef struct { float x, y, z; } Vector3;
typedef struct { float x, y, z, w; } Quaternion;
typedef struct { float R, G, B, A; } FloatColor;
typedef struct { float x, y, w, h; } Rectangle;
typedef struct { float m[4][4]; } Matrix4;

/* =========================================================================
 * CppString — GNU libstdc++ COW std::string (same layout as SRE's SreString)
 * ========================================================================= */
typedef struct {
    uint64_t length;    /* offset 0  */
    uint64_t capacity;  /* offset 8  */
    int32_t  refcount;  /* offset 16 */
    int32_t  _pad;      /* offset 20 */
} SreExtrasStringRep;

typedef struct {
    char* data;         /* points to char data after the _Rep header */
} CppString;

#define SRE_EXTRAS_REP(data_ptr) (((SreExtrasStringRep*)(data_ptr)) - 1)
#define ADJ(s)                  (((SreExtrasStringRep*)(s)) - 1)

/* Allocate a heap CppString copying `s` (refcount = 0, not shared). */
int  CppString_create(CppString** out, const char* s);
/* Release a heap CppString created by CppString_create. */
void CppString_release(CppString* cs);

/* =========================================================================
 * Lua API function pointers (filled by sre_extras_init, host-supplied)
 * ========================================================================= */
#ifndef lua_h
/* Only used when the real lua.h was not included yet. */
typedef void  lua_State;
#endif

/* g_lua_* globals — declared in sre_lua.h, defined in sre_extras_init.c.
 * sre_lua_compat.h redirects the lua_* C API to them. Two extras-only
 * additions not covered by sre_lua_compat.h: */
typedef void* (*pfn_lua_newuserdata)(lua_State*, size_t);
extern pfn_lua_newuserdata g_lua_newuserdata;
#ifndef lua_topointer
#define lua_topointer g_lua_topointer
#endif
#ifndef lua_newuserdata
#define lua_newuserdata g_lua_newuserdata
#endif

#define LUA_GLOBALSINDEX (-10002)
#define LUA_REGISTRYINDEX (-10000)

typedef int    (*pfn_ex_lua_gettop)(lua_State*);
typedef void   (*pfn_ex_lua_settop)(lua_State*, int);
typedef int    (*pfn_ex_lua_type)(lua_State*, int);
typedef double (*pfn_ex_lua_tonumber)(lua_State*, int);
typedef int    (*pfn_ex_lua_toboolean)(lua_State*, int);
typedef const char* (*pfn_ex_lua_tolstring)(lua_State*, int, size_t*);
typedef void*  (*pfn_ex_lua_touserdata)(lua_State*, int);
typedef const void* (*pfn_ex_lua_topointer)(lua_State*, int);
typedef void   (*pfn_ex_lua_pushnumber)(lua_State*, double);
typedef void   (*pfn_ex_lua_pushboolean)(lua_State*, int);
typedef void   (*pfn_ex_lua_pushnil)(lua_State*);
typedef void   (*pfn_ex_lua_pushstring)(lua_State*, const char*);
typedef void   (*pfn_ex_lua_pushlstring)(lua_State*, const char*, size_t);
typedef void   (*pfn_ex_lua_pushlightuserdata)(lua_State*, void*);
typedef void   (*pfn_ex_lua_pushcclosure)(lua_State*, int (*)(lua_State*), int);
typedef void   (*pfn_ex_lua_setfield)(lua_State*, int, const char*);
typedef void   (*pfn_ex_lua_getfield)(lua_State*, int, const char*);
typedef void   (*pfn_ex_lua_createtable)(lua_State*, int, int);
typedef void   (*pfn_ex_lua_settable)(lua_State*, int);
typedef void   (*pfn_ex_lua_gettable)(lua_State*, int);
typedef void   (*pfn_ex_lua_rawseti)(lua_State*, int, int);
typedef void   (*pfn_ex_lua_rawgeti)(lua_State*, int, int);
typedef void   (*pfn_ex_lua_pushvalue)(lua_State*, int);
typedef void   (*pfn_ex_lua_pushinteger)(lua_State*, int64_t);
typedef int64_t (*pfn_ex_lua_tointeger)(lua_State*, int);
typedef int    (*pfn_ex_lua_getmetatable)(lua_State*, int);
typedef int    (*pfn_ex_lua_setmetatable)(lua_State*, int);
typedef int    (*pfn_ex_lua_rawequal)(lua_State*, int, int);
typedef int    (*pfn_ex_lua_isuserdata)(lua_State*, int);
typedef int    (*pfn_ex_lua_isnumber)(lua_State*, int);
typedef int    (*pfn_ex_lua_isstring)(lua_State*, int);
typedef void*  (*pfn_ex_lua_newuserdata)(lua_State*, size_t);
typedef int    (*pfn_ex_lua_error)(lua_State*);

/* All Lua API pointers used by the extras. */
typedef struct {
    pfn_ex_lua_gettop        lua_gettop;
    pfn_ex_lua_settop        lua_settop;
    pfn_ex_lua_type          lua_type;
    pfn_ex_lua_tonumber      lua_tonumber;
    pfn_ex_lua_toboolean     lua_toboolean;
    pfn_ex_lua_tolstring     lua_tolstring;
    pfn_ex_lua_touserdata    lua_touserdata;
    pfn_ex_lua_topointer     lua_topointer;
    pfn_ex_lua_pushnumber    lua_pushnumber;
    pfn_ex_lua_pushboolean   lua_pushboolean;
    pfn_ex_lua_pushnil       lua_pushnil;
    pfn_ex_lua_pushstring    lua_pushstring;
    pfn_ex_lua_pushlstring   lua_pushlstring;
    pfn_ex_lua_pushlightuserdata lua_pushlightuserdata;
    pfn_ex_lua_pushcclosure  lua_pushcclosure;
    pfn_ex_lua_setfield      lua_setfield;
    pfn_ex_lua_getfield      lua_getfield;
    pfn_ex_lua_createtable   lua_createtable;
    pfn_ex_lua_settable      lua_settable;
    pfn_ex_lua_gettable      lua_gettable;
    pfn_ex_lua_rawseti       lua_rawseti;
    pfn_ex_lua_rawgeti       lua_rawgeti;
    pfn_ex_lua_pushvalue     lua_pushvalue;
    pfn_ex_lua_pushinteger   lua_pushinteger;
    pfn_ex_lua_tointeger     lua_tointeger;
    pfn_ex_lua_getmetatable  lua_getmetatable;
    pfn_ex_lua_setmetatable  lua_setmetatable;
    pfn_ex_lua_rawequal      lua_rawequal;
    pfn_ex_lua_isuserdata    lua_isuserdata;
    pfn_ex_lua_isnumber      lua_isnumber;
    pfn_ex_lua_isstring      lua_isstring;
    pfn_ex_lua_newuserdata   lua_newuserdata;
    pfn_ex_lua_error         lua_error;
} SreExtrasLuaApi;

/* =========================================================================
 * ABI config — version-dependent offsets/addresses (the "checker").
 *
 * Every engine-version-specific constant this module used to hardcode
 * (CppString layout, mangled engine symbols) is now supplied by the HOST
 * per running binary (swordi_abi 12 / 13) through SreExtrasInit::abi and
 * copied into the module global g_sre_extras_abi by sre_extras_init().
 * Consumers read the global — never a literal. sre_extras_abi_validate()
 * sanity-checks the values at init and clamps/falls back when the host
 * supplied nothing.
 *
 * Verified layouts (GNU libstdc++ COW std::string, both engine ABIs):
 *   - the string OBJECT is a single pointer to the char data
 *     (cppstring_data_off = 0)
 *   - the _Rep header precedes the data by 24 bytes
 *     (cppstring_rep_len = 24): { length u64 @0, capacity u64 @8,
 *       refcount s32 @16, pad @20, data... @24 }
 */
typedef struct {
    uint32_t swordi_abi;            /* 12 or 13 */
    uint32_t cppstring_data_off;    /* offset of the char-data pointer inside a
                                       CppString object (0 for GNU COW) */
    uint32_t cppstring_rep_len;     /* _Rep header bytes before the char data
                                       (24 for GNU COW) */
    uint32_t _reserved;
    uint64_t component_with_interface_fn; /* guest vaddr of
                                       Caver::SceneObject::ComponentWithInterface
                                       (host-resolved per version; 0 => lazy
                                       symbol fallback) */
} SreExtrasAbi;

/* =========================================================================
 * Init — called by the host right after loading this module into guest RAM.
 * ========================================================================= */
typedef struct {
    uint64_t        swordigo_base;   /* guest base of libswordigo.so */
    uint64_t        resolve_fn;      /* guest vaddr of sre_resolve_address
                                        (via host bridge import) — kept for
                                        completeness; the bridge import
                                        already works without it */
    SreExtrasAbi    abi;             /* version-dependent offsets/addresses */
    SreExtrasLuaApi lua;             /* Lua API function addresses */
} SreExtrasInit;

/* Live ABI config — read by all version-dependent code (never literals). */
extern SreExtrasAbi g_sre_extras_abi;

/* Validate the host-supplied ABI block: normalizes offsets, clamps to the
 * known-good values, fills safe fallbacks, and logs a report. Returns the
 * number of fields that needed correction (0 = all good). */
int sre_extras_abi_validate(const SreExtrasAbi* in, SreExtrasAbi* out);

/* Extract the C string from a CppString* (engine std::string*) using the
 * configured data offset. Returns NULL when the object/layout is bogus. */
const char* sre_extras_cppstring_data(const void* str_obj);

/* Entry points exported by this module. */
void    sre_extras_init(const SreExtrasInit* init);
int     sre_extras_version(void);
/* ABI tag reported by the host (0 = not initialized). */
int     sre_extras_abi_tag(void);
/* Registers Mini.MemoryAddress + Mini.* + ffi.call_sig into the current
 * Lua state. Returns the memory library table (merged into Mini by SRE). */
int     miniLL_open_memory(lua_State* L);
/* Convenience: full extras registration (memory + standalone ffi). */
int     sre_extras_register_lua(lua_State* L);

/* =========================================================================
 * Mod FS — sandboxed filesystem API for mods (mod_fs.c)
 * ========================================================================= */
void    sre_extras_fs_set_mod_dir(const char* dir);
void    sre_extras_fs_register(lua_State* L);

/* =========================================================================
 * Per-mod save isolation — redirect Documents/ to mod saves (mod_saves.c)
 * ========================================================================= */
void    sre_extras_saves_init(const char* mod_saves_dir, const char* documents_dir);
void    sre_extras_init_saves(void);
int     sre_extras_saves_active(void);

/* Hook callbacks for engine file I/O */
void*   sre_extras_hook_byte_buffer_from_file(void* path_str, unsigned int* out_size);
int     sre_extras_hook_save_byte_buffer_to_file(void* data, unsigned int size, void* path_str);
int     sre_extras_hook_file_exists_at_path(void* path_str);
void    sre_extras_hook_delete_file_at_path(void* path_str);

/* =========================================================================
 * Component interfaces — host resolves <Class>_Interface globals with the
 * same dynamic resolver it uses for libsre12.so (see main.cpp).
 * ========================================================================= */
#define SRE_EXTRAS_IFACE_LIST(X) \
    X(GlowComponent) X(ManaComponent) X(LightComponent) X(ModelComponent) \
    X(ShapeComponent) X(SkillComponent) X(SpellComponent) X(SwingComponent) \
    X(AttackComponent) X(DamageComponent) X(EntityComponent) X(HealthComponent) \
    X(PortalComponent) X(ShadowComponent) X(SpriteComponent) X(OverlayComponent) \
    X(ProgramComponent) X(ShatterComponent) X(ItemDropComponent) X(ParticleComponent) \
    X(AnimationComponent) X(MagicBoltComponent) X(MagicBombComponent) X(TouchableComponent) \
    X(TransformComponent) X(WaterMeshComponent) X(BackgroundComponent) X(EntityInfoComponent) \
    X(FireBreathComponent) X(GroundMeshComponent) X(HeroEntityComponent) X(PropertiesComponent) \
    X(SimpleGlowComponent) X(SpawnPointComponent) X(TextBubbleComponent) X(WeaponGlowComponent) \
    X(FireEmitterComponent) X(OverlayTextComponent) X(SoundEffectComponent) X(WeaponTrailComponent) \
    X(EntityActionComponent) X(PortalEffectComponent) X(ShadowVolumeComponent) X(UtilityShapeComponent) \
    X(GroundPolygonComponent) X(HookshotTrailComponent) X(MagicHookshotComponent) X(MonsterEntityComponent) \
    X(ParticleFieldComponent) X(PhysicsObjectComponent) X(BlendAnimationComponent) X(BushControllerComponent) \
    X(CharControllerComponent) X(CollisionShapeComponent) X(DimensionSpellComponent) X(DoorControllerComponent) \
    X(MagicExplosionComponent) X(MagicSpellCastComponent) X(ObjectModifierComponent) X(ParticleObjectComponent) \
    X(TextureMappingComponent) X(BreakableObjectComponent) X(CollectableItemComponent) X(DimensionObjectComponent) \
    X(OrbitControllerComponent) X(ParticleEmitterComponent) X(PhysicsPlatformComponent) X(PressureTriggerComponent) \
    X(SwingableWeaponComponent) X(EntityControllerComponent) X(KeyframeAnimationComponent) X(MonsterControllerComponent) \
    X(CharAnimControllerComponent) X(ElevatorControllerComponent) X(OverlayTargetArrowComponent) X(RotatingBackgroundComponent) \
    X(AnimationControllerComponent) X(GroundMeshGeneratorComponent) X(TransformControllerComponent) X(BatMonsterControllerComponent) \
    X(MagicParticleEmitterComponent) X(ObjectLinkControllerComponent) X(ProjectileControllerComponent) X(MonsterDeathControllerComponent) \
    X(SkellyMonsterControllerComponent) X(StaticMonsterControllerComponent) X(GenericMonsterControllerComponent) X(LeapingMonsterControllerComponent) \
    X(ModelTransformControllerComponent) X(WalkingMonsterControllerComponent) X(BouncingMonsterControllerComponent) X(ChargingMonsterControllerComponent) \
    X(ShootingMonsterControllerComponent) X(SnappingMonsterControllerComponent) X(SwingableWeaponControllerComponent) X(ProjectileMonsterControllerComponent) \
    X(BoneControlledCollisionShapeComponent)

#define SRE_EXTRAS_IFACE_DECL(name) extern void* name##_Interface;
SRE_EXTRAS_IFACE_LIST(SRE_EXTRAS_IFACE_DECL)

/* Resolve a component interface by class name, or NULL. */
void* sre_extras_find_interface(const char* name);

/* =========================================================================
 * Engine symbol resolution (host bridge import, same as libsre12.so).
 * ========================================================================= */
extern uint64_t sre_resolve_address(const char* symbol);

#ifdef __cplusplus
}
#endif

#endif /* SRE_EXTRAS_H */
