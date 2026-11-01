/* Raijin's SwKiwi "Mini.MemoryAddress" module — adapted for the
 * SwordigoDesktop PC port (SRE). Exposed to Lua as:
 *
 *   Mini.GetAddress(value)          -> MemoryAddress (raw pointer of a Lua value)
 *   Mini.GetComponentAddress(obj, "ComponentName") -> MemoryAddress | nil
 *   Mini.Dlsym("symbol")            -> MemoryAddress | nil
 *   Mini.Malloc(size)               -> MemoryAddress (zeroed heap block)
 *   addr:readInt8/16/32/64, readUInt*, readFloat/Double/Bool/Pointer/CString/CppString/Vector3
 *   addr:write*  addr:offset(n)  addr:getAddress()  addr:isNull()  addr:free()
 *   addr:call("pV*ib:v", ...)      -> Raijin signature FFI (see ffi.h)
 */

#include "sre_extras.h"
#include "ffi.h"
#include "sre_lua.h"          /* g_lua_* externs (redirected by sre_lua_compat.h) */

#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define LOG_TAG "KiwiMemory"
#define MEMORY_MT "Mini.MemoryAddress"

/* Reads a float out of a Lua table field (defined below). */
static float table_field_f(lua_State *L, int idx, const char *field);

typedef struct {
	void *ptr;
} MemoryAddress;

#define CHECK_ADDR(L, i) ((MemoryAddress*)sre_extras_check_addr(L, i))

/* luaL_checkudata replacement (avoids the engine's luaL_* aux functions). */
static void *sre_extras_check_addr(lua_State *L, int i) {
	if (!lua_isuserdata(L, i)) return NULL;
	void *ud = lua_touserdata(L, i);
	if (lua_getmetatable(L, i)) {
		lua_getfield(L, LUA_REGISTRYINDEX, MEMORY_MT);
		int eq = lua_rawequal(L, -1, -2);
		lua_pop(L, 2);
		if (!eq) return NULL;
	} else {
		return NULL;
	}
	return ud;
}

#define DEFINE_INT_RW(NAME, CTYPE) \
static int m_read##NAME(lua_State *L) { \
	MemoryAddress *addr = CHECK_ADDR(L, 1); \
	if (!addr) { lua_pushnil(L); return 1; } \
	lua_pushinteger(L, (lua_Integer)*(CTYPE *)addr->ptr); \
	return 1; \
} \
static int m_write##NAME(lua_State *L) { \
	MemoryAddress *addr = CHECK_ADDR(L, 1); \
	if (!addr) return 0; \
	*(CTYPE *)addr->ptr = (CTYPE)lua_tonumber(L, 2); \
	lua_settop(L, 1); \
	return 1; \
}

#define DEFINE_NUM_RW(NAME, CTYPE) \
static int m_read##NAME(lua_State *L) { \
	MemoryAddress *addr = CHECK_ADDR(L, 1); \
	if (!addr) { lua_pushnil(L); return 1; } \
	lua_pushnumber(L, (lua_Number)*(CTYPE *)addr->ptr); \
	return 1; \
} \
static int m_write##NAME(lua_State *L) { \
	MemoryAddress *addr = CHECK_ADDR(L, 1); \
	if (!addr) return 0; \
	*(CTYPE *)addr->ptr = (CTYPE)lua_tonumber(L, 2); \
	lua_settop(L, 1); \
	return 1; \
}

static MemoryAddress *push_addr(lua_State *L, void *ptr) {
	MemoryAddress *addr = (MemoryAddress*)lua_newuserdata(L, sizeof(*addr));
	if (!addr) return NULL;
	addr->ptr = ptr;

	lua_getfield(L, LUA_REGISTRYINDEX, MEMORY_MT);
	lua_setmetatable(L, -2);

	return addr;
}

static int GetAddress(lua_State *L) {
	const void *ptr = lua_topointer(L, 1);

	if (!ptr) {
		lua_pushnil(L);
		return 1;
	}

	push_addr(L, (void*)ptr);
	return 1;
}

static int Malloc(lua_State *L) {
	size_t size = (size_t)lua_tonumber(L, 1);
	if (size == 0) {
		push_addr(L, NULL);
		return 1;
	}

	void *ptr = malloc(size);
	if (!ptr) {
		lua_pushnil(L);
		return 1;
	}

	memset(ptr, 0, size);
	push_addr(L, ptr);
	return 1;
}

