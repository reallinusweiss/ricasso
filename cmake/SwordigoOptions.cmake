# ============================================================================
# SwordigoOptions.cmake — all user-facing build options and cache paths.
# ============================================================================

option(SWORDIGO_USE_DYNARMIC "Build and link the Dynarmic ARM64 JIT" ON)
option(SWORDIGO_USE_FFMPEG "Link static FFmpeg (MP4 background videos)" ON)
option(SWORDIGO_BUILD_SRE "Build the ARM64 guest libsre.so" ON)
option(SWORDIGO_BUILD_RUBY "Build the asset viewer" ON)
option(SWORDIGO_BUILD_RUBY_GG "Build ruby_gg (Xpera native editor frontend)" ON)
option(SWORDIGO_BUILD_STUDIO "Build swordigo_build_studio GUI launcher" ON)
option(SWORDIGO_CLIENT_ONLY "Build the original ELF/JIT game client without editor tools" OFF)
if (SWORDIGO_CLIENT_ONLY)
    set(SWORDIGO_STATIC ON CACHE BOOL "Use one host executable" FORCE)
    set(SWORDIGO_BUILD_RUBY OFF CACHE BOOL "" FORCE)
    set(SWORDIGO_BUILD_RUBY_GG OFF CACHE BOOL "" FORCE)
    set(SWORDIGO_BUILD_STUDIO OFF CACHE BOOL "" FORCE)
    set(SWORDIGO_USE_FFMPEG OFF CACHE BOOL "" FORCE)
endif()
# CMAKE MODE DEV — the single switch that turns the developer test suite on.
# OFF = normal/release build: no test target is declared and nothing lands in
# <dist>/tests/. ON = declare every test target and emit the binaries into
# <dist>/tests/ (bin/tests/ on Linux, binw/tests/ on Windows) so the app's
# bin/ stays clean. See the `CMAKE MODE DEV` banner in the root CMakeLists.txt
# for the full rationale and the configure/build/ctest commands.
option(SWORDIGO_DEV_MODE "CMAKE MODE DEV: build the test suite into <dist>/tests/ (OFF = normal build, no tests)" OFF)
option(SWORDIGO_STATIC "Build components as static libraries (single-PE app; avoids cross-DLL globals)" OFF)
# Opt-in: strip symbols from our own executables/libraries at link time for a
# smaller distributable. OFF by default so normal builds keep debug symbols.
option(SWORDIGO_STRIP_RELEASE "Strip symbols from Swordigo binaries (smaller, for distribution)" OFF)
set(SWORDIGO_FFMPEG_ROOT "${CMAKE_SOURCE_DIR}/src/tools/ffmpeg/build" CACHE PATH "Local static FFmpeg prefix")
set(SWORDIGO_DYNARMIC_ROOT "${CMAKE_SOURCE_DIR}/deps/dynarmic/build" CACHE PATH "Dynarmic build directory")
