# ============================================================================
# libsre-extras.so — optional SRE addon, ABI-aware (ARM64 guest module:
# Mini.MemoryAddress + Raijin signature FFI). Served to BOTH Swordigo
# 1.4.12 (libsre12.so) and 1.4.13 (libsre13.so): version-dependent offsets /
# addresses are supplied per engine version by the host through
# SreExtrasInit::abi and validated by sre_extras_abi_validate() inside the
# module (see sre_extras.h — nothing is hardcoded anymore).
#
# Builds SEPARATELY from the main project on purpose:
#   * The host (ruby/swordfare) works fine without it — SRE falls back to
#     stub memory/ffi Lua APIs (sre12: sre_extras_stubs.c; sre13:
#     sre13_extras_stubs.c).
#   * When present next to the SRE library (bin/libs/libsre-extras.so), the
#     host loads it for whichever ABI is running and SRE uses the real
#     Mini.MemoryAddress + Raijin FFI.
#
# Usage (standalone):
#   cmake -S . -B build-sre-extras
#   cmake --build build-sre-extras
# Output: bin/libs/libsre-extras.so
# ============================================================================
# NOTE: the host loads this module for whichever ABI is running (main.cpp,
# SRE-Extras block — the old `swordi_abi == 12` gate is gone). To gate
# per-version availability later (e.g. extras only for 1.4.12), add a
# swordi_abi check in main.cpp before the load.
cmake_minimum_required(VERSION 3.16)
project(sre_extras LANGUAGES C ASM)

set(CMAKE_C_STANDARD 11)

# This project sits at src/sre/extras — the SRE tree root is one level up.
set(SRE_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(EXTRA_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# Output next to libsre12.so so the host finds both.
set(OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../bin/libs")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")

find_program(AARCH64_CC aarch64-linux-gnu-gcc)
if (NOT AARCH64_CC)
    message(FATAL_ERROR "aarch64-linux-gnu-gcc not found — install the cross toolchain")
endif ()

# Same freestanding flags as the SRE guest libs (undefined libc/Lua symbols
# are resolved at load time by the SRE host bridge). Version code (sre_lua.h
# etc.) lives in sre/sre12, shared infra in sre/base.
set(EXTRA_COMMON_FLAGS
    -shared -fPIC -O2 -nostdlib -fno-builtin -fno-stack-protector
    -D_GNU_SOURCE -D_DEFAULT_SOURCE
    -I${SRE_SRC_DIR}/sre12
    -I${SRE_SRC_DIR}/base
    -I${SRE_SRC_DIR}/base/include
    -I${SRE_SRC_DIR}/base/lua/src
    -I${EXTRA_SRC_DIR}/libffi
    -include ${SRE_SRC_DIR}/base/sre_lua_compat.h
)

set(EXTRA_SOURCES
    ${EXTRA_SRC_DIR}/sre_extras_init.c
    ${EXTRA_SRC_DIR}/memory.c
    # sre_ffi.c — upgraded, libffi-backed generic FFI engine (moved from SRE
    # core). Registers the real _G.ffi table. raijin_ffi.c layers Raijin's
    # signature API on top.
    ${EXTRA_SRC_DIR}/sre_ffi.c
    # raijin_ffi.c — Raijin's signature-based dispatch (renamed from ffi.c).
    ${EXTRA_SRC_DIR}/raijin_ffi.c
    ${EXTRA_SRC_DIR}/mod_fs.c
    ${EXTRA_SRC_DIR}/mod_button_hooks.c
    ${EXTRA_SRC_DIR}/mod_saves.c
    ${EXTRA_SRC_DIR}/libffi/prep_cif.c
    ${EXTRA_SRC_DIR}/libffi/types.c
    ${EXTRA_SRC_DIR}/libffi/ffi.c
    ${EXTRA_SRC_DIR}/libffi/sysv.S
)

set(EXTRA_OBJECTS)
foreach (source IN LISTS EXTRA_SOURCES)
    file(RELATIVE_PATH relative "${EXTRA_SRC_DIR}" "${source}")
    string(REPLACE "/" "_" object_name "${relative}")
    string(REGEX REPLACE "\\.(c|S)$" ".o" object_name "${object_name}")
    set(object "${CMAKE_CURRENT_BINARY_DIR}/${object_name}")
    add_custom_command(OUTPUT "${object}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}"
        COMMAND ${AARCH64_CC} ${EXTRA_COMMON_FLAGS} -c "${source}" -o "${object}"
        DEPENDS "${source}" VERBATIM)
    list(APPEND EXTRA_OBJECTS "${object}")
endforeach ()

add_custom_command(OUTPUT "${OUTPUT_DIR}/libsre-extras.so"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}"
    COMMAND ${AARCH64_CC} -shared -fPIC -nostdlib
            -o "${OUTPUT_DIR}/libsre-extras.so" ${EXTRA_OBJECTS}
    DEPENDS ${EXTRA_OBJECTS} VERBATIM)

add_custom_target(sre_extras ALL DEPENDS "${OUTPUT_DIR}/libsre-extras.so")