static int GetComponentAddress(lua_State *L) {
	/* Accept a lightuserdata SceneObject* (SRE style) or a "SceneObject"
	 * userdata (SwKiwi style). */
	void *obj = NULL;
	if (lua_islightuserdata(L, 1)) {
		obj = (void*)lua_touserdata(L, 1);
	} else if (lua_isuserdata(L, 1)) {
		void *block = lua_touserdata(L, 1);
		if (block) obj = *(void**)block;
	}
	const char *name = lua_tostring(L, 2);
	if (!obj || !name) {
		lua_pushnil(L);
		return 1;
	}

	void *iface = sre_extras_find_interface(name);
	if (!iface) {
		lua_pushnil(L);
		return 1;
	}

	/* SceneObject::ComponentWithInterface — guest vaddr supplied by the host
	 * in g_sre_extras_abi.component_with_interface_fn (resolved per engine
	 * version). Fallback: lazy bridge resolution of the 1.4.12/1.4.13 mangled
	 * symbol (identical in both binaries, verified via nm -D). */
	typedef void* (*pfn_cwi)(const void*, void*);
	static pfn_cwi s_component_with_interface = NULL;
	if (!s_component_with_interface) {
		uint64_t fn = g_sre_extras_abi.component_with_interface_fn;
		if (!fn)
			fn = sre_resolve_address("_ZNK5Caver11SceneObject22ComponentWithInterfaceEl");
		s_component_with_interface = (pfn_cwi)(uintptr_t)fn;
	}
	if (!s_component_with_interface) {
		lua_pushnil(L);
		return 1;
	}

	void *comp = s_component_with_interface(obj, iface);
	if (!comp) {
		lua_pushnil(L);
		return 1;
	}

	push_addr(L, comp);
	return 1;
}

static int Dlsym(lua_State *L) {
	const char *symbol = lua_tostring(L, 1);
	if (!symbol) {
		lua_pushnil(L);
		return 1;
	}

	uint64_t addr = sre_resolve_address(symbol);
	if (!addr) {
		lua_pushnil(L);
		return 1;
	}

	push_addr(L, (void*)(uintptr_t)addr);
	return 1;
}

static int m_free(lua_State *L) {
	MemoryAddress *addr = CHECK_ADDR(L, 1);
	if (addr && addr->ptr) {
		free(addr->ptr);
		addr->ptr = NULL;
	}
	return 0;
}

static int m_add(lua_State *L) {
	MemoryAddress *addr = CHECK_ADDR(L, 1);
	if (!addr) { lua_pushnil(L); return 1; }
	lua_Integer offset = (lua_Integer)lua_tonumber(L, 2);

	push_addr(L, (char*)addr->ptr + (ptrdiff_t)offset);
	return 1;
}

static int m_sub(lua_State *L) {
	MemoryAddress *addr = CHECK_ADDR(L, 1);
	if (!addr) { lua_pushnil(L); return 1; }

	if (lua_isuserdata(L, 2)) {
		MemoryAddress *other = CHECK_ADDR(L, 2);
		if (!other) { lua_pushnil(L); return 1; }

		lua_pushinteger(
			L,
			(lua_Integer)((char*)addr->ptr - (char*)other->ptr)
		);
		return 1;
	}

	lua_Integer offset = (lua_Integer)lua_tonumber(L, 2);

	push_addr(L, (char*)addr->ptr - (ptrdiff_t)offset);
	return 1;
}

static int m_eq(lua_State *L) {
	MemoryAddress *a = CHECK_ADDR(L, 1);
	MemoryAddress *b = CHECK_ADDR(L, 2);

	lua_pushboolean(L, (a && b) && (a->ptr == b->ptr));
	return 1;
}

static int m_tostring(lua_State *L) {
	MemoryAddress *addr = CHECK_ADDR(L, 1);

	/* No snprintf: the guest's variadic snprintf goes through the host
	 * bridge, which can't forward varargs (format string is copied
	 * verbatim — "%p" would print literally). Format the pointer by hand. */
	char buf[32];
	if (addr) {
		static const char hex[] = "0123456789abcdef";
		uintptr_t v = (uintptr_t)addr->ptr;
		buf[0] = '0'; buf[1] = 'x';
		int pos = 2;
		int started = 0;
		for (int shift = 60; shift >= 0; shift -= 4) {  /* full 64-bit */
			int nib = (int)((v >> shift) & 0xF);
			if (nib || started || shift == 0) {
				buf[pos++] = hex[nib];
				started = 1;
			}
		}
		buf[pos] = 0;
	} else {
		buf[0] = 'n'; buf[1] = 'i'; buf[2] = 'l'; buf[3] = 0;
	}

	lua_pushstring(L, buf);
	return 1;
}

static int m_offset(lua_State *L) {
	MemoryAddress *addr = CHECK_ADDR(L, 1);
	if (!addr) { lua_pushnil(L); return 1; }
	lua_Integer offset = (lua_Integer)lua_tonumber(L, 2);

	push_addr(L, (char*)addr->ptr + (ptrdiff_t)offset);
	return 1;
}

