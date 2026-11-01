# ============================================================================
# Component: swordigo_build_studio — GUI Build Launcher & State Manager
# ============================================================================

if (SWORDIGO_BUILD_STUDIO)
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

    set(BUILD_STUDIO_SOURCES
        ${SRC_DIR}/tools/build_studio/main.cpp
        ${SRC_DIR}/tools/build_studio/main_window.cpp
        ${SRC_DIR}/tools/build_studio/build_runner.cpp
        ${SRC_DIR}/tools/build_studio/change_detector.cpp
        ${SRC_DIR}/tools/build_studio/elf_check.cpp
    )

    add_executable(swordigo_build_studio ${BUILD_STUDIO_SOURCES})

    set_target_properties(swordigo_build_studio PROPERTIES
        AUTOMOC ON
    )

    target_include_directories(swordigo_build_studio PRIVATE
        ${SRC_DIR}/tools/build_studio
    )

    target_link_libraries(swordigo_build_studio PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
    )
endif()
