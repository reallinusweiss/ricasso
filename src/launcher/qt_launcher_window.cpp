// ============================================================================
// qt_launcher_window.cpp — Main Qt6 launcher window implementation
// ============================================================================

#include "launcher/qt_launcher_window.h"
#include "launcher/launcher_theme.h"
#include "launcher/profile_manager.h"

#include "launcher/pages/play_page.h"
#include "launcher/pages/library_page.h"
#include "launcher/pages/mods_page.h"
#include "launcher/pages/store_page.h"
#include "launcher/pages/save_editor_page.h"
#include "launcher/pages/profile_page.h"
#include "launcher/pages/settings_page.h"
#include "launcher/pages/tools_page.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QMouseEvent>
#include <QApplication>
#include <QPainter>

namespace swordfare::launcher {

QtLauncherWindow::QtLauncherWindow(BinarySelector& selector, QWidget* parent)
    : QMainWindow(parent), m_selector(selector) {
    LauncherTheme::init_fonts();

    setWindowTitle("Swordigo Desktop Launcher");
    resize(1100, 680);
    setMinimumSize(960, 600);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setStyleSheet(LauncherTheme::stylesheet());

    setup_ui();
    switch_page(0); // Default to Home
    update_sidebar_profile();
    update_status_footer();

    connect(&ProfileManager::instance(), &ProfileManager::profileChanged, this, &QtLauncherWindow::update_sidebar_profile);
}

void QtLauncherWindow::setup_ui() {
    auto* central = new QWidget(this);
    central->setObjectName("CentralWidget");
    setCentralWidget(central);

    auto* root_layout = new QVBoxLayout(central);
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setSpacing(0);

    // ── 1. Top Custom Titlebar ───────────────────────────────────────────────
    auto* titlebar = new QFrame(this);
    titlebar->setFixedHeight(36);
    titlebar->setStyleSheet("background-color: #0A0D14; border-bottom: 1px solid #1C2433;");
    auto* tb_layout = new QHBoxLayout(titlebar);
    tb_layout->setContentsMargins(14, 0, 8, 0);
    tb_layout->setSpacing(10);

    // App Logo icon + Title text
    auto* top_icon = new QLabel(titlebar);
    top_icon->setFixedSize(18, 18);
    top_icon->setPixmap(LauncherTheme::app_logo(18));
    tb_layout->addWidget(top_icon);

    auto* title_logo = new QLabel("SWORDIGO DESKTOP", titlebar);
    title_logo->setStyleSheet("font-size: 11px; font-weight: 800; color: #E94560; letter-spacing: 1.2px;");
    tb_layout->addWidget(title_logo);

    tb_layout->addStretch();

    // Window controls
    auto* btn_min = new QPushButton(titlebar);
    btn_min->setFixedSize(32, 26);
    btn_min->setIcon(LauncherTheme::fa_icon(ICON_FA_WINDOW_MINIMIZE, QColor(139, 148, 158), 12));
    btn_min->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 4px; }"
        "QPushButton:hover { background-color: #1F2737; }"
    );
    btn_min->setToolTip("Minimize");
    connect(btn_min, &QPushButton::clicked, this, &QMainWindow::showMinimized);
    tb_layout->addWidget(btn_min);

    auto* btn_close = new QPushButton(titlebar);
    btn_close->setFixedSize(32, 26);
    btn_close->setIcon(LauncherTheme::fa_icon(ICON_FA_XMARK, QColor(139, 148, 158), 13));
    btn_close->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 4px; }"
        "QPushButton:hover { background-color: #E94560; }"
    );
    btn_close->setToolTip("Close (ESC)");
    connect(btn_close, &QPushButton::clicked, this, &QMainWindow::close);
    tb_layout->addWidget(btn_close);

    root_layout->addWidget(titlebar);

    // ── 2. Body Split: Left Sidebar + Central Stack ──────────────────────────
    auto* body = new QWidget(this);
    auto* body_layout = new QHBoxLayout(body);
    body_layout->setContentsMargins(0, 0, 0, 0);
    body_layout->setSpacing(0);

    // ── Left Sidebar (240px width 1:1 with ImGui) ────────────────────────────
    auto* sidebar = new QFrame(body);
    sidebar->setObjectName("SidebarContainer");
    sidebar->setFixedWidth(240);
    sidebar->setStyleSheet("background-color: #0E1420; border-right: 1px solid #1C2433;");
    auto* sb_layout = new QVBoxLayout(sidebar);
    sb_layout->setContentsMargins(0, 0, 0, 0);
    sb_layout->setSpacing(0);

    // ── Sidebar Top Branding Area (80px height 1:1 with ImGui) ───────────────
    auto* logo_area = new QFrame(sidebar);
    logo_area->setFixedHeight(80);
    logo_area->setStyleSheet("background-color: transparent;");
    auto* logo_layout = new QHBoxLayout(logo_area);
    logo_layout->setContentsMargins(18, 16, 18, 16);
    logo_layout->setSpacing(14);

    auto* logo_icon_lbl = new QLabel(logo_area);
    logo_icon_lbl->setFixedSize(48, 48);
    logo_icon_lbl->setPixmap(LauncherTheme::app_logo(48));
    logo_layout->addWidget(logo_icon_lbl);

    auto* logo_text_box = new QVBoxLayout();
    logo_text_box->setSpacing(0);
    logo_text_box->setAlignment(Qt::AlignVCenter);

    auto* brand_name = new QLabel("SWORDIGO", logo_area);
    brand_name->setStyleSheet("font-size: 17px; font-weight: 900; color: #E94560; letter-spacing: 1.2px;");
    auto* brand_sub = new QLabel("DESKTOP", logo_area);
    brand_sub->setStyleSheet("font-size: 11px; font-weight: 700; color: #E6EDF3; letter-spacing: 2.8px; opacity: 0.9;");

    logo_text_box->addWidget(brand_name);
    logo_text_box->addWidget(brand_sub);
    logo_layout->addLayout(logo_text_box);
    logo_layout->addStretch();

    sb_layout->addWidget(logo_area);

    // Logo bottom separator line
    auto* logo_sep = new QFrame(sidebar);
    logo_sep->setFixedHeight(1);
    logo_sep->setStyleSheet("background-color: rgba(255, 255, 255, 0.08);");
    sb_layout->addWidget(logo_sep);

    // Navigation buttons container
    auto* nav_container = new QWidget(sidebar);
    auto* nav_layout = new QVBoxLayout(nav_container);
    nav_layout->setContentsMargins(0, 8, 0, 8);
    nav_layout->setSpacing(2);

    // Nav items (1:1 with ImGui edition)
    nav_layout->addWidget(create_nav_btn(ICON_FA_HOUSE, "Home", 0));
    nav_layout->addWidget(create_nav_btn(ICON_FA_LAYER_GROUP, "Library", 1));
    nav_layout->addWidget(create_nav_btn(ICON_FA_PUZZLE_PIECE, "Mods", 2));
    nav_layout->addWidget(create_nav_btn(ICON_FA_GLOBE, "Mod Browser", 3));
    nav_layout->addWidget(create_nav_btn(ICON_FA_FLOPPY_DISK, "Saves", 4));
    nav_layout->addWidget(create_nav_btn(ICON_FA_WRENCH, "SDK / Tools", 5));

    // Nav separator
    auto* nav_sep = new QFrame(nav_container);
    nav_sep->setFixedHeight(1);
    nav_sep->setStyleSheet("background-color: rgba(255, 255, 255, 0.06); margin: 6px 16px;");
    nav_layout->addWidget(nav_sep);

    nav_layout->addWidget(create_nav_btn(ICON_FA_GEAR, "Settings", 6));
    nav_layout->addStretch();

    sb_layout->addWidget(nav_container, 1);

    // ── Profile Card at Bottom (74px height 1:1 with ImGui) ─────────────────
    auto* prof_sep = new QFrame(sidebar);
    prof_sep->setFixedHeight(1);
    prof_sep->setStyleSheet("background-color: rgba(255, 255, 255, 0.08);");
    sb_layout->addWidget(prof_sep);

    auto* profile_card = new QFrame(sidebar);
    profile_card->setFixedHeight(74);
    profile_card->setStyleSheet(
        "QFrame {"
        "  background-color: #121826;"
        "  border: none;"
        "}"
        "QFrame:hover {"
        "  background-color: #182032;"
        "}"
    );
    profile_card->setCursor(Qt::PointingHandCursor);
    auto* prof_layout = new QHBoxLayout(profile_card);
    prof_layout->setContentsMargins(14, 0, 14, 0);
    prof_layout->setSpacing(12);

    m_pill_avatar = new QLabel(profile_card);
    m_pill_avatar->setFixedSize(40, 40);
    prof_layout->addWidget(m_pill_avatar);

    auto* prof_meta = new QVBoxLayout();
    prof_meta->setSpacing(2);
    prof_meta->setAlignment(Qt::AlignVCenter);

    m_pill_name = new QLabel("Hero", profile_card);
    m_pill_name->setStyleSheet("font-size: 13px; font-weight: bold; color: #FFFFFF;");
    auto* pill_sub = new QLabel("Local Profile", profile_card);
    pill_sub->setStyleSheet("font-size: 11px; color: #8B949E;");
    prof_meta->addWidget(m_pill_name);
    prof_meta->addWidget(pill_sub);
    prof_layout->addLayout(prof_meta, 1);

    auto* edit_link = new QLabel(profile_card);
    edit_link->setPixmap(LauncherTheme::fa_pixmap(ICON_FA_PEN, QColor(88, 166, 255), 14));
    prof_layout->addWidget(edit_link);

    profile_card->installEventFilter(this);
    // Clicking profile card switches to Profile tab (page 7)
    connect(profile_card, &QObject::destroyed, this, [](){});
    sb_layout->addWidget(profile_card);

    body_layout->addWidget(sidebar);

    // ── Central Stack ────────────────────────────────────────────────────────
    m_stack = new QStackedWidget(body);

    m_play_page = new PlayPage(m_selector, m_stack);
    connect(m_play_page, &PlayPage::launchRequested, this, &QtLauncherWindow::on_launch_requested);
    connect(m_play_page, &PlayPage::navigateToLibrary, this, [this]() { switch_page(1); });
    m_stack->addWidget(m_play_page); // Page 0: Home

    m_library_page = new LibraryPage(m_selector, m_stack);
    connect(m_library_page, &LibraryPage::launchRequested, this, &QtLauncherWindow::on_launch_requested);
    connect(m_library_page, &LibraryPage::instancesChanged, m_play_page, &PlayPage::refresh_instances);
    m_stack->addWidget(m_library_page); // Page 1: Library

    m_mods_page = new ModsPage(m_stack);
    m_stack->addWidget(m_mods_page); // Page 2: Mods

    m_store_page = new StorePage(m_stack);
    connect(m_store_page, &StorePage::modInstalled, m_mods_page, &ModsPage::refresh_mods);
    m_stack->addWidget(m_store_page); // Page 3: Mod Browser

    m_save_page = new SaveEditorPage(m_stack);
    m_stack->addWidget(m_save_page); // Page 4: Saves

    m_tools_page = new ToolsPage(m_stack);
    m_stack->addWidget(m_tools_page); // Page 5: Tools

    m_settings_page = new SettingsPage(m_stack);
    m_stack->addWidget(m_settings_page); // Page 6: Settings

    m_profile_page = new ProfilePage(m_stack);
    m_stack->addWidget(m_profile_page); // Page 7: Profile

    body_layout->addWidget(m_stack, 1);
    root_layout->addWidget(body, 1);

    // ── 3. Bottom Status Footer (28px height 1:1 with ImGui) ─────────────────
    auto* footer = new QFrame(this);
    footer->setFixedHeight(28);
    footer->setStyleSheet("background-color: #06080B; border-top: 1px solid rgba(255, 255, 255, 0.05);");
    auto* ft_layout = new QHBoxLayout(footer);
    ft_layout->setContentsMargins(16, 0, 16, 0);

    auto* ft_status_box = new QHBoxLayout();
    ft_status_box->setSpacing(6);
    auto* ft_status_dot = new QLabel(footer);
    ft_status_dot->setPixmap(LauncherTheme::fa_pixmap(ICON_FA_CIRCLE_CHECK, QColor(61, 184, 79), 12));
    m_lbl_footer_status = new QLabel("Ready", footer);
    m_lbl_footer_status->setStyleSheet("font-size: 11px; color: #3DB84F; font-weight: 600;");
    ft_status_box->addWidget(ft_status_dot);
    ft_status_box->addWidget(m_lbl_footer_status);
    ft_layout->addLayout(ft_status_box);

    ft_layout->addStretch();

    m_lbl_footer_hints = new QLabel("v8.0 Remaster  |  Enter: Launch  |  ESC: Close", footer);
    m_lbl_footer_hints->setStyleSheet("font-size: 11px; color: #6E7681;");
    ft_layout->addWidget(m_lbl_footer_hints);

    root_layout->addWidget(footer);
}