static int m_getAddress(lua_State *L) {
	MemoryAddress *addr = CHECK_ADDR(L, 1);
	lua_pushlightuserdata(L, addr ? addr->ptr : NULL);
	return 1;
}

static int m_isNull(lua_State *L) {
	MemoryAddress *addr = CHECK_ADDR(L, 1);
	lua_pushboolean(L, !addr || addr->ptr == NULL);
	return 1;
}

static int m_gc(lua_State *L) {
	(void)L;
	return 0;
}

/* booler */
static int m_readBool(lua_State *L) {
	MemoryAddress *addr = CHECK_ADDR(L, 1);
	if (!addr) { lua_pushnil(L); return 1; }
	lua_pushboolean(L, *(bool *)addr->ptr);
	return 1;
}

static int m_writeBool(lua_State *L) {
	MemoryAddress *addr = CHECK_ADDR(L, 1);
	if (!addr) return 0;
	*(bool *)addr->ptr = lua_toboolean(L, 2) != 0;
	lua_settop(L, 1);
	return 1;
}

/* signed */
DEFINE_INT_RW(Int8,  int8_t)
DEFINE_INT_RW(Int16, int16_t)
DEFINE_INT_RW(Int32, int32_t)
DEFINE_NUM_RW(Int64, int64_t)

/* unsigned */
DEFINE_INT_RW(UInt8,  uint8_t)
DEFINE_INT_RW(UInt16, uint16_t)
DEFINE_INT_RW(UInt32, uint32_t)
DEFINE_NUM_RW(UInt64, uint64_t)

/* floating point */
DEFINE_NUM_RW(Float,  float)
DEFINE_NUM_RW(Double, double)

static int m_readPointer(lua_State *L) {
	MemoryAddress *addr = CHECK_ADDR(L, 1);
	if (!addr) { lua_pushnil(L); return 1; }
	push_addr(L, *(void **)addr->ptr);
	return 1;
}

static int m_writePointer(lua_State *L) {
	MemoryAddress *addr = CHECK_ADDR(L, 1);
	MemoryAddress *value = CHECK_ADDR(L, 2);
	if (!addr || !value) return 0;

	*(void **)addr->ptr = value->ptr;

	lua_settop(L, 1);
	return 1;
}

static int m_readCString(lua_State *L) {
	MemoryAddress *addr = CHECK_ADDR(L, 1);
	if (!addr) { lua_pushnil(L); return 1; }

	const char *s = *(const char **)addr->ptr;

	if (s)
		lua_pushstring(L, s);
	else
		lua_pushnil(L);

	return 1;
}

static int m_writeCString(lua_State *L) {
	MemoryAddress *addr = CHECK_ADDR(L, 1);
	const char *s = lua_tostring(L, 2);
	if (!addr || !s) return 0;

	*(const char **)addr->ptr = s;

	lua_settop(L, 1);
	return 1;
}

static int m_readCppString(lua_State *L) {
	MemoryAddress *addr = CHECK_ADDR(L, 1);
	if (!addr) { lua_pushnil(L); return 1; }
	CppString *s = *(CppString **)addr->ptr;

	if (s) {
		/* The _Rep header precedes the char data by the ABI-configured length
		 * (24 for the GNU COW layout). Never a hardcoded literal. */
		const char* data = (const char*)s;
		const SreExtrasStringRep* rep =
			(const SreExtrasStringRep*)(data - g_sre_extras_abi.cppstring_rep_len);
		lua_pushlstring(L, data, rep->length);
	} else {
		lua_pushnil(L);
	}

	return 1;
}

static int m_writeCppString(lua_State *L) {
	MemoryAddress *addr = CHECK_ADDR(L, 1);
	const char *s = lua_tostring(L, 2);
	if (!addr || !s) return 0;

	CppString **slot = (CppString **)addr->ptr;
	CppString *old = *slot;

	/* Create the new string into the slot first, then release the old one --
	 * avoids a window where the slot holds a dangling/aliased pointer. */
	CppString_create(slot, s);
	if (old) CppString_release(old);

	lua_settop(L, 1);
	return 1;
}

/* Vector3 */

static int m_readVector3(lua_State *L) {
	MemoryAddress *addr = CHECK_ADDR(L, 1);
	if (!addr) { lua_pushnil(L); return 1; }
	Vector3 *v = (Vector3 *)addr->ptr;

	lua_newtable(L);
	lua_pushnumber(L, v->x); lua_setfield(L, -2, "x");
	lua_pushnumber(L, v->y); lua_setfield(L, -2, "y");
	lua_pushnumber(L, v->z); lua_setfield(L, -2, "z");
	return 1;
}

static int m_writeVector3(lua_State *L) {
	MemoryAddress *addr = CHECK_ADDR(L, 1);
	if (!addr || !lua_istable(L, 2)) return 0;

	Vector3 *v = (Vector3 *)addr->ptr;
	v->x = table_field_f(L, 2, "x");
	v->y = table_field_f(L, 2, "y");
	v->z = table_field_f(L, 2, "z");

	lua_settop(L, 1);
	return 1;
}

