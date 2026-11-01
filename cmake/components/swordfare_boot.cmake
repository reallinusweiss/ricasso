# ============================================================================
# Component: swordfare_boot — the main launcher executable (bin/swordfare).
# Static FFmpeg + all component libraries get linked into this one binary.
# ============================================================================

add_executable(swordfare_boot
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/platform/client_host_smoke.cpp
    ${SRC_DIR}/platform/arm64_reloc.cpp
    ${SRC_DIR}/platform/display.cpp
    ${SRC_DIR}/platform/pod_ipc.cpp
    ${SRC_DIR}/platform/loading_screen.cpp
    ${SRC_DIR}/platform/crash_dialog.cpp
    ${SRC_DIR}/platform/openswordigo_host.cpp
    ${SRC_DIR}/platform/gui.cpp
    ${SRC_DIR}/platform/input_config.cpp
    ${SRC_DIR}/platform/binary_selector.cpp
    ${SRC_DIR}/platform/ffmpeg_dyn.cpp)
set_target_properties(swordfare_boot PROPERTIES OUTPUT_NAME swordfare AUTOMOC ON)
swordigo_target(swordfare_boot)
if (WIN32)
    enable_language(RC)
    target_sources(swordfare_boot PRIVATE ${SRC_DIR}/platform/windows_client.rc)
    target_include_directories(swordfare_boot PRIVATE ${SRC_DIR}/platform)
    if (MINGW)
        # windres otherwise builds an unquoted absolute gcc command internally.
        # Resolve cpp through PATH so toolchains under paths with spaces work.
        set_source_files_properties(${SRC_DIR}/platform/windows_client.rc
            PROPERTIES COMPILE_OPTIONS "--preprocessor=cpp")
    endif()
    if (MSVC)
        target_link_options(swordfare_boot PRIVATE /MANIFEST:NO)
    endif()
endif()

target_link_libraries(swordfare_boot PRIVATE
    swcore swgui swfmt swpod filerift swgfx swemu swordfare
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network
    SDL3::SDL3 ${SDL3_IMAGE_LINK} ${VORBISFILE_LIBRARIES} ${MPG123_LIBRARIES}
    ZLIB::ZLIB OpenAL::OpenAL OpenGL::GL Threads::Threads ${CMAKE_DL_LIBS} ${SWORDIGO_LIBM} ${SWORDIGO_SOCKLIB})

# shm_open / mmap for the Ruby GG engine-pod frame ring (pod_ipc.cpp).
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(swordfare_boot PRIVATE rt)
endif()


if (SWORDIGO_USE_DYNARMIC)
    target_link_libraries(swordfare_boot PRIVATE dynarmic)
endif()
