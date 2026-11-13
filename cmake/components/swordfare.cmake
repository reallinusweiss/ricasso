# ============================================================================
# Component: swordfare — the in-game overlay GUI (script editor, mods, launcher).
# ============================================================================

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

swordigo_library(swordfare
    ${SRC_DIR}/platform/swordfare_theme.cpp
    ${SRC_DIR}/platform/swordfare_gui.cpp
    ${SRC_DIR}/platform/win32_menu.cpp
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
    ${SRC_DIR}/platform/mod_manager.cpp
    ${SRC_DIR}/platform/mod_catalog_embedded.cpp
    ${SRC_DIR}/game/mod_tools.cpp
    ${SRC_DIR}/game/mod_config.cpp
    ${SRC_DIR}/game/save_editor_logic.cpp
    ${SRC_DIR}/game/camera_override.cpp
    # ── Xpera GUI style toolkit ──────────────────────────────────────────
    ${SRC_DIR}/platform/xpera/xpera_style.cpp
    ${SRC_DIR}/platform/xpera/xpera_gui.cpp)
set_target_properties(swordfare PROPERTIES AUTOMOC ON)
target_link_libraries(swordfare PRIVATE swcore swgui filerift SDL3::SDL3 OpenGL::GL Threads::Threads ZLIB::ZLIB Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)