/* Reads a float out of a Lua table field, defaulting to 0 if absent/non-table. */
static float table_field_f(lua_State *L, int idx, const char *field) {
	if (!lua_istable(L, idx)) return 0.0f;
	lua_getfield(L, idx, field);
	float v = (float)lua_tonumber(L, -1);
	lua_pop(L, 1);
	return v;
}

static const luaL_Reg memory_methods[] = {
	/* metamethods */
	{"__gc",		m_gc},
	{"__add",	   m_add},
	{"__sub",	   m_sub},
	{"__eq",		m_eq},
	{"__tostring",  m_tostring},

	/* utility */
	{"offset",	  m_offset},
	{"getAddress",  m_getAddress},
	{"isNull",	  m_isNull},
	{"free", m_free},

	/* bool */
	{"readBool",	m_readBool},
	{"writeBool",   m_writeBool},

	/* signed */
	{"readInt8",	m_readInt8},
	{"writeInt8",   m_writeInt8},
	{"readInt16",   m_readInt16},
	{"writeInt16",  m_writeInt16},
	{"readInt32",   m_readInt32},
	{"writeInt32",  m_writeInt32},
	{"readInt64",   m_readInt64},
	{"writeInt64",  m_writeInt64},

	/* unsigned */
	{"readUInt8",   m_readUInt8},
	{"writeUInt8",  m_writeUInt8},
	{"readUInt16",  m_readUInt16},
	{"writeUInt16", m_writeUInt16},
	{"readUInt32",  m_readUInt32},
	{"writeUInt32", m_writeUInt32},
	{"readUInt64",  m_readUInt64},
	{"writeUInt64", m_writeUInt64},

	/* floating point */
	{"readFloat",   m_readFloat},
	{"writeFloat",  m_writeFloat},
	{"readDouble",  m_readDouble},
	{"writeDouble", m_writeDouble},

	/* pointer */
	{"readPointer",  m_readPointer},
	{"writePointer", m_writePointer},

	/* c strings */
	{"readCString",  m_readCString},
	{"writeCString", m_writeCString},

	/* C++ strings */
	{"readCppString",  m_readCppString},
	{"writeCppString", m_writeCppString},

	/* Vector3 */
	{"readVector3",  m_readVector3},
	{"writeVector3", m_writeVector3},

	/* Call — Raijin signature FFI */
	{"call",		ffi_lua_call},

	{NULL, NULL}
};

static const luaL_Reg memory_library[] = {
	{"GetAddress", GetAddress},
	{"GetComponentAddress", GetComponentAddress},
	{"Dlsym", Dlsym},
	{"Malloc", Malloc},
	{NULL, NULL}
};

int miniLL_open_memory(lua_State *L) {

	/* FFI is a closed-source feature: register the REAL libffi-backed _G.ffi
	 * table here (sre_ffi.c), overwriting the safe stub table that SRE core
	 * installed via sre_extras_stub_register_ffi(). Must run before the
	 * call_sig attach below so the table it extends is the real one. */
	extern void sre_ffi_register_lua(lua_State *L);
	sre_ffi_register_lua(L);

	/* Standalone Raijin signature FFI: ffi.call_sig(addr, "<args>:<ret>", ...).
	 * The real _G.ffi now exists (registered just above), so the table exists. */
	lua_getfield(L, LUA_GLOBALSINDEX, "ffi");
	if (lua_istable(L, -1)) {
		lua_pushcclosure(L, ffi_lua_call, 0);
		lua_setfield(L, -2, "call_sig");
	}
	lua_pop(L, 1);

	/* metatable */
	lua_getfield(L, LUA_REGISTRYINDEX, MEMORY_MT);
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		lua_newtable(L);

		for (int i = 0; memory_methods[i].name; i++) {
			lua_pushcclosure(L, memory_methods[i].func, 0);
			lua_setfield(L, -2, memory_methods[i].name);
		}

		/* mt.__index = mt — plain method lookup, no per-instance env needed. */
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");

		lua_pushvalue(L, -1);
		lua_setfield(L, LUA_REGISTRYINDEX, MEMORY_MT);
		/* Pop the metatable — the caller's Mini table stays on top so the
		 * library table below lands on the correct stack slot. */
		lua_pop(L, 1);
	} else {
		lua_pop(L, 1);
	}

	/* library table — stack is now [Mini, lib] */
	lua_newtable(L);

	for (int i = 0; memory_library[i].name; i++) {
		lua_pushcclosure(L, memory_library[i].func, 0);
		lua_setfield(L, -2, memory_library[i].name);
	}

	return 1;
}
