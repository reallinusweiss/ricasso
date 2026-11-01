/* ============================================================
 * sre_ffi.c — Native FFI Lua module for SRE (GNU GPLv3 / extras)
 * ============================================================
 * This is the UPGRADED FFI engine, moved out of SRE core and now
 * backed by the vendored libffi (libffi/ dir). Dispatch is ABI-correct
 * for every call shape libffi supports (mixed int/float args, pointer
 * returns, arbitrary arg counts, and — unlike the old hand-rolled core
 * version — it is portable to x86_64, not just ARM64).
 *
 * It registers the base _G.ffi table (`sre_ffi_register_lua`). Raijin's
 * signature API (raijin_ffi.c) is layered on top separately and is not
 * touched here.
 *
 * Lua API — 100% source-compatible with the old core sre_ffi.c, plus a
 * set of advanced/risky additions (see section "Advanced FFI").
 *
 *   ffi.call(addr, ret_type, arg_types, ...)
 *   ffi.bind(addr, ret_type, arg_types) -> closure
 *   ffi.peek8/16/32/64/f, ffi.peekstr
 *   ffi.poke8/16/32/64/f
 *   ffi.readf32/f64/i32/i64, ffi.writef32/i32
 *   ffi.offset, ffi.deref, ffi.null, ffi.base, ffi.at
 *   ffi.memcpy, ffi.memset, ffi.typeof
 *
 * Type string chars (unchanged from core):
 *   v=void  b=bool  c=i8  h=i16  i=i32  l=i64
 *   f=f32   d=f64   p=ptr  s=cstr
 *
 * The freestanding (-nostdlib) build resolves libc/Lua/syscall symbols
 * at load time via the SRE host bridge, exactly like the rest of extras.
 * All lua_* names are redirected to g_lua_* by sre_lua_compat.h (force
 * included by the build), so we use the plain lua_* spelling here.
 * ============================================================ */

#include "sre_extras.h"
#include "sre_lua.h"          /* g_lua_* externs (redirected by sre_lua_compat.h) */
#include <ffi.h>              /* vendored libffi — see libffi/ */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Base address of libswordigo.so in guest space. In extras this is
 * captured by sre_extras_init() into g_sre_extras_swordigo_base. */
extern uint64_t g_sre_extras_swordigo_base;

/* =========================================================================
 * Type system — same enum/codes as the original core file.
 * ========================================================================= */
#define SRE_FFI_MAX_ARGS 16

typedef enum {
    FFI_T_VOID = 0,
    FFI_T_BOOL = 1,
    FFI_T_I8   = 2,
    FFI_T_I16  = 3,
    FFI_T_I32  = 4,
    FFI_T_I64  = 5,
    FFI_T_F32  = 6,
    FFI_T_F64  = 7,
    FFI_T_PTR  = 8,
    FFI_T_STR  = 9,
} SreFfiType;

static SreFfiType ffi_type_from_char(char c) {
    switch (c) {
        case 'v': return FFI_T_VOID;
        case 'b': return FFI_T_BOOL;
        case 'c': return FFI_T_I8;
        case 'h': return FFI_T_I16;
        case 'i': return FFI_T_I32;
        case 'l': return FFI_T_I64;
        case 'f': return FFI_T_F32;
        case 'd': return FFI_T_F64;
        case 'p': return FFI_T_PTR;
        case 's': return FFI_T_STR;
        default:  return FFI_T_PTR;
    }
}

static SreFfiType ffi_parse_ret_type(const char* s) {
    if (!s || !s[0]) return FFI_T_VOID;
    if (!s[1]) return ffi_type_from_char(s[0]);
    static const char* kws[] = { "void","bool","i8","i16","i32","i64","f32","f64","ptr","str" };
    static const SreFfiType kwtypes[] = { FFI_T_VOID,FFI_T_BOOL,FFI_T_I8,FFI_T_I16,FFI_T_I32,
                                          FFI_T_I64,FFI_T_F32,FFI_T_F64,FFI_T_PTR,FFI_T_STR };
    for (int i = 0; i < 10; i++)
        if (strcmp(kws[i], s) == 0) return kwtypes[i];
    return FFI_T_VOID;
}

/* Map our type enum to a libffi ffi_type*. */
static ffi_type* ffi_libffi_type(SreFfiType t) {
    switch (t) {
        case FFI_T_VOID: return &ffi_type_void;
        case FFI_T_BOOL: return &ffi_type_uint8;
        case FFI_T_I8:   return &ffi_type_sint8;
        case FFI_T_I16:  return &ffi_type_sint16;
        case FFI_T_I32:  return &ffi_type_sint32;
        case FFI_T_I64:  return &ffi_type_sint64;
        case FFI_T_F32:  return &ffi_type_float;
        case FFI_T_F64:  return &ffi_type_double;
        case FFI_T_PTR:  return &ffi_type_pointer;
        case FFI_T_STR:  return &ffi_type_pointer;
        default:         return &ffi_type_pointer;
    }
}

/* =========================================================================
 * Argument storage for a single dispatch. Each argument gets its own
 * typed storage slot so libffi can take stable pointers to them.
 * ========================================================================= */
