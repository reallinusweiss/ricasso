// ============================================================================
// tools_page.cpp — Tools page implementation
// ============================================================================

#include "launcher/pages/tools_page.h"
#include "launcher/launcher_theme.h"
#include "platform/data_path.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>

namespace swordfare::launcher {

ToolsPage::ToolsPage(QWidget* parent) : QWidget(parent) {
    setup_ui();
}

void ToolsPage::setup_ui() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(28, 24, 28, 24);
    main_layout->setSpacing(20);

    auto* title = new QLabel("DEVELOPER SDK & UTILITIES", this);
    title->setStyleSheet("font-size: 14px; font-weight: 800; color: #FFFFFF; letter-spacing: 0.8px;");
    main_layout->addWidget(title);

    // Ruby Studio Card
    auto* ruby_card = new QFrame(this);
    ruby_card->setStyleSheet("background-color: #161B22; border: 1px solid #283141; border-radius: 12px;");
    auto* r_layout = new QHBoxLayout(ruby_card);
    r_layout->setContentsMargins(20, 18, 20, 18);
    r_layout->setSpacing(18);

    auto* r_info = new QVBoxLayout();
    auto* r_name = new QLabel("Ruby Studio — 3D Scene & Asset IDE", ruby_card);
    r_name->setStyleSheet("font-size: 15px; font-weight: bold; color: #FFFFFF;");
    auto* r_desc = new QLabel("Full visual level editor, FileRift schema explorer, 3D viewport, and Lua script IDE.", ruby_card);
    r_desc->setStyleSheet("font-size: 12px; color: #8B949E;");
    r_info->addWidget(r_name);
    r_info->addWidget(r_desc);
    r_layout->addLayout(r_info, 1);

    auto* btn_ruby = new QPushButton("  Launch Ruby Studio", ruby_card);
    btn_ruby->setIcon(LauncherTheme::fa_icon(ICON_FA_ROCKET, Qt::white, 14));
    btn_ruby->setStyleSheet("background-color: #E94560; color: white; border: none; font-weight: bold; border-radius: 6px; padding: 10px 18px;");
    connect(btn_ruby, &QPushButton::clicked, this, &ToolsPage::on_launch_ruby_clicked);
    r_layout->addWidget(btn_ruby);

    main_layout->addWidget(ruby_card);

    // Quick Directory Shortcuts Card
    auto* dir_card = new QFrame(this);
    dir_card->setStyleSheet("background-color: #161B22; border: 1px solid #283141; border-radius: 12px;");
    auto* d_layout = new QVBoxLayout(dir_card);
    d_layout->setContentsMargins(20, 18, 20, 18);
    d_layout->setSpacing(14);

    auto* d_title = new QLabel("FILESYSTEM SHORTCUTS", dir_card);
    d_title->setStyleSheet("font-size: 11px; font-weight: bold; color: #8B949E;");
    d_layout->addWidget(d_title);

    auto* btn_data = new QPushButton("  Open User Data Directory (~/.local/share/swordigo-desktop)", dir_card);
    btn_data->setIcon(LauncherTheme::fa_icon(ICON_FA_FOLDER_OPEN, QColor(160, 174, 192), 14));
    btn_data->setProperty("class", "SecondaryBtn");
    connect(btn_data, &QPushButton::clicked, this, &ToolsPage::on_open_data_clicked);
    d_layout->addWidget(btn_data);

    auto* btn_saves = new QPushButton("  Open Save Directory (VFS Documents)", dir_card);
    btn_saves->setIcon(LauncherTheme::fa_icon(ICON_FA_FLOPPY_DISK, QColor(160, 174, 192), 14));
    btn_saves->setProperty("class", "SecondaryBtn");
    connect(btn_saves, &QPushButton::clicked, this, &ToolsPage::on_open_saves_clicked);
    d_layout->addWidget(btn_saves);

    main_layout->addWidget(dir_card);
    main_layout->addStretch();
}

void ToolsPage::on_launch_ruby_clicked() {
    QString app_dir = QCoreApplication::applicationDirPath();
    QStringList candidates = {
        app_dir + "/ruby_gg",
        app_dir + "/ruby",
        app_dir + "/../bin/ruby_gg",
        "ruby_gg",
        "ruby"
    };

    for (const auto& c : candidates) {
        if (QFileInfo::exists(c) || !c.contains("/")) {
            if (QProcess::startDetached(c, {})) {
                return;
            }
        }
    }
}

void ToolsPage::on_open_data_clicked() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(get_user_data_dir())));
}

void ToolsPage::on_open_saves_clicked() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(get_vfs_save_dir())));
}

} // namespace swordfare::launcher
