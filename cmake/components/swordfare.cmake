# ============================================================================
# Component: swordfare — the in-game overlay GUI (script editor, mods, launcher).
# ============================================================================

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

swordigo_library(swordfare
    ${SRC_DIR}/platform/swordfare_theme.cpp
    ${SRC_DIR}/platform/swordfare_gui.cpp
    ${SRC_DIR}/launcher/launcher_theme.cpp
    ${SRC_DIR}/launcher/profile_manager.cpp
    ${SRC_DIR}/launcher/pages/play_page.cpp
    ${SRC_DIR}/launcher/pages/library_page.cpp
    ${SRC_DIR}/launcher/pages/mods_page.cpp
    ${SRC_DIR}/launcher/pages/store_page.cpp
    ${SRC_DIR}/launcher/pages/save_editor_page.cpp
    ${SRC_DIR}/launcher/pages/profile_page.cpp
    ${SRC_DIR}/launcher/pages/settings_page.cpp
    ${SRC_DIR}/launcher/pages/tools_page.cpp
    ${SRC_DIR}/launcher/qt_launcher_window.cpp
    ${SRC_DIR}/launcher/launcher_bridge.cpp
    ${SRC_DIR}/platform/launcher_config.cpp
    ${SRC_DIR}/platform/save_editor.cpp
    ${SRC_DIR}/platform/scl_parser.cpp
    ${SRC_DIR}/platform/mod_manager.cpp
    ${SRC_DIR}/platform/mod_catalog_embedded.cpp
    ${SRC_DIR}/game/mod_tools.cpp
    ${SRC_DIR}/game/mod_config.cpp
    ${SRC_DIR}/game/save_editor_logic.cpp
    ${SRC_DIR}/game/camera_override.cpp
    # ── Memory Research subsystem ──────────────────────────────────────────
    ${SRC_DIR}/game/research/embedded_recovery_db.cpp
    ${SRC_DIR}/game/research/recovery_catalog.cpp
    ${SRC_DIR}/game/research/struct_decoder.cpp
    ${SRC_DIR}/game/research/memory_research_tab.cpp
    ${SRC_DIR}/game/research/vtable_classifier.cpp
    ${SRC_DIR}/game/research/root_walker.cpp
    ${SRC_DIR}/game/research/recovery_session_manager.cpp
    ${SRC_DIR}/game/research/hex_inspector.cpp
    ${SRC_DIR}/game/research/instance_registry.cpp
    ${SRC_DIR}/game/research/mem_identity.cpp
    ${SRC_DIR}/game/research/elf_symbols.cpp
    ${SRC_DIR}/game/research/elf_sections.cpp
    ${SRC_DIR}/game/research/mem_scanner.cpp
    ${SRC_DIR}/game/research/mem_address_list.cpp
    ${SRC_DIR}/game/research/unclaimed_explorer.cpp
    ${SRC_DIR}/game/research/live_object_map.cpp
    ${SRC_DIR}/game/research/mem_access_trace.cpp
    ${SRC_DIR}/game/research/research_workspace.cpp
    # ── Xpera GUI style toolkit ──────────────────────────────────────────
    ${SRC_DIR}/platform/xpera/xpera_style.cpp
    ${SRC_DIR}/platform/xpera/xpera_gui.cpp)
set_target_properties(swordfare PROPERTIES AUTOMOC ON)
# embedded_recovery_db.cpp is a large generated TU of const byte arrays — compile at -O0
set_source_files_properties(${SRC_DIR}/game/research/embedded_recovery_db.cpp
    PROPERTIES COMPILE_OPTIONS "$<IF:$<CXX_COMPILER_ID:MSVC>,/Od,-O0>")
target_link_libraries(swordfare PRIVATE swcore swgui filerift SDL3::SDL3 OpenGL::GL Threads::Threads ZLIB::ZLIB Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)
if (SWORDIGO_HAVE_SQLITE3)
    target_include_directories(swordfare PRIVATE ${SWORDIGO_SQLITE3_INCLUDE_DIRS})
    target_link_libraries(swordfare PRIVATE ${SWORDIGO_SQLITE3_LIBRARIES})
    target_compile_definitions(swordfare PRIVATE SWORDIGO_HAVE_SQLITE3=1)
endif()
