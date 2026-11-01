#ifndef SRE_EXTRAS_FFI_H
#define SRE_EXTRAS_FFI_H

/* Raijin's SwKiwi FFI, adapted for the SwordigoDesktop PC port (SRE).
 * Dispatch is backed by vendored libffi (aarch64) compiled into this
 * module, so arbitrary ABI-correct calls work (structs by value, mixed
 * int/float args, pointer returns, ...). */

#include "sre_extras.h"
#include <stdbool.h>

#define FFI_MAX_ARGS 8

typedef enum {
	FFI_TYPE_VOID,
	FFI_TYPE_INT,
	FFI_TYPE_FLOAT,
	FFI_TYPE_DOUBLE,
	FFI_TYPE_BOOL,
	FFI_TYPE_POINTER,
	FFI_TYPE_INT64,
	FFI_TYPE_VECTOR3,
	FFI_TYPE_VECTOR2,
	FFI_TYPE_QUATERNION,
	FFI_TYPE_MATRIX4,
	FFI_TYPE_FLOATCOLOR,
	FFI_TYPE_RECTANGLE,
	FFI_TYPE_CPPSTRING,
} ffi_type_t;

typedef struct {
	ffi_type_t arg_types[FFI_MAX_ARGS];
	bool       arg_is_ptr[FFI_MAX_ARGS]; /* true => arg is "T*", passed/read as a pointer to T */
	int        arg_count;
	ffi_type_t ret_type;
	bool       ret_is_ptr;
} ffi_signature_t;

/**
 * Signature grammar: "<args>:<ret>". Each type char may be followed by '*' meaning
 * "pointer to type" instead of "by value" (ABI-wise this always degrades to a plain
 * pointer slot, regardless of the base type - only the marshal/push semantics differ).
 *
 * Type chars: v=void i=int l=int64 f=float d=double b=bool p=pointer
 *             V=Vector3 2=Vector2 Q=Quaternion M=Matrix4 C=FloatColor R=Rectangle
 *             S=CppString (Lua string <-> C++ std::string wrapper, always passed by pointer)
 *
 * Examples:
 *   "pV*ib:v"  -> (void *this, Vector3 *location, int, bool) -> void
 *   "v*Vi*:v"  -> (void *, Vector3 (by value), int *) -> void
 */
ffi_signature_t ffi_parse_signature(const char *sig);

/** Calls func_ptr per sig, reading Lua args from the stack starting at arg_base.
 *  Pushes the return value (if ret_type != FFI_TYPE_VOID) and returns the Lua result count. */
int caver_ffi_dispatch(lua_State *L, void *func_ptr, ffi_signature_t sig, int arg_base);

/**
 * Generic Lua entry point: fn(funcAddr, sigString, ...args).
 * funcAddr may be a MemoryAddress userdata, light userdata, or nil.
 * Bind this as both a MemoryAddress:call(sig, ...) method and a standalone library function.
 */
int ffi_lua_call(lua_State *L);

/** Shared pointer-extraction helper (MemoryAddress userdata / light userdata / nil -> void*). */
void *ffi_extract_ptr(lua_State *L, int idx);

#endif /* SRE_EXTRAS_FFI_H */
