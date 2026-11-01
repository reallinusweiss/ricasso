#include "platform/gdb_compat.h"
#include <iostream>
#include <cstdlib>
#include <cstring>

#ifndef _WIN32
#include <elf.h>
#include <link.h>

#if defined(_r_debug)
static struct r_debug* g_r_debug_ptr = &_r_debug;
#else
extern ElfW(Dyn) _DYNAMIC[] __attribute__((weak));
static struct r_debug* g_r_debug_ptr = nullptr;

static void init_r_debug() {
    if (g_r_debug_ptr) return;
    if (_DYNAMIC) {
        for (int i = 0; _DYNAMIC[i].d_tag != DT_NULL; i++) {
            if (_DYNAMIC[i].d_tag == DT_DEBUG) {
                g_r_debug_ptr = (struct r_debug*)_DYNAMIC[i].d_un.d_ptr;
                break;
            }
        }
    }
}
#endif
#endif

extern "C" void gdb_register_elf(const char* filepath, uintptr_t load_base, void* dynamic_section) {
#ifndef _WIN32
#if !defined(_r_debug)
    init_r_debug();
#endif
    if (!g_r_debug_ptr) return;

    struct link_map* map = (struct link_map*)std::calloc(1, sizeof(struct link_map));
    if (!map) return;

    map->l_addr = (ElfW(Addr))load_base;
    map->l_name = strdup(filepath ? filepath : "");
    map->l_ld = (ElfW(Dyn)*)dynamic_section;

    // Notify GDB: adding a new library
    g_r_debug_ptr->r_state = r_debug::RT_ADD;
    if (g_r_debug_ptr->r_brk) {
        ((void (*)(void))g_r_debug_ptr->r_brk)();
    }

    // Append to link_map list
    if (!g_r_debug_ptr->r_map) {
        g_r_debug_ptr->r_map = map;
    } else {
        struct link_map* tail = g_r_debug_ptr->r_map;
        while (tail->l_next) {
            tail = tail->l_next;
        }
        tail->l_next = map;
        map->l_prev = tail;
    }

    // Notify GDB: mapping is consistent
    g_r_debug_ptr->r_state = r_debug::RT_CONSISTENT;
    if (g_r_debug_ptr->r_brk) {
        ((void (*)(void))g_r_debug_ptr->r_brk)();
    }

    std::cout << "[GDB] Registered ELF with _r_debug: " << (filepath ? filepath : "unknown")
              << " @ 0x" << std::hex << load_base << std::dec << std::endl;
#else
    (void)filepath;
    (void)load_base;
    (void)dynamic_section;
#endif
}
