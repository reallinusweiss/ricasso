# ============================================================================
# SwordigoDeps.cmake — external dependency discovery. Required on all platforms
# unless noted. FFmpeg stays local/static; vorbisfile + mpg123 are pkg-config
# only on non-Windows (Windows builds link their own copies or skip audio).
# ============================================================================

include(GNUInstallDirs)
if (NOT WIN32)
    find_package(PkgConfig REQUIRED)
endif()
find_package(OpenGL REQUIRED)
if (WIN32)
    # GLEW supplies the modern (1.2+) OpenGL entry points that opengl32.lib does
    # not export; every host component links it via swordigo_target().
    find_package(GLEW REQUIRED)
    set(SWORDIGO_GLEW_TARGET GLEW::GLEW)
endif()
find_package(ZLIB REQUIRED)
find_package(OpenAL REQUIRED)
find_package(Threads REQUIRED)
find_package(Vulkan QUIET)
# When the full Vulkan loader isn't present (e.g. MinGW cross build with only
# the vcpkg vulkan-headers port), still expose the headers-only target so the
# VK_NO_PROTOTYPES ImGui backend can build without hard-linking a loader.
if (NOT Vulkan_FOUND)
    find_package(VulkanHeaders CONFIG QUIET)
endif()
find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_image CONFIG QUIET)
if (TARGET SDL3_image::SDL3_image-shared)
    set(SDL3_IMAGE_LINK SDL3_image::SDL3_image-shared)
elseif (TARGET SDL3_image::SDL3_image-static)
    set(SDL3_IMAGE_LINK SDL3_image::SDL3_image-static)
elseif (TARGET SDL3_image::SDL3_image)
    set(SDL3_IMAGE_LINK SDL3_image::SDL3_image)
else()
    pkg_check_modules(SDL3_IMAGE REQUIRED SDL3_image)
    set(SDL3_IMAGE_LINK ${SDL3_IMAGE_LIBRARIES} ${SDL3_IMAGE_LDFLAGS})
endif()

if (NOT WIN32)
    pkg_check_modules(VORBISFILE REQUIRED vorbisfile)
    pkg_check_modules(MPG123 REQUIRED libmpg123)
else()
    # Windows: resolve audio backends through vcpkg's CONFIG packages so the
    # headers (vorbis/vorbisfile.h, mpg123.h) and import libs are wired in.
    find_package(Vorbis CONFIG QUIET)
    find_package(MPG123 CONFIG QUIET)
    if (TARGET Vorbis::vorbisfile AND TARGET MPG123::libmpg123)
        set(VORBISFILE_LIBRARIES Vorbis::vorbisfile Vorbis::vorbis)
        set(MPG123_LIBRARIES MPG123::libmpg123)
    else()
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(VORBISFILE REQUIRED IMPORTED_TARGET vorbisfile)
        pkg_check_modules(MPG123 REQUIRED IMPORTED_TARGET libmpg123)
        set(VORBISFILE_LIBRARIES PkgConfig::VORBISFILE)
        set(MPG123_LIBRARIES PkgConfig::MPG123)
    endif()
endif()

# Unicorn is optional and runtime-loaded (src/platform/unicorn_dyn.cpp).
find_library(UNICORN_LIBRARY NAMES unicorn)

# ---------------------------------------------------------------------------
# libwebp — OPTIONAL, and the only dependency here that changes what a model
# converts to rather than whether it builds.
#
# stb_image (converter) and QImage (viewer) both decode PNG/JPEG/TGA/BMP/GIF,
# and neither decodes WebP. glTF's EXT_texture_webp replaces the core texture
# rather than falling back to it, so a file may be required to ship WebP and
# carry no PNG at all — pilot.glb is exactly that, and its albedo has never
# reached a converted POD or the viewport. Without libwebp those payloads are
# skipped and reported, exactly as before; with it they convert like any other
# texture. See src/tools/image_decode.h.
# ---------------------------------------------------------------------------
if (NOT WIN32)
    pkg_check_modules(WEBP QUIET libwebp)
endif()
if (WEBP_FOUND)
    set(SWORDIGO_HAVE_WEBP TRUE)
    set(SWORDIGO_WEBP_INCLUDE_DIRS ${WEBP_INCLUDE_DIRS})
    set(SWORDIGO_WEBP_LIBRARIES ${WEBP_LIBRARIES})
    message(STATUS "libwebp ${WEBP_VERSION} — EXT_texture_webp textures will decode")
