#pragma once
// ============================================================================
// qt_launcher_window.h — Main Qt6 launcher window with sidebar navigation
// ============================================================================

#include <QMainWindow>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QPoint>
#include "platform/binary_selector.h"
#include "platform/launcher_ui.h"

namespace swordfare::launcher {

class PlayPage;
class LibraryPage;
class ModsPage;
class StorePage;
class SaveEditorPage;
class ProfilePage;
class SettingsPage;
class ToolsPage;

class QtLauncherWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit QtLauncherWindow(BinarySelector& selector, QWidget* parent = nullptr);

    LaunchConfig get_launch_config() const { return m_launch_config; }
    bool should_launch() const { return m_should_launch; }

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private slots:
    void switch_page(int index);
    void on_launch_requested(const LaunchConfig& config);
    void update_sidebar_profile();
    void update_status_footer();

private:
    void setup_ui();
    QPushButton* create_nav_btn(const char* fa_glyph, const QString& text, int page_idx);

    BinarySelector&   m_selector;
    LaunchConfig      m_launch_config;
    bool              m_should_launch = false;

    QStackedWidget*   m_stack = nullptr;
    std::vector<QPushButton*> m_nav_buttons;

    // Sidebar Profile Pill
    QLabel*           m_pill_avatar = nullptr;
    QLabel*           m_pill_name = nullptr;

    // Status Footer
    QLabel*           m_lbl_footer_status = nullptr;
    QLabel*           m_lbl_footer_hints = nullptr;

    // Pages
    PlayPage*         m_play_page = nullptr;
    LibraryPage*      m_library_page = nullptr;
    ModsPage*         m_mods_page = nullptr;
    StorePage*        m_store_page = nullptr;
    SaveEditorPage*   m_save_page = nullptr;
    ToolsPage*        m_tools_page = nullptr;
    SettingsPage*     m_settings_page = nullptr;
    ProfilePage*      m_profile_page = nullptr;

    // Window dragging
    bool              m_dragging = false;
    QPoint            m_drag_pos;
};

} // namespace swordfare::launcher