typedef union {
    uint8_t  u8;
    int8_t   i8;
    int16_t  i16;
    int32_t  i32;
    int64_t  i64;
    float    f32;
    double   f64;
    void*    ptr;
} SreFfiArgVal;

typedef struct {
    SreFfiType    types[SRE_FFI_MAX_ARGS];
    SreFfiArgVal  vals[SRE_FFI_MAX_ARGS];
    void*         ptrs[SRE_FFI_MAX_ARGS];  /* &vals[i] */
    ffi_type*     ftypes[SRE_FFI_MAX_ARGS];
    int           n;
} SreFfiArgs;

/* g_errno mirror — updated after each dispatch. */
static int s_ffi_errno = 0;

/* Fill argument storage from the Lua stack starting at base_idx. */
static void ffi_fill_args(lua_State* L, SreFfiArgs* a,
                          const char* types_str, int base_idx) {
    a->n = 0;
    if (!types_str) return;

    for (int i = 0; types_str[i] && a->n < SRE_FFI_MAX_ARGS; i++) {
        SreFfiType t = ffi_type_from_char(types_str[i]);
        int slot = a->n;
        int li   = base_idx + i;

        a->types[slot]  = t;
        a->ftypes[slot] = ffi_libffi_type(t);

        switch (t) {
            case FFI_T_VOID:
                a->ftypes[slot] = &ffi_type_pointer;
                a->vals[slot].ptr = 0;
                break;
            case FFI_T_BOOL:
                a->vals[slot].u8 = (uint8_t)(lua_toboolean(L, li) ? 1 : 0);
                break;
            case FFI_T_I8:
                a->vals[slot].i8 = (int8_t)(int64_t)lua_tonumber(L, li);
                break;
            case FFI_T_I16:
                a->vals[slot].i16 = (int16_t)(int64_t)lua_tonumber(L, li);
                break;
            case FFI_T_I32:
                a->vals[slot].i32 = (int32_t)(int64_t)lua_tonumber(L, li);
                break;
            case FFI_T_I64:
                a->vals[slot].i64 = (int64_t)lua_tonumber(L, li);
                break;
            case FFI_T_F32:
                a->vals[slot].f32 = (float)lua_tonumber(L, li);
                break;
            case FFI_T_F64:
                a->vals[slot].f64 = (double)lua_tonumber(L, li);
                break;
            case FFI_T_PTR: {
                if (lua_type(L, li) == LUA_TLIGHTUSERDATA)
                    a->vals[slot].ptr = lua_touserdata(L, li);
                else
                    a->vals[slot].ptr = (void*)(uintptr_t)(int64_t)lua_tonumber(L, li);
                break;
            }
            case FFI_T_STR:
                a->vals[slot].ptr = (void*)(uintptr_t)lua_tolstring(L, li, 0);
                break;
        }
        a->ptrs[slot] = &a->vals[slot];
        a->n++;
    }
}

/* Push a libffi return value onto the Lua stack. */
static int ffi_push_result(lua_State* L, SreFfiType ret,
                           const SreFfiArgVal* rv) {
    switch (ret) {
        case FFI_T_VOID:
            return 0;
        case FFI_T_BOOL:
            lua_pushboolean(L, (int)(rv->u8 & 1));
            return 1;
        case FFI_T_I8:
            lua_pushnumber(L, (double)(int8_t)rv->i64);
            return 1;
        case FFI_T_I16:
            lua_pushnumber(L, (double)(int16_t)rv->i64);
            return 1;
        case FFI_T_I32:
            lua_pushnumber(L, (double)(int32_t)rv->i64);
            return 1;
        case FFI_T_I64:
            lua_pushnumber(L, (double)(int64_t)rv->i64);
            return 1;
        case FFI_T_F32:
            lua_pushnumber(L, (double)rv->f32);
            return 1;
        case FFI_T_F64:
            lua_pushnumber(L, rv->f64);
            return 1;
        case FFI_T_PTR:
            lua_pushnumber(L, (double)(uint64_t)(uintptr_t)rv->ptr);
            return 1;
        case FFI_T_STR: {
            const char* s = (const char*)rv->ptr;
            if (s) lua_pushstring(L, s);
            else   lua_pushnil(L);
            return 1;
        }
    }
    return 0;
}

/* Core libffi dispatch. Returns 1 if prep/call succeeded, 0 otherwise. */
static int ffi_do_call(void* fn, SreFfiType ret_type,
                       SreFfiArgs* a, SreFfiArgVal* out) {
    ffi_cif  cif;
    ffi_type* rtype = ffi_libffi_type(ret_type);

    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, (unsigned)a->n,
                     rtype, a->ftypes) != FFI_OK) {
        s_ffi_errno = -1;
        return 0;
    }

    /* libffi widens integer/bool/small returns to ffi_arg; give it a slot
     * at least that wide, then narrow on push. */
    union {
        ffi_arg      wide;
        float        f32;
        double       f64;
        void*        ptr;
        SreFfiArgVal val;
    } r;
    r.wide = 0;

    ffi_call(&cif, FFI_FN(fn), &r, a->ptrs);

    switch (ret_type) {
        case FFI_T_VOID: break;
        case FFI_T_F32:  out->f32 = r.f32; break;
        case FFI_T_F64:  out->f64 = r.f64; break;
        case FFI_T_PTR:
        case FFI_T_STR:  out->ptr = r.ptr; break;
        default:         out->i64 = (int64_t)(ffi_sarg)r.wide; out->u8 = (uint8_t)r.wide; break;
    }
    s_ffi_errno = 0;
    return 1;
}