QPushButton* QtLauncherWindow::create_nav_btn(const char* fa_glyph, const QString& text, int page_idx) {
    auto* btn = new QPushButton(this);
    btn->setIcon(LauncherTheme::fa_icon(fa_glyph, QColor(160, 174, 192), 16));
    btn->setIconSize(QSize(18, 18));
    btn->setText(QString("   %1").arg(text));
    btn->setProperty("class", "NavBtn");
    btn->setCheckable(true);
    btn->setFixedHeight(46);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(
        "QPushButton.NavBtn {"
        "  background-color: transparent;"
        "  color: #A0AEC0;"
        "  text-align: left;"
        "  padding-left: 20px;"
        "  border: none;"
        "  border-radius: 0px;"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "}"
        "QPushButton.NavBtn:hover {"
        "  background-color: rgba(255, 255, 255, 0.05);"
        "  color: #FFFFFF;"
        "}"
        "QPushButton.NavBtn:checked {"
        "  background-color: rgba(233, 69, 96, 0.12);"
        "  color: #E94560;"
        "  border-left: 3px solid #E94560;"
        "  font-weight: 700;"
        "}"
    );

    connect(btn, &QPushButton::clicked, this, [this, page_idx]() { switch_page(page_idx); });
    m_nav_buttons.push_back(btn);
    return btn;
}