else()
    find_path(SWORDIGO_WEBP_INCLUDE_DIR webp/decode.h)
    find_library(SWORDIGO_WEBP_LIBRARY NAMES webp webpdecoder)
    if (SWORDIGO_WEBP_INCLUDE_DIR AND SWORDIGO_WEBP_LIBRARY)
        set(SWORDIGO_HAVE_WEBP TRUE)
        set(SWORDIGO_WEBP_INCLUDE_DIRS ${SWORDIGO_WEBP_INCLUDE_DIR})
        set(SWORDIGO_WEBP_LIBRARIES ${SWORDIGO_WEBP_LIBRARY})
        message(STATUS "libwebp found — EXT_texture_webp textures will decode")
    else()
        set(SWORDIGO_HAVE_WEBP FALSE)
        message(STATUS "libwebp not found — EXT_texture_webp textures will be skipped")
    endif()
endif()

# ---------------------------------------------------------------------------
# SQLite3 — used by the Memory Research subsystem (embedded recovery catalog).
#
# Strategy:
#   1. Look for a bundled amalgamation at third_party/sqlite3/sqlite3.{c,h}.
#      If present, we compile it directly (no external package needed).
#   2. Fall back to find_package / pkg-config for the system SQLite3 library.
#
# In both cases we expose:
#   SWORDIGO_SQLITE3_INCLUDE_DIRS  — path(s) containing sqlite3.h
#   SWORDIGO_SQLITE3_LIBRARIES     — link target(s)
# ---------------------------------------------------------------------------
set(_sw_sqlite3_bundle "${CMAKE_SOURCE_DIR}/third_party/sqlite3")
if (EXISTS "${_sw_sqlite3_bundle}/sqlite3.c" AND EXISTS "${_sw_sqlite3_bundle}/sqlite3.h")
    message(STATUS "SQLite3: using bundled amalgamation at ${_sw_sqlite3_bundle}")
    add_library(sqlite3_bundled STATIC "${_sw_sqlite3_bundle}/sqlite3.c")
    target_include_directories(sqlite3_bundled PUBLIC "${_sw_sqlite3_bundle}")
    # Silence SQLite's self-promotion commentary and enable WAL extension
    target_compile_definitions(sqlite3_bundled PRIVATE
        SQLITE_OMIT_LOAD_EXTENSION
        SQLITE_ENABLE_DESERIALIZE
        SQLITE_THREADSAFE=1)
    set(SWORDIGO_SQLITE3_INCLUDE_DIRS "${_sw_sqlite3_bundle}")
    set(SWORDIGO_SQLITE3_LIBRARIES    sqlite3_bundled)
    set(SWORDIGO_HAVE_SQLITE3 TRUE)
else()
    find_package(SQLite3 QUIET)
    if (SQLite3_FOUND)
        message(STATUS "SQLite3 ${SQLite3_VERSION} — system library")
        set(SWORDIGO_SQLITE3_INCLUDE_DIRS ${SQLite3_INCLUDE_DIRS})
        set(SWORDIGO_SQLITE3_LIBRARIES    SQLite::SQLite3)
        set(SWORDIGO_HAVE_SQLITE3 TRUE)
    elseif (NOT WIN32)
        pkg_check_modules(SQLITE3_PKG QUIET sqlite3)
        if (SQLITE3_PKG_FOUND)
            message(STATUS "SQLite3 ${SQLITE3_PKG_VERSION} — pkg-config")
            set(SWORDIGO_SQLITE3_INCLUDE_DIRS ${SQLITE3_PKG_INCLUDE_DIRS})
            set(SWORDIGO_SQLITE3_LIBRARIES    ${SQLITE3_PKG_LIBRARIES})
            set(SWORDIGO_HAVE_SQLITE3 TRUE)
        else()
            message(WARNING "SQLite3 not found — Memory Research catalog will be disabled")
            set(SWORDIGO_HAVE_SQLITE3 FALSE)
        endif()
    else()
        message(WARNING "SQLite3 not found — Memory Research catalog will be disabled")
        set(SWORDIGO_HAVE_SQLITE3 FALSE)
    endif()
endif()

# Compile host JIT with this exact compiler/configuration; CMake carries all
# transitive static dependencies and build ordering instead of guessed filenames.
if (SWORDIGO_USE_DYNARMIC)
    set(DYNARMIC_TESTS OFF CACHE BOOL "" FORCE)
    set(DYNARMIC_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
    set(DYNARMIC_USE_BUNDLED_EXTERNALS ON CACHE BOOL "" FORCE)
    set(DYNARMIC_FRONTENDS "A32;A64" CACHE STRING "" FORCE)
    add_subdirectory(deps/dynarmic "${CMAKE_BINARY_DIR}/dynarmic" EXCLUDE_FROM_ALL)
endif()
