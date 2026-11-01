/* sre_extras_init.c — libsre-extras (GNU GPLv3 / extras)
 *
 * Module entry point. The host loads libsre-extras.so into guest RAM,
 * fills a SreExtrasInit struct in guest memory (Lua API pointers from the
 * engine's dynsym, same resolution as libsre12.so), then calls
 * sre_extras_init(&struct). All g_lua_* globals live in THIS module.
 */

#include "sre_extras.h"
#include "sre_lua.h"          /* g_lua_* externs + pfn typedefs */
#include <string.h>
#include <stdlib.h>

/* =========================================================================
 * g_lua_* globals — filled from SreExtrasInit::lua by sre_extras_init().
 * ========================================================================= */
pfn_lua_settop       g_lua_settop       = 0;
pfn_lua_gettop       g_lua_gettop       = 0;
pfn_lua_tolstring    g_lua_tolstring    = 0;
pfn_lua_pushstring   g_lua_pushstring   = 0;
pfn_lua_pushcclosure g_lua_pushcclosure = 0;
pfn_lua_setfield     g_lua_setfield     = 0;
pfn_lua_getfield     g_lua_getfield     = 0;
pfn_lua_createtable  g_lua_createtable  = 0;
pfn_lua_pushnumber   g_lua_pushnumber   = 0;
pfn_lua_pushboolean  g_lua_pushboolean  = 0;
pfn_lua_pushnil      g_lua_pushnil      = 0;
pfn_lua_tonumber     g_lua_tonumber     = 0;
pfn_lua_toboolean    g_lua_toboolean    = 0;
pfn_lua_type         g_lua_type         = 0;
pfn_lua_touserdata   g_lua_touserdata   = 0;
pfn_lua_topointer    g_lua_topointer    = 0;
pfn_lua_pushlightuserdata g_lua_pushlightuserdata = 0;
pfn_lua_error        g_lua_error        = 0;

/* Extended */
pfn_lua_pushvalue    g_lua_pushvalue    = 0;
pfn_lua_rawgeti      g_lua_rawgeti      = 0;
pfn_lua_rawseti      g_lua_rawseti      = 0;
pfn_lua_settable     g_lua_settable     = 0;
pfn_lua_gettable     g_lua_gettable     = 0;
pfn_lua_pushinteger  g_lua_pushinteger  = 0;
pfn_lua_pushlstring  g_lua_pushlstring  = 0;
pfn_lua_setmetatable g_lua_setmetatable = 0;
pfn_lua_getmetatable g_lua_getmetatable = 0;
pfn_lua_rawequal     g_lua_rawequal     = 0;
pfn_lua_isuserdata   g_lua_isuserdata   = 0;

/* Extras-only additions */
pfn_ex_lua_newuserdata g_lua_newuserdata = 0;

/* =========================================================================
 * Component interface globals — use SRE's extern declarations from sre_caver.h.
 * The SRE_EXTRAS_IFACE_DECL macro in sre_extras.h declares them as extern,
 * and the dynamic linker resolves them to libsre12.so's definitions.
 * NO local definitions here — avoids 97 duplicate globals.
 * ========================================================================= */
typedef struct {
    const char* name;
    void**      slot;
} SreExtrasIfaceEntry;

static const SreExtrasIfaceEntry s_interface_table[] = {
#define SRE_EXTRAS_IFACE_ENTRY(name) { #name, &name##_Interface },
SRE_EXTRAS_IFACE_LIST(SRE_EXTRAS_IFACE_ENTRY)
    { NULL, NULL }
};

void* sre_extras_find_interface(const char* name) {
    if (!name) return NULL;
    for (int i = 0; s_interface_table[i].name; i++) {
        if (strcmp(s_interface_table[i].name, name) == 0)
            return *s_interface_table[i].slot;
    }
    return NULL;
}

/* =========================================================================
 * CppString helpers — GNU libstdc++ COW std::string (see sre_extras.h).
 * ========================================================================= */
int CppString_create(CppString** out, const char* s) {
    if (!out) return 0;
    *out = NULL;
    if (!s) s = "";

    size_t len = strlen(s);
    SreExtrasStringRep* rep = (SreExtrasStringRep*)malloc(sizeof(SreExtrasStringRep) + len + 1);
    if (!rep) return 0;

    rep->length   = len;
    rep->capacity = len;
    rep->refcount = 0;   /* owned by the caller (not shared) */
    rep->_pad     = 0;

    char* data = (char*)(rep + 1);
    memcpy(data, s, len);
    data[len] = '\0';

    *out = (CppString*)data;
    return 1;
}

void CppString_release(CppString* cs) {
    if (!cs) return;
    SreExtrasStringRep* rep = ADJ(cs);
    free(rep);
}

/* =========================================================================
 * ABI config — version-dependent offsets, supplied by the host (checker).
 * ========================================================================= */