/* =========================================================================
 * ffi.call(addr, ret_type, arg_types, ...)
 * ========================================================================= */
static int l_ffi_call(lua_State* L) {
    uint64_t fn_addr = (uint64_t)(int64_t)lua_tonumber(L, 1);
    if (!fn_addr) return 0;

    const char* ret_str  = lua_tostring(L, 2);
    const char* args_str = lua_tostring(L, 3);
    SreFfiType  ret_type = ffi_parse_ret_type(ret_str);

    SreFfiArgs a;
    ffi_fill_args(L, &a, args_str, 4);

    SreFfiArgVal out;
    memset(&out, 0, sizeof(out));
    if (!ffi_do_call((void*)(uintptr_t)fn_addr, ret_type, &a, &out))
        return 0;

    return ffi_push_result(L, ret_type, &out);
}

/* =========================================================================
 * ffi.bind(addr, ret_type, arg_types) -> closure
 * ========================================================================= */
#define SRE_FFI_CLOSURE_MAX 64
typedef struct {
    uint64_t   fn_addr;
    SreFfiType ret_type;
    char       arg_types[SRE_FFI_MAX_ARGS + 1];
} SreFfiClosure;

static SreFfiClosure s_closures[SRE_FFI_CLOSURE_MAX];
static int           s_closures_used = 0;

static int l_ffi_closure_call(lua_State* L) {
    SreFfiClosure* cl = (SreFfiClosure*)lua_touserdata(L, lua_upvalueindex(1));
    if (!cl || !cl->fn_addr) return 0;

    SreFfiArgs a;
    ffi_fill_args(L, &a, cl->arg_types, 1);

    SreFfiArgVal out;
    memset(&out, 0, sizeof(out));
    if (!ffi_do_call((void*)(uintptr_t)cl->fn_addr, cl->ret_type, &a, &out))
        return 0;

    return ffi_push_result(L, cl->ret_type, &out);
}

static int l_ffi_bind(lua_State* L) {
    uint64_t fn_addr = (uint64_t)(int64_t)lua_tonumber(L, 1);
    const char* ret_str  = lua_tostring(L, 2);
    const char* args_str = lua_tostring(L, 3);

    if (s_closures_used >= SRE_FFI_CLOSURE_MAX) return 0;

    SreFfiClosure* cl = &s_closures[s_closures_used++];
    cl->fn_addr  = fn_addr;
    cl->ret_type = ffi_parse_ret_type(ret_str);

    int i = 0;
    if (args_str)
        for (; args_str[i] && i < SRE_FFI_MAX_ARGS; i++)
            cl->arg_types[i] = args_str[i];
    cl->arg_types[i] = '\0';

    lua_pushlightuserdata(L, cl);
    lua_pushcclosure(L, l_ffi_closure_call, 1);
    return 1;
}

/* =========================================================================
 * Peek / Poke helpers
 * ========================================================================= */
static inline uint64_t ffi_lua_addr(lua_State* L, int idx) {
    return (uint64_t)(int64_t)lua_tonumber(L, idx);
}

static int l_ffi_peek8(lua_State* L) {
    uint64_t a = ffi_lua_addr(L, 1);
    if (!a) { lua_pushnil(L); return 1; }
    lua_pushnumber(L, (double)(*(uint8_t*)(uintptr_t)a));
    return 1;
}
static int l_ffi_peek16(lua_State* L) {
    uint64_t a = ffi_lua_addr(L, 1);
    if (!a) { lua_pushnil(L); return 1; }
    lua_pushnumber(L, (double)(*(uint16_t*)(uintptr_t)a));
    return 1;
}
static int l_ffi_peek32(lua_State* L) {
    uint64_t a = ffi_lua_addr(L, 1);
    if (!a) { lua_pushnil(L); return 1; }
    lua_pushnumber(L, (double)(*(uint32_t*)(uintptr_t)a));
    return 1;
}
static int l_ffi_peek64(lua_State* L) {
    uint64_t a = ffi_lua_addr(L, 1);
    if (!a) { lua_pushnil(L); return 1; }
    lua_pushnumber(L, (double)(*(uint64_t*)(uintptr_t)a));
    return 1;
}
static int l_ffi_peekf(lua_State* L) {
    uint64_t a = ffi_lua_addr(L, 1);
    if (!a) { lua_pushnil(L); return 1; }
    lua_pushnumber(L, (double)(*(float*)(uintptr_t)a));
    return 1;
}
static int l_ffi_peekstr(lua_State* L) {
    uint64_t a = ffi_lua_addr(L, 1);
    if (!a) { lua_pushnil(L); return 1; }
    const char* s = *(const char**)(uintptr_t)a;
    if (s) lua_pushstring(L, s);
    else   lua_pushnil(L);
    return 1;
}

