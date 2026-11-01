# ============================================================================
# Component: swcore — platform foundation (paths, IO thread, embedded assets,
# OS shims). Everything else depends on this.
# ============================================================================

if (NOT EXISTS "${SRC_DIR}/platform/embedded_assets.cpp")
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/embed_assets.py"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" RESULT_VARIABLE _embed_result)
    if (NOT _embed_result EQUAL 0)
        message(FATAL_ERROR "Failed to generate embedded launcher assets")
    endif()
endif()

swordigo_library(swcore
    ${SRC_DIR}/platform/data_path.cpp
    ${SRC_DIR}/platform/font_fallback.cpp
    ${SRC_DIR}/platform/gdb_compat.cpp
    ${SRC_DIR}/platform/io_thread.cpp
    ${SRC_DIR}/platform/embedded_assets.cpp
    ${SRC_DIR}/platform/os_external.cpp
    ${SRC_DIR}/android/log.c
    ${SRC_DIR}/wincompat/pthread_mutex_shim.c)
target_link_libraries(swcore PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
if (SWORDIGO_STATIC)
    target_compile_definitions(swcore PRIVATE SWORDIGO_STATIC_COMPONENTS=1)
endif()
# embedded_assets.cpp is a ~60MB generated TU of const byte arrays — no -O3 needed
if (MSVC)
    set_source_files_properties(${SRC_DIR}/platform/embedded_assets.cpp PROPERTIES COMPILE_OPTIONS "/Od")
else()
    set_source_files_properties(${SRC_DIR}/platform/embedded_assets.cpp PROPERTIES COMPILE_OPTIONS "-O0")
endif()