SreExtrasAbi g_sre_extras_abi = {
    0,   /* swordi_abi            */
    0,   /* cppstring_data_off    */
    24,  /* cppstring_rep_len     */
    0,   /* _reserved             */
    0    /* component_with_interface_fn */
};

/* Known-good layouts (GNU libstdc++ COW, both engine ABIs). */
#define SRE_EXTRAS_CPPSTR_DATA_OFF_KNOWN 0u
#define SRE_EXTRAS_CPPSTR_REP_LEN_KNOWN  24u

int sre_extras_abi_validate(const SreExtrasAbi* in, SreExtrasAbi* out) {
    if (!out) return 1;
    if (!in) in = &g_sre_extras_abi;

    *out = *in;
    int fixed = 0;

    /* Only the GNU COW layout is implemented — clamp anything else. */
    if (out->cppstring_data_off != SRE_EXTRAS_CPPSTR_DATA_OFF_KNOWN) {
        out->cppstring_data_off = SRE_EXTRAS_CPPSTR_DATA_OFF_KNOWN;
        fixed++;
    }
    if (out->cppstring_rep_len != SRE_EXTRAS_CPPSTR_REP_LEN_KNOWN) {
        out->cppstring_rep_len = SRE_EXTRAS_CPPSTR_REP_LEN_KNOWN;
        fixed++;
    }
    if (out->swordi_abi != 12 && out->swordi_abi != 13) {
        out->swordi_abi = 12;   /* safe default */
        fixed++;
    }
    return fixed;
}

const char* sre_extras_cppstring_data(const void* str_obj) {
    if (!str_obj) return NULL;
    /* The char-data pointer lives at g_sre_extras_abi.cppstring_data_off
     * inside the string object (0 for the GNU COW layout). Reading a pointer
     * out of guest memory is safe here: str_obj is an engine std::string*
     * handed to us by a hooked engine function. */
    const char* data;
    if (g_sre_extras_abi.cppstring_data_off == 0) {
        data = *(const char* const*)str_obj;
    } else {
        data = *(const char* const*)((const char*)str_obj + g_sre_extras_abi.cppstring_data_off);
    }
    return data;
}

int sre_extras_abi_tag(void) {
    return (int)g_sre_extras_abi.swordi_abi;
}

/* =========================================================================
 * Init — called by the host.
 * ========================================================================= */
uint64_t g_sre_extras_swordigo_base = 0;

void sre_extras_init(const SreExtrasInit* init) {
    if (!init) return;

    g_sre_extras_swordigo_base = init->swordigo_base;

    /* ABI config: validate the host-supplied offsets, then publish to the
     * module global so every consumer (mod_saves, memory) reads the same
     * corrected values instead of a hardcoded literal. */
    {
        SreExtrasAbi validated;
        int fixed = sre_extras_abi_validate(&init->abi, &validated);
        g_sre_extras_abi = validated;
        if (fixed) {
            /* Host has no variadic-safe log here; flag via the ABI tag. */
            g_sre_extras_abi._reserved = (uint32_t)fixed;
        }
    }

    g_lua_settop       = init->lua.lua_settop;
    g_lua_gettop       = init->lua.lua_gettop;
    g_lua_tolstring    = init->lua.lua_tolstring;
    g_lua_pushstring   = init->lua.lua_pushstring;
    g_lua_pushcclosure = init->lua.lua_pushcclosure;
    g_lua_setfield     = init->lua.lua_setfield;
    g_lua_getfield     = init->lua.lua_getfield;
    g_lua_createtable  = init->lua.lua_createtable;
    g_lua_pushnumber   = init->lua.lua_pushnumber;
    g_lua_pushboolean  = init->lua.lua_pushboolean;
    g_lua_pushnil      = init->lua.lua_pushnil;
    g_lua_tonumber     = init->lua.lua_tonumber;
    g_lua_toboolean    = init->lua.lua_toboolean;
    g_lua_type         = init->lua.lua_type;
    g_lua_touserdata   = init->lua.lua_touserdata;
    g_lua_topointer    = init->lua.lua_topointer;
    g_lua_pushlightuserdata = init->lua.lua_pushlightuserdata;
    g_lua_error        = init->lua.lua_error;

    g_lua_pushvalue    = init->lua.lua_pushvalue;
    g_lua_rawgeti      = init->lua.lua_rawgeti;
    g_lua_rawseti      = init->lua.lua_rawseti;
    g_lua_settable     = init->lua.lua_settable;
    g_lua_gettable     = init->lua.lua_gettable;
    g_lua_pushinteger  = init->lua.lua_pushinteger;
    g_lua_pushlstring  = init->lua.lua_pushlstring;
    g_lua_setmetatable = init->lua.lua_setmetatable;
    g_lua_getmetatable = init->lua.lua_getmetatable;
    g_lua_rawequal     = init->lua.lua_rawequal;
    g_lua_isuserdata   = init->lua.lua_isuserdata;
    g_lua_newuserdata  = init->lua.lua_newuserdata;
}

int sre_extras_version(void) {
    return 1;
}