static int l_ffi_poke8(lua_State* L) {
    uint64_t a = ffi_lua_addr(L, 1); if (!a) return 0;
    *(uint8_t*)(uintptr_t)a = (uint8_t)(int64_t)lua_tonumber(L, 2);
    return 0;
}
static int l_ffi_poke16(lua_State* L) {
    uint64_t a = ffi_lua_addr(L, 1); if (!a) return 0;
    *(uint16_t*)(uintptr_t)a = (uint16_t)(int64_t)lua_tonumber(L, 2);
    return 0;
}
static int l_ffi_poke32(lua_State* L) {
    uint64_t a = ffi_lua_addr(L, 1); if (!a) return 0;
    *(uint32_t*)(uintptr_t)a = (uint32_t)(int64_t)lua_tonumber(L, 2);
    return 0;
}
static int l_ffi_poke64(lua_State* L) {
    uint64_t a = ffi_lua_addr(L, 1); if (!a) return 0;
    *(uint64_t*)(uintptr_t)a = (uint64_t)(int64_t)lua_tonumber(L, 2);
    return 0;
}
static int l_ffi_pokef(lua_State* L) {
    uint64_t a = ffi_lua_addr(L, 1); if (!a) return 0;
    *(float*)(uintptr_t)a = (float)lua_tonumber(L, 2);
    return 0;
}

/* =========================================================================
 * Struct field helpers
 * ========================================================================= */
static int l_ffi_readf32(lua_State* L) {
    uint64_t b = ffi_lua_addr(L, 1);
    int64_t  o = (int64_t)lua_tonumber(L, 2);
    if (!b) { lua_pushnil(L); return 1; }
    lua_pushnumber(L, (double)(*(float*)(uintptr_t)(b + o)));
    return 1;
}
static int l_ffi_readf64(lua_State* L) {
    uint64_t b = ffi_lua_addr(L, 1);
    int64_t  o = (int64_t)lua_tonumber(L, 2);
    if (!b) { lua_pushnil(L); return 1; }
    lua_pushnumber(L, *(double*)(uintptr_t)(b + o));
    return 1;
}
static int l_ffi_readi32(lua_State* L) {
    uint64_t b = ffi_lua_addr(L, 1);
    int64_t  o = (int64_t)lua_tonumber(L, 2);
    if (!b) { lua_pushnil(L); return 1; }
    lua_pushnumber(L, (double)(*(int32_t*)(uintptr_t)(b + o)));
    return 1;
}
static int l_ffi_readi64(lua_State* L) {
    uint64_t b = ffi_lua_addr(L, 1);
    int64_t  o = (int64_t)lua_tonumber(L, 2);
    if (!b) { lua_pushnil(L); return 1; }
    lua_pushnumber(L, (double)(*(int64_t*)(uintptr_t)(b + o)));
    return 1;
}
static int l_ffi_writef32(lua_State* L) {
    uint64_t b = ffi_lua_addr(L, 1); if (!b) return 0;
    int64_t  o = (int64_t)lua_tonumber(L, 2);
    *(float*)(uintptr_t)(b + o) = (float)lua_tonumber(L, 3);
    return 0;
}
static int l_ffi_writei32(lua_State* L) {
    uint64_t b = ffi_lua_addr(L, 1); if (!b) return 0;
    int64_t  o = (int64_t)lua_tonumber(L, 2);
    *(int32_t*)(uintptr_t)(b + o) = (int32_t)(int64_t)lua_tonumber(L, 3);
    return 0;
}

/* =========================================================================
 * Offset / Utility helpers
 * ========================================================================= */