void QtLauncherWindow::switch_page(int index) {
    if (index < 0 || index >= m_stack->count()) return;
    m_stack->setCurrentIndex(index);

    for (size_t i = 0; i < m_nav_buttons.size(); ++i) {
        m_nav_buttons[i]->setChecked(static_cast<int>(i) == index);
    }

    update_status_footer();
}

void QtLauncherWindow::update_sidebar_profile() {
    const auto& prof = ProfileManager::instance().profile();
    m_pill_name->setText(prof.username);
    m_pill_avatar->setPixmap(ProfileManager::instance().get_avatar_pixmap(38));
}

void QtLauncherWindow::update_status_footer() {
    const auto& bins = m_selector.get_binaries();
    if (bins.empty()) {
        m_lbl_footer_status->setText("No Game Installed");
        m_lbl_footer_status->setStyleSheet("font-size: 11px; color: #E94560; font-weight: 600;");
    } else {
        m_lbl_footer_status->setText("Ready");
        m_lbl_footer_status->setStyleSheet("font-size: 11px; color: #3DB84F; font-weight: 600;");
    }
}

void QtLauncherWindow::on_launch_requested(const LaunchConfig& config) {
    m_launch_config = config;
    m_should_launch = true;
    close();
}

// Frameless window dragging support
void QtLauncherWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && event->position().y() <= 36.0) {
        m_dragging = true;
        m_drag_pos = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    } else if (event->button() == Qt::LeftButton) {
        // Check if clicking in bottom profile area to open Profile page
        if (event->position().x() <= 240 && event->position().y() >= height() - 74 - 28) {
            switch_page(7); // Profile Page
        }
    }
}

void QtLauncherWindow::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_drag_pos);
        event->accept();
    }
}

void QtLauncherWindow::mouseReleaseEvent(QMouseEvent* event) {
    m_dragging = false;
    event->accept();
}

} // namespace swordfare::launcher
