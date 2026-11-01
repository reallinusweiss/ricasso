/* fficonfig.h — vendored libffi configuration for aarch64-linux (guest).
 *
 * Hand-written for the libsre-extras guest build. Only the ffi_call /
 * ffi_prep_cif subset is used (FFI_CLOSURES is forced off in ffitarget.h),
 * so trampoline/mmap machinery is not needed. Allocator + libc symbols
 * (malloc, free, memcpy, abort, ...) are resolved at load time via the
 * SRE host bridge, exactly like the rest of the guest code.
 */

#ifndef SRE_EXTRAS_FFICONFIG_H
#define SRE_EXTRAS_FFICONFIG_H

/* Debugging */
/* #undef FFI_DEBUG */

/* Trampolines are disabled in this build (see ffitarget.h: FFI_CLOSURES 0). */
/* #undef FFI_EXEC_TRAMPOLINE_TABLE */
/* #undef FFI_MMAP_EXEC_EMUTRAMP_PAX */
/* #undef FFI_MMAP_EXEC_WRIT */

/* Raw API + structs are supported by the target. */
/* #undef FFI_NO_RAW_API */
/* #undef FFI_NO_STRUCTS */

/* aarch64 has real 128-bit long double. */
#define HAVE_LONG_DOUBLE 1
/* #undef HAVE_LONG_DOUBLE_VARIANT */

#define HAVE_ALLOCA 1
/* #undef HAVE_ALLOCA_H */

#define HAVE_AS_CFI_PSEUDO_OP 1
/* #undef HAVE_AS_REGISTER_PSEUDO_OP */
/* #undef HAVE_AS_S390_ZARCH */
/* #undef HAVE_AS_SPARC_UA_PCREL */
/* #undef HAVE_AS_X86_64_UNWIND_SECTION_TYPE */
/* #undef HAVE_AS_X86_PCREL */

#define HAVE_DLFCN_H 1
#define HAVE_HIDDEN_VISIBILITY_ATTRIBUTE 1
#define HAVE_INTTYPES_H 1
#define HAVE_MEMCPY 1
#define HAVE_MEMORY_H 1
#define HAVE_MKOSTEMP 1
#define HAVE_MMAP 1
#define HAVE_MMAP_ANON 1
#define HAVE_MMAP_DEV_ZERO 1
#define HAVE_MMAP_FILE 1
#define HAVE_RO_EH_FRAME 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_MMAN_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1

#define LIBFFI_GNU_SYMBOL_VERSIONING 1
#define LT_OBJDIR ".libs/"
/* #undef NO_MINUS_C_MINUS_O */

#define PACKAGE "libffi"
#define PACKAGE_BUGREPORT "http://github.com/libffi/libffi/issues"
#define PACKAGE_NAME "libffi"
#define PACKAGE_STRING "libffi 3.3-rc0"
#define PACKAGE_TARNAME "libffi"
#define PACKAGE_URL ""
#define PACKAGE_VERSION "3.3-rc0"

#define SIZEOF_DOUBLE 8
#define SIZEOF_LONG_DOUBLE 16
#define SIZEOF_SIZE_T 8
/* #undef STACK_DIRECTION */
#define STDC_HEADERS 1
/* #undef SYMBOL_UNDERSCORE */
/* #undef USING_PURIFY */

#define VERSION "3.3-rc0"

/* aarch64-linux is little-endian. */
/* #undef WORDS_BIGENDIAN */

/* FFFI_HIDDEN visibility helper (matches configure-generated header). */
#ifdef HAVE_HIDDEN_VISIBILITY_ATTRIBUTE
#ifdef LIBFFI_ASM
#ifdef __APPLE__
#define FFI_HIDDEN(name) .private_extern name
#else
#define FFI_HIDDEN(name) .hidden name
#endif
#else
#define FFI_HIDDEN __attribute__ ((visibility ("hidden")))
#endif
#else
#ifdef LIBFFI_ASM
#define FFI_HIDDEN(name)
#else
#define FFI_HIDDEN
#endif
#endif

#endif /* SRE_EXTRAS_FFICONFIG_H */