static int l_ffi_offset(lua_State* L) {
    int n = lua_gettop(L);
    uint64_t addr = (uint64_t)(int64_t)lua_tonumber(L, 1);
    for (int i = 2; i <= n; i++)
        addr += (uint64_t)(int64_t)lua_tonumber(L, i);
    lua_pushnumber(L, (double)addr);
    return 1;
}
static int l_ffi_deref(lua_State* L) {
    uint64_t a = ffi_lua_addr(L, 1);
    if (!a) { lua_pushnumber(L, 0.0); return 1; }
    lua_pushnumber(L, (double)(*(uint64_t*)(uintptr_t)a));
    return 1;
}
static int l_ffi_null(lua_State* L) {
    lua_pushnumber(L, 0.0);
    return 1;
}
static int l_ffi_base(lua_State* L) {
    lua_pushnumber(L, (double)g_sre_extras_swordigo_base);
    return 1;
}
static int l_ffi_at(lua_State* L) {
    uint64_t offset = (uint64_t)(int64_t)lua_tonumber(L, 1);
    lua_pushnumber(L, (double)(g_sre_extras_swordigo_base + offset));
    return 1;
}
static int l_ffi_memcpy(lua_State* L) {
    uint64_t dst = ffi_lua_addr(L, 1);
    uint64_t src = ffi_lua_addr(L, 2);
    uint64_t n   = (uint64_t)(int64_t)lua_tonumber(L, 3);
    if (dst && src && n) {
        volatile char* d = (volatile char*)(uintptr_t)dst;
        const    char* s = (const    char*)(uintptr_t)src;
        for (uint64_t i = 0; i < n; i++) d[i] = s[i];
    }
    return 0;
}
static int l_ffi_memset(lua_State* L) {
    uint64_t dst = ffi_lua_addr(L, 1);
    int      val = (int)(int64_t)lua_tonumber(L, 2);
    uint64_t n   = (uint64_t)(int64_t)lua_tonumber(L, 3);
    if (dst && n) {
        volatile char* d = (volatile char*)(uintptr_t)dst;
        for (uint64_t i = 0; i < n; i++) d[i] = (char)val;
    }
    return 0;
}
static int l_ffi_typeof(lua_State* L) {
    int t = lua_type(L, 1);
    switch (t) {
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

/* =========================================================================
 * Advanced FFI (LuaJIT-inspired) — low risk
 * ========================================================================= */

/* Byte width of a named simple type (also used by sizeof/alignment). */
static int ffi_type_width(const char* name) {
    if (!name) return 0;
    if (!strcmp(name, "i8")  || !strcmp(name, "u8")  || !strcmp(name, "bool")) return 1;
    if (!strcmp(name, "i16") || !strcmp(name, "u16")) return 2;
    if (!strcmp(name, "i32") || !strcmp(name, "u32") || !strcmp(name, "f32") ||
        !strcmp(name, "float") || !strcmp(name, "int")) return 4;
    if (!strcmp(name, "i64") || !strcmp(name, "u64") || !strcmp(name, "f64") ||
        !strcmp(name, "double") || !strcmp(name, "ptr") || !strcmp(name, "pointer")) return 8;
    /* Known Raijin structs (sizes match sre_extras.h layout). */
    if (!strcmp(name, "Vector2"))    return 8;
    if (!strcmp(name, "Vector3"))    return 12;
    if (!strcmp(name, "Quaternion")) return 16;
    if (!strcmp(name, "FloatColor")) return 16;
    if (!strcmp(name, "Rectangle"))  return 16;
    if (!strcmp(name, "Matrix4"))    return 64;
    return 0;
}

/* ffi.sizeof(typename) -> bytes | nil */
static int l_ffi_sizeof(lua_State* L) {
    int w = ffi_type_width(lua_tostring(L, 1));
    if (w) lua_pushnumber(L, (double)w);
    else   lua_pushnil(L);
    return 1;
}

/* ffi.alignment(typename) -> alignment (== width for our simple types) */
static int l_ffi_alignment(lua_State* L) {
    const char* n = lua_tostring(L, 1);
    int w = ffi_type_width(n);
    if (!w) { lua_pushnil(L); return 1; }
    /* Composite types align to their largest scalar member (f32 -> 4). */
    if (n && (!strcmp(n, "Vector2") || !strcmp(n, "Vector3") ||
              !strcmp(n, "Quaternion") || !strcmp(n, "FloatColor") ||
              !strcmp(n, "Rectangle") || !strcmp(n, "Matrix4")))
        w = 4;
    lua_pushnumber(L, (double)w);
    return 1;
}

/* ffi.cast(typename, value) -> value reinterpreted. For scalar/ptr only;
 * we normalize to a number the caller can feed back into peek/poke/call. */
static int l_ffi_cast(lua_State* L) {
    const char* ty = lua_tostring(L, 1);
    double v = lua_tonumber(L, 2);
    if (!ty) { lua_pushnumber(L, v); return 1; }
    int64_t iv = (int64_t)v;
    if (!strcmp(ty, "i8"))   { lua_pushnumber(L, (double)(int8_t)iv);  return 1; }
    if (!strcmp(ty, "u8"))   { lua_pushnumber(L, (double)(uint8_t)iv); return 1; }
    if (!strcmp(ty, "i16"))  { lua_pushnumber(L, (double)(int16_t)iv); return 1; }
    if (!strcmp(ty, "u16"))  { lua_pushnumber(L, (double)(uint16_t)iv);return 1; }
    if (!strcmp(ty, "i32") || !strcmp(ty, "int"))
                             { lua_pushnumber(L, (double)(int32_t)iv); return 1; }
    if (!strcmp(ty, "u32"))  { lua_pushnumber(L, (double)(uint32_t)iv);return 1; }
    if (!strcmp(ty, "f32") || !strcmp(ty, "float"))
                             { lua_pushnumber(L, (double)(float)v);    return 1; }
    /* i64 / u64 / ptr / f64 default: pass through */
    lua_pushnumber(L, v);
    return 1;
}

/* ffi.new(typename [, count]) -> zeroed heap address | nil.
 * Uses libc malloc/memset resolved by the host bridge. */
extern void* malloc(unsigned long);
extern void  free(void*);
extern void* memset(void*, int, unsigned long);

static int l_ffi_new(lua_State* L) {
    int w = ffi_type_width(lua_tostring(L, 1));
    if (!w) { lua_pushnil(L); return 1; }
    int64_t count = (lua_type(L, 2) == LUA_TNUMBER) ? (int64_t)lua_tonumber(L, 2) : 1;
    if (count < 1) count = 1;
    unsigned long total = (unsigned long)w * (unsigned long)count;
    void* p = malloc(total);
    if (!p) { lua_pushnil(L); return 1; }
    memset(p, 0, total);
    lua_pushnumber(L, (double)(uint64_t)(uintptr_t)p);
    return 1;
}

/* ffi.alloc(size) -> zeroed heap address | nil */
static int l_ffi_alloc(lua_State* L) {
    unsigned long size = (unsigned long)(int64_t)lua_tonumber(L, 1);
    if (!size) { lua_pushnil(L); return 1; }
    void* p = malloc(size);
    if (!p) { lua_pushnil(L); return 1; }
    memset(p, 0, size);
    lua_pushnumber(L, (double)(uint64_t)(uintptr_t)p);
    return 1;
}

/* ffi.free(addr) */
static int l_ffi_free(lua_State* L) {
    uint64_t a = ffi_lua_addr(L, 1);
    if (a) free((void*)(uintptr_t)a);
    return 0;
}

/* ffi.addr(ptr, offset) -> ptr + offset (pure pointer arithmetic). */
static int l_ffi_addr(lua_State* L) {
    uint64_t p   = ffi_lua_addr(L, 1);
    int64_t  off = (int64_t)lua_tonumber(L, 2);
    lua_pushnumber(L, (double)(uint64_t)(p + off));
    return 1;
}

/* ffi.copy(dst, src, n) — LuaJIT-compatible alias for memcpy. */
static int l_ffi_copy(lua_State* L)  { return l_ffi_memcpy(L); }
/* ffi.fill(dst, n, val) — note LuaJIT arg order (dst, n, val). */
static int l_ffi_fill(lua_State* L) {
    uint64_t dst = ffi_lua_addr(L, 1);
    uint64_t n   = (uint64_t)(int64_t)lua_tonumber(L, 2);
    int      val = (int)(int64_t)lua_tonumber(L, 3);
    if (dst && n) {
        volatile char* d = (volatile char*)(uintptr_t)dst;
        for (uint64_t i = 0; i < n; i++) d[i] = (char)val;
    }
    return 0;
}

/* ffi.string(addr [, len]) -> Lua string read from memory. */
static int l_ffi_string(lua_State* L) {
    uint64_t a = ffi_lua_addr(L, 1);
    if (!a) { lua_pushnil(L); return 1; }
    const char* s = (const char*)(uintptr_t)a;
    if (lua_type(L, 2) == LUA_TNUMBER) {
        int len = (int)lua_tonumber(L, 2);
        if (len < 0) len = 0;
        lua_pushlstring(L, s, (size_t)len);
    } else {
        lua_pushstring(L, s);
    }
    return 1;
}
/* ffi.tostring(addr) -> null-terminated string (alias of string w/o len). */
static int l_ffi_tostring(lua_State* L) {
    uint64_t a = ffi_lua_addr(L, 1);
    if (!a) { lua_pushnil(L); return 1; }
    lua_pushstring(L, (const char*)(uintptr_t)a);
    return 1;
}

/* ffi.tonumber(v) / ffi.tobool(v) — pure Lua conversions. */
static int l_ffi_tonumber(lua_State* L) { lua_pushnumber(L, lua_tonumber(L, 1)); return 1; }
static int l_ffi_tobool(lua_State* L)   { lua_pushboolean(L, lua_toboolean(L, 1)); return 1; }

/* ffi.errno() -> last dispatch status (0 ok, -1 prep failure). */
static int l_ffi_errno(lua_State* L) { lua_pushnumber(L, (double)s_ffi_errno); return 1; }

/* ffi.abi(query) -> boolean/string describing this build's ABI. */
static int l_ffi_abi(lua_State* L) {
    const char* q = lua_tostring(L, 1);
    if (!q) {
#if defined(__aarch64__)
        lua_pushstring(L, "arm64");
#elif defined(__x86_64__)
        lua_pushstring(L, "x86_64");
#else
        lua_pushstring(L, "unknown");
#endif
        return 1;
    }
    int r = 0;
    if (!strcmp(q, "64bit")) r = (sizeof(void*) == 8);
    else if (!strcmp(q, "le")) { uint16_t x = 1; r = (*(uint8_t*)&x == 1); }
    else if (!strcmp(q, "be")) { uint16_t x = 1; r = (*(uint8_t*)&x == 0); }
#if defined(__aarch64__)
    else if (!strcmp(q, "arm64")) r = 1;
#elif defined(__x86_64__)
    else if (!strcmp(q, "x86_64") || !strcmp(q, "x64")) r = 1;
#endif
    lua_pushboolean(L, r);
    return 1;
}

/* Hand-rolled hex formatting.
 *
 * IMPORTANT: the guest's variadic printf/snprintf go through the host JNI
 * bridge, which copies the format string VERBATIM and drops the varargs
 * (see src/jni/jni_bridge.cpp and memory.c's m_tostring). So "%x"/"%llx"
 * print literally and are useless for addresses. We format hex by hand and
 * emit via fputs (a non-varargs bridged call that forwards the string). This
 * is also why ffi.hex() exists — string.format("%x", n) is broken port-wide.
 * fputs()/stdout come from <stdio.h> (pulled in via sre_lua_compat.h).
 */
static const char SFFI_HEXD[] = "0123456789abcdef";

/* Write value v as fixed-width hex (width nibbles) into buf at *pos. */
static void sffi_hex_fixed(char* buf, int* pos, uint64_t v, int width) {
    for (int shift = (width - 1) * 4; shift >= 0; shift -= 4)
        buf[(*pos)++] = SFFI_HEXD[(v >> shift) & 0xF];
}

/* ffi.hex(number [, width]) -> "0x..." string. Works around the broken
 * string.format("%x", n) on this port. Default width = minimal. */
static int l_ffi_hex(lua_State* L) {
    uint64_t v = (uint64_t)(int64_t)lua_tonumber(L, 1);
    int width  = (lua_type(L, 2) == LUA_TNUMBER) ? (int)lua_tonumber(L, 2) : 0;
    if (width < 0)  width = 0;
    if (width > 16) width = 16;
    char buf[24];
    int pos = 0;
    buf[pos++] = '0'; buf[pos++] = 'x';
    if (width > 0) {
        sffi_hex_fixed(buf, &pos, v, width);
    } else {
        int started = 0;
        for (int shift = 60; shift >= 0; shift -= 4) {
            int nib = (int)((v >> shift) & 0xF);
            if (nib || started || shift == 0) { buf[pos++] = SFFI_HEXD[nib]; started = 1; }
        }
    }
    buf[pos] = 0;
    lua_pushstring(L, buf);
    return 1;
}

/* ffi.dump(addr, n) — hex dump to the host log (debug aid). Builds each line
 * by hand (no varargs) and emits it via the bridged fputs. */
static int l_ffi_dump(lua_State* L) {
    uint64_t a = ffi_lua_addr(L, 1);
    int      n = (int)(int64_t)lua_tonumber(L, 2);
    if (!a || n <= 0) return 0;
    const uint8_t* p = (const uint8_t*)(uintptr_t)a;
    for (int i = 0; i < n; i += 16) {
        char line[96];
        int pos = 0;
        sffi_hex_fixed(line, &pos, a + i, 16);   /* 16-nibble address */
        line[pos++] = ':'; line[pos++] = ' ';
        for (int j = 0; j < 16 && i + j < n; j++) {
            uint8_t b = p[i + j];
            line[pos++] = SFFI_HEXD[(b >> 4) & 0xF];
            line[pos++] = SFFI_HEXD[b & 0xF];
            line[pos++] = ' ';
        }
        line[pos++] = '\n';
        line[pos]   = 0;
        fputs(line, stdout);
    }
    return 0;
}

/* ffi.search(start, end, pattern) — byte-pattern search; returns first
 * matching address or nil. Medium risk (reads a memory range). */
static int l_ffi_search(lua_State* L) {
    uint64_t start = ffi_lua_addr(L, 1);
    uint64_t end   = ffi_lua_addr(L, 2);
    size_t   plen  = 0;
    const char* pat = lua_tolstring(L, 3, &plen);
    if (!start || end <= start || !pat || plen == 0) { lua_pushnil(L); return 1; }
    const uint8_t* base = (const uint8_t*)(uintptr_t)start;
    uint64_t span = end - start;
    if (span < plen) { lua_pushnil(L); return 1; }
    for (uint64_t i = 0; i + plen <= span; i++) {
        if (memcmp(base + i, pat, plen) == 0) {
            lua_pushnumber(L, (double)(uint64_t)(start + i));
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

/* =========================================================================
 * Risky mode — patch / seal / unseal / load require explicit opt-in.
 * ========================================================================= */
static int s_risky_mode = 0;

/* ffi.risky_mode([bool]) -> current state. Enables the dangerous ops. */
static int l_ffi_risky_mode(lua_State* L) {
    if (lua_gettop(L) >= 1 && lua_type(L, 1) != LUA_TNIL)
        s_risky_mode = lua_toboolean(L, 1) ? 1 : 0;
    lua_pushboolean(L, s_risky_mode);
    return 1;
}

/* mprotect resolved via host bridge (freestanding build). */
#ifndef PROT_READ
#define PROT_READ  0x1
#define PROT_WRITE 0x2
#define PROT_EXEC  0x4
#endif
extern int mprotect(void*, unsigned long, int);

static uint64_t page_align_down(uint64_t a) { return a & ~((uint64_t)0xFFF); }

/* ffi.patch(addr, bytes) — overwrite instruction/data bytes. Very high risk. */
static int l_ffi_patch(lua_State* L) {
    if (!s_risky_mode) { lua_pushboolean(L, 0); return 1; }
    uint64_t a = ffi_lua_addr(L, 1);
    size_t   n = 0;
    const char* bytes = lua_tolstring(L, 2, &n);
    if (!a || !bytes || n == 0) { lua_pushboolean(L, 0); return 1; }
    volatile char* d = (volatile char*)(uintptr_t)a;
    for (size_t i = 0; i < n; i++) d[i] = bytes[i];
    lua_pushboolean(L, 1);
    return 1;
}

/* ffi.seal(addr, size) — make memory read+execute. Very high risk. */
static int l_ffi_seal(lua_State* L) {
    if (!s_risky_mode) { lua_pushboolean(L, 0); return 1; }
    uint64_t a = ffi_lua_addr(L, 1);
    uint64_t n = (uint64_t)(int64_t)lua_tonumber(L, 2);
    if (!a || !n) { lua_pushboolean(L, 0); return 1; }
    uint64_t page = page_align_down(a);
    uint64_t len  = (a - page) + n;
    int r = mprotect((void*)(uintptr_t)page, (unsigned long)len, PROT_READ | PROT_EXEC);
    lua_pushboolean(L, r == 0);
    return 1;
}

/* ffi.unseal(addr, size) — make memory read+write. Very high risk. */
static int l_ffi_unseal(lua_State* L) {
    if (!s_risky_mode) { lua_pushboolean(L, 0); return 1; }
    uint64_t a = ffi_lua_addr(L, 1);
    uint64_t n = (uint64_t)(int64_t)lua_tonumber(L, 2);
    if (!a || !n) { lua_pushboolean(L, 0); return 1; }
    uint64_t page = page_align_down(a);
    uint64_t len  = (a - page) + n;
    int r = mprotect((void*)(uintptr_t)page, (unsigned long)len, PROT_READ | PROT_WRITE);
    lua_pushboolean(L, r == 0);
    return 1;
}

/* ffi.load(libname) — dlopen a shared library. High risk; risky-mode gated.
 * dlopen/dlsym are resolved by the host bridge; if unavailable this returns
 * nil. Returns the handle address on success. */
extern void* dlopen(const char*, int);
#ifndef RTLD_NOW
#define RTLD_NOW 0x2
#endif
static int l_ffi_load(lua_State* L) {
    if (!s_risky_mode) { lua_pushnil(L); return 1; }
    const char* name = lua_tostring(L, 1);
    if (!name) { lua_pushnil(L); return 1; }
    void* h = dlopen(name, RTLD_NOW);
    if (!h) { lua_pushnil(L); return 1; }
    lua_pushnumber(L, (double)(uint64_t)(uintptr_t)h);
    return 1;
}

/* =========================================================================
 * Register _G.ffi — real, libffi-backed table (overwrites the core stub).
 * Called by the extras merge (miniLL_open_memory) when libsre-extras loads.
 * ========================================================================= */
__attribute__((visibility("default")))
void sre_ffi_register_lua(lua_State* L) {
    lua_createtable(L, 0, 48);

#define FFI_REG(name, fn) \
    lua_pushcclosure(L, fn, 0); \
    lua_setfield(L, -2, name)

    /* --- Basic (moved from core, now libffi-backed) --- */
    FFI_REG("call",     l_ffi_call);
    FFI_REG("bind",     l_ffi_bind);

    FFI_REG("peek8",    l_ffi_peek8);
    FFI_REG("peek16",   l_ffi_peek16);
    FFI_REG("peek32",   l_ffi_peek32);
    FFI_REG("peek64",   l_ffi_peek64);
    FFI_REG("peekf",    l_ffi_peekf);
    FFI_REG("peekstr",  l_ffi_peekstr);

    FFI_REG("poke8",    l_ffi_poke8);
    FFI_REG("poke16",   l_ffi_poke16);
    FFI_REG("poke32",   l_ffi_poke32);
    FFI_REG("poke64",   l_ffi_poke64);
    FFI_REG("pokef",    l_ffi_pokef);

    FFI_REG("readf32",  l_ffi_readf32);
    FFI_REG("readf64",  l_ffi_readf64);
    FFI_REG("readi32",  l_ffi_readi32);
    FFI_REG("readi64",  l_ffi_readi64);
    FFI_REG("writef32", l_ffi_writef32);
    FFI_REG("writei32", l_ffi_writei32);

    FFI_REG("offset",   l_ffi_offset);
    FFI_REG("deref",    l_ffi_deref);
    FFI_REG("null",     l_ffi_null);
    FFI_REG("base",     l_ffi_base);
    FFI_REG("at",       l_ffi_at);
    FFI_REG("memcpy",   l_ffi_memcpy);
    FFI_REG("memset",   l_ffi_memset);
    FFI_REG("typeof",   l_ffi_typeof);

    /* --- Advanced (new; low/medium risk) --- */
    FFI_REG("sizeof",    l_ffi_sizeof);
    FFI_REG("alignment", l_ffi_alignment);
    FFI_REG("cast",      l_ffi_cast);
    FFI_REG("new",       l_ffi_new);
    FFI_REG("alloc",     l_ffi_alloc);
    FFI_REG("free",      l_ffi_free);
    FFI_REG("addr",      l_ffi_addr);
    FFI_REG("copy",      l_ffi_copy);
    FFI_REG("fill",      l_ffi_fill);
    FFI_REG("string",    l_ffi_string);
    FFI_REG("tostring",  l_ffi_tostring);
    FFI_REG("tonumber",  l_ffi_tonumber);
    FFI_REG("tobool",    l_ffi_tobool);
    FFI_REG("errno",     l_ffi_errno);
    FFI_REG("abi",       l_ffi_abi);
    FFI_REG("hex",       l_ffi_hex);
    FFI_REG("dump",      l_ffi_dump);
    FFI_REG("search",    l_ffi_search);

    /* --- Risky (require ffi.risky_mode(true)) --- */
    FFI_REG("risky_mode", l_ffi_risky_mode);
    FFI_REG("patch",      l_ffi_patch);
    FFI_REG("seal",       l_ffi_seal);
    FFI_REG("unseal",     l_ffi_unseal);
    FFI_REG("load",       l_ffi_load);

#undef FFI_REG

    lua_setfield(L, LUA_GLOBALSINDEX, "ffi");
}
