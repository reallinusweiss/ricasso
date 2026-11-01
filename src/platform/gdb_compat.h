#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Registers an ELF shared library with GDB/LLDB using glibc's _r_debug rendezvous protocol.
// filepath: path to .so file on disk
// load_base: host virtual base address where segments are loaded
// dynamic_section: pointer to .dynamic section
void gdb_register_elf(const char* filepath, uintptr_t load_base, void* dynamic_section);

#ifdef __cplusplus
}
#endif
