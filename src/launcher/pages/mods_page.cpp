// ============================================================================
// mods_page.cpp — Mod manager implementation with mod icons and markdown
// ============================================================================

#include "launcher/pages/mods_page.h"
#include "launcher/launcher_theme.h"
#include "platform/data_path.h"
#include "platform/launcher_config.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QPainter>
#include <QPainterPath>

namespace swordfare::launcher {

static std::string get_mods_dir() {
    std::string path = get_user_data_dir() + "/mods";
    QDir().mkpath(QString::fromStdString(path));
    return path;
}

ModsPage::ModsPage(QWidget* parent) : QWidget(parent) {
    setup_ui();
    refresh_mods();
}

void ModsPage::setup_ui() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(28, 22, 28, 22);
    main_layout->setSpacing(16);

    // ── Header Bar ───────────────────────────────────────────────────────────
    auto* header_layout = new QHBoxLayout();
    
    auto* title_box = new QHBoxLayout();
    title_box->setSpacing(10);
    auto* icon_title = new QLabel(this);
    icon_title->setPixmap(LauncherTheme::fa_pixmap(ICON_FA_PUZZLE_PIECE, QColor(233, 69, 96), 20));
    title_box->addWidget(icon_title);

    auto* title = new QLabel("Installed Mods & Packs", this);
    title->setStyleSheet("font-size: 20px; font-weight: 800; color: #FFFFFF; letter-spacing: 0.5px;");
    title_box->addWidget(title);
    header_layout->addLayout(title_box);

    header_layout->addStretch();

    auto* btn_open = new QPushButton("  Open Mods Folder", this);
    btn_open->setIcon(LauncherTheme::fa_icon(ICON_FA_FOLDER_OPEN, QColor(160, 174, 192), 14));
    btn_open->setProperty("class", "SecondaryBtn");
    connect(btn_open, &QPushButton::clicked, this, &ModsPage::on_open_folder_clicked);
    header_layout->addWidget(btn_open);

    auto* btn_install = new QPushButton("  Install Mod (.zip)", this);
    btn_install->setIcon(LauncherTheme::fa_icon(ICON_FA_PLUS, Qt::white, 14));
    btn_install->setStyleSheet(
        "QPushButton {"
        "  background-color: #E94560; color: white; border: none; font-weight: bold; border-radius: 6px;"
        "  padding: 8px 16px; font-size: 13px;"
        "}"
        "QPushButton:hover { background-color: #FF5B79; }"
        "QPushButton:pressed { background-color: #D63447; }"
    );
    connect(btn_install, &QPushButton::clicked, this, &ModsPage::on_install_zip_clicked);
    header_layout->addWidget(btn_install);

    main_layout->addLayout(header_layout);

    // ── Content Split: Mod List on Left, Details & Controls on Right ──────────
    auto* content_layout = new QHBoxLayout();
    content_layout->setSpacing(18);

    // Left List
    auto* list_box = new QVBoxLayout();
    m_list = new QListWidget(this);
    m_list->setSpacing(6);
    m_list->setIconSize(QSize(44, 44));
    connect(m_list, &QListWidget::currentRowChanged, this, &ModsPage::on_selection_changed);
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        int r = m_list->row(item);
        if (r >= 0) on_toggle_mod(r);
    });
    list_box->addWidget(m_list);

    // Reorder Buttons below list
    auto* reorder_row = new QHBoxLayout();
    m_btn_up = new QPushButton("  Move Up (Higher Priority)", this);
    m_btn_up->setIcon(LauncherTheme::fa_icon(ICON_FA_ARROW_UP, QColor(160, 174, 192), 12));
    m_btn_up->setProperty("class", "SecondaryBtn");
    connect(m_btn_up, &QPushButton::clicked, this, &ModsPage::on_move_up_clicked);
    reorder_row->addWidget(m_btn_up);

    m_btn_down = new QPushButton("  Move Down", this);
    m_btn_down->setIcon(LauncherTheme::fa_icon(ICON_FA_ARROW_DOWN, QColor(160, 174, 192), 12));
    m_btn_down->setProperty("class", "SecondaryBtn");
    connect(m_btn_down, &QPushButton::clicked, this, &ModsPage::on_move_down_clicked);
    reorder_row->addWidget(m_btn_down);

    list_box->addLayout(reorder_row);
    content_layout->addLayout(list_box, 3);

    // Right Details Card
    auto* details_card = new QFrame(this);
    details_card->setObjectName("ModDetailsCard");
    details_card->setStyleSheet(
        "#ModDetailsCard {"
        "  background-color: #121824;"
        "  border: 1px solid #1F2737;"
        "  border-radius: 12px;"
        "}"
    );
    auto* d_layout = new QVBoxLayout(details_card);
    d_layout->setContentsMargins(22, 20, 22, 20);
    d_layout->setSpacing(14);

    // Top Header with Icon, Title, and Meta
    auto* header_meta_row = new QHBoxLayout();
    header_meta_row->setSpacing(16);

    m_lbl_detail_icon = new QLabel(details_card);
    m_lbl_detail_icon->setFixedSize(64, 64);
    header_meta_row->addWidget(m_lbl_detail_icon);

    auto* meta_col = new QVBoxLayout();
    meta_col->setSpacing(4);

    m_lbl_detail_title = new QLabel("Select a Mod", details_card);
    m_lbl_detail_title->setStyleSheet("font-size: 18px; font-weight: 800; color: #FFFFFF;");
    meta_col->addWidget(m_lbl_detail_title);

    m_lbl_detail_meta = new QLabel(details_card);
    m_lbl_detail_meta->setStyleSheet("font-size: 12px; color: #8B949E;");
    meta_col->addWidget(m_lbl_detail_meta);

    m_lbl_detail_status = new QLabel(details_card);
    m_lbl_detail_status->setStyleSheet("background-color: #162E1C; color: #3FB950; border: 1px solid #244C2E; border-radius: 4px; padding: 2px 8px; font-size: 11px; font-weight: bold;");
    meta_col->addWidget(m_lbl_detail_status);

    header_meta_row->addLayout(meta_col, 1);
    d_layout->addLayout(header_meta_row);

    // Separator line
    auto* sep = new QFrame(details_card);
    sep->setFixedHeight(1);
    sep->setStyleSheet("background-color: #1F2737;");
    d_layout->addWidget(sep);

    auto* desc_label = new QLabel("DESCRIPTION", details_card);
    desc_label->setStyleSheet("font-size: 11px; font-weight: 700; color: #6E7681; letter-spacing: 0.8px;");
    d_layout->addWidget(desc_label);

    // Markdown description viewer
    m_txt_desc = new QTextBrowser(details_card);
    m_txt_desc->setOpenExternalLinks(true);
    m_txt_desc->setStyleSheet(
        "QTextBrowser {"
        "  background-color: #0A0D14;"
        "  border: 1px solid #1C2433;"
        "  border-radius: 8px;"
        "  padding: 10px;"
        "  color: #E6EDF3;"
        "  font-size: 13px;"
        "  line-height: 1.45;"
        "}"
    );
    d_layout->addWidget(m_txt_desc, 1);

    // Action buttons
    auto* btn_actions = new QHBoxLayout();
    btn_actions->setSpacing(10);

    m_btn_toggle = new QPushButton("Toggle Mod", details_card);
    m_btn_toggle->setProperty("class", "SecondaryBtn");
    m_btn_toggle->setCursor(Qt::PointingHandCursor);
    connect(m_btn_toggle, &QPushButton::clicked, this, [this]() {
        int r = m_list->currentRow();
        if (r >= 0) on_toggle_mod(r);
    });
    btn_actions->addWidget(m_btn_toggle);

    m_btn_delete = new QPushButton("  Uninstall", details_card);
    m_btn_delete->setIcon(LauncherTheme::fa_icon(ICON_FA_TRASH, QColor(255, 107, 107), 12));
    m_btn_delete->setProperty("class", "DangerBtn");
    m_btn_delete->setCursor(Qt::PointingHandCursor);
    connect(m_btn_delete, &QPushButton::clicked, this, &ModsPage::on_delete_mod_clicked);
    btn_actions->addWidget(m_btn_delete);

    d_layout->addLayout(btn_actions);

    content_layout->addWidget(details_card, 2);
    main_layout->addLayout(content_layout);
}

QPixmap ModsPage::get_mod_icon(const modman::ModMeta& m, int size) {
    QPixmap raw;
    // 1. Try explicit icon_path
    if (!m.icon_path.empty() && QFile::exists(QString::fromStdString(m.icon_path))) {
        raw.load(QString::fromStdString(m.icon_path));
    }
    // 2. Try dir_path / icon.png
    if (raw.isNull() && !m.dir_path.empty()) {
        QString p = QString::fromStdString(m.dir_path) + "/icon.png";
        if (QFile::exists(p)) raw.load(p);
    }
    // 3. Try dir_path / icon.jpg or preview.png
    if (raw.isNull() && !m.dir_path.empty()) {
        QString p2 = QString::fromStdString(m.dir_path) + "/preview.png";
        if (QFile::exists(p2)) raw.load(p2);
    }
    // 4. Try user cache: mod_cache/icons/<id>.png
    if (raw.isNull()) {
        QString cache = QString::fromStdString(get_user_data_dir()) + "/mod_cache/icons/" + QString::fromStdString(m.id) + ".png";
        if (QFile::exists(cache)) raw.load(cache);
    }
    // 5. Fallback procedural / FA icon
    if (raw.isNull()) {
        raw = LauncherTheme::fa_pixmap(ICON_FA_PUZZLE_PIECE, QColor(233, 69, 96), size);
    }

    // Return crisp rounded pixmap
    const int scale = 2;
    const int px = size * scale;
    QPixmap out(px, px);
    out.fill(Qt::transparent);

    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QPainterPath path;
    path.addRoundedRect(0, 0, px, px, 12, 12);
    p.setClipPath(path);
    p.drawPixmap(0, 0, raw.scaled(px, px, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    p.end();

    out.setDevicePixelRatio(scale);
    return out;
}

void ModsPage::refresh_mods() {
    m_list->clear();
    m_mods = modman::list_mods(get_mods_dir());

    if (m_mods.empty()) {
        LauncherConfig lcfg = launcher_config_load();
        if (!lcfg.mod_load_order.empty()) {
            lcfg.mod_load_order.clear();
            launcher_config_save(lcfg);
        }
        auto* item = new QListWidgetItem("No mods installed. Click '+ Install Mod (.zip)' or download from Mod Browser.");
        item->setFlags(Qt::NoItemFlags);
        m_list->addItem(item);
        m_btn_toggle->setEnabled(false);
        m_btn_delete->setEnabled(false);
        m_btn_up->setEnabled(false);
        m_btn_down->setEnabled(false);
        m_lbl_detail_title->setText("No Mods Installed");
        m_lbl_detail_meta->setText("Folder is empty");
        m_lbl_detail_status->setVisible(false);
        m_txt_desc->setPlainText("No mods installed. You can install mods via zip packages or through the online Mod Browser.");
        return;
    }

    // Synchronize with LauncherConfig::mod_load_order
    LauncherConfig lcfg = launcher_config_load();
    std::vector<std::string> valid_order;
    for (const auto& id : lcfg.mod_load_order) {
        for (const auto& m : m_mods) {
            if (m.id == id && m.enabled) {
                if (std::find(valid_order.begin(), valid_order.end(), id) == valid_order.end()) {
                    valid_order.push_back(id);
                }
                break;
            }
        }
    }
    for (const auto& m : m_mods) {
        if (m.enabled) {
            if (std::find(valid_order.begin(), valid_order.end(), m.id) == valid_order.end()) {
                valid_order.push_back(m.id);
            }
        }
    }
    lcfg.mod_load_order = valid_order;
    launcher_config_save(lcfg);

    // Sort m_mods so enabled mods appear in load_order priority, followed by disabled mods
    std::vector<modman::ModMeta> sorted_mods;
    for (const auto& id : valid_order) {
        for (const auto& m : m_mods) {
            if (m.id == id) {
                sorted_mods.push_back(m);
                break;
            }
        }
    }
    for (const auto& m : m_mods) {
        if (!m.enabled) {
            sorted_mods.push_back(m);
        }
    }
    m_mods = sorted_mods;

    m_btn_toggle->setEnabled(true);
    m_btn_delete->setEnabled(true);
    m_btn_up->setEnabled(true);
    m_btn_down->setEnabled(true);
    m_lbl_detail_status->setVisible(true);

    for (size_t i = 0; i < m_mods.size(); ++i) {
        const auto& m = m_mods[i];
        QString status = m.enabled ? "ACTIVE" : "DISABLED";
        QString item_text = QString("%1  (v%2)\n[%3]  •  %4  •  %5")
                                .arg(QString::fromStdString(m.name))
                                .arg(QString::fromStdString(m.version.empty() ? "1.0" : m.version))
                                .arg(status)
                                .arg(QString::fromStdString(m.category.empty() ? "General" : m.category))
                                .arg(QString::fromStdString(m.author.empty() ? "Community" : m.author));

        auto* item = new QListWidgetItem(item_text);
        item->setIcon(QIcon(get_mod_icon(m, 44)));
        if (!m.enabled) {
            item->setForeground(QBrush(QColor("#6E7681")));
        } else {
            item->setForeground(QBrush(QColor("#F0F6FC")));
        }
        m_list->addItem(item);
    }

    m_list->setCurrentRow(0);
}

void ModsPage::on_selection_changed(int row) {
    if (row < 0 || row >= static_cast<int>(m_mods.size())) return;
    const auto& m = m_mods[row];

    m_lbl_detail_icon->setPixmap(get_mod_icon(m, 64));
    m_lbl_detail_title->setText(QString::fromStdString(m.name));
    m_lbl_detail_meta->setText(
        QString("Author: %1  |  Category: %2  |  v%3")
            .arg(QString::fromStdString(m.author.empty() ? "Unknown" : m.author))
            .arg(QString::fromStdString(m.category.empty() ? "General" : m.category))
            .arg(QString::fromStdString(m.version.empty() ? "1.0" : m.version))
    );

    if (m.enabled) {
        m_lbl_detail_status->setText("ACTIVE (LOADED IN VFS)");
        m_lbl_detail_status->setStyleSheet("background-color: #162E1C; color: #3FB950; border: 1px solid #244C2E; border-radius: 4px; padding: 2px 8px; font-size: 11px; font-weight: bold;");
    } else {
        m_lbl_detail_status->setText("DISABLED");
        m_lbl_detail_status->setStyleSheet("background-color: #242D3E; color: #8B949E; border: 1px solid #36445C; border-radius: 4px; padding: 2px 8px; font-size: 11px; font-weight: bold;");
    }

    // Markdown description parsing
    QString desc = QString::fromStdString(m.description.empty() ? "No description provided." : m.description);
    desc.replace("\\n", "\n");
    m_txt_desc->setMarkdown(desc);

    m_btn_toggle->setText(m.enabled ? "Disable Mod" : "Enable Mod");
}

void ModsPage::on_toggle_mod(int row) {
    if (row < 0 || row >= static_cast<int>(m_mods.size())) return;
    auto& m = m_mods[row];
    bool will_enable = !m.enabled;
    modman::set_mod_enabled(m.dir_path, will_enable);
    LauncherConfig lcfg = launcher_config_load();
    if (will_enable) {
        launcher_config_enable_mod(lcfg, m.id);
    } else {
        launcher_config_disable_mod(lcfg, m.id);
    }
    launcher_config_save(lcfg);
    refresh_mods();
    if (row < static_cast<int>(m_mods.size())) {
        m_list->setCurrentRow(row);
    }
}

void ModsPage::on_delete_mod_clicked() {
    int row = m_list->currentRow();
    if (row < 0 || row >= static_cast<int>(m_mods.size())) return;
    const auto& m = m_mods[row];

    auto reply = QMessageBox::question(
        this, "Uninstall Mod",
        QString("Are you sure you want to completely uninstall the mod \"%1\"?\nThis will remove its files from your disk.")
            .arg(QString::fromStdString(m.name)),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        LauncherConfig lcfg = launcher_config_load();
        launcher_config_disable_mod(lcfg, m.id);
        launcher_config_save(lcfg);
        modman::delete_mod(m);
        refresh_mods();
    }
}

void ModsPage::on_move_up_clicked() {
    int row = m_list->currentRow();
    if (row <= 0 || row >= static_cast<int>(m_mods.size())) return;
    if (!m_mods[row].enabled || !m_mods[row - 1].enabled) return;
    std::swap(m_mods[row], m_mods[row - 1]);
    LauncherConfig lcfg = launcher_config_load();
    lcfg.mod_load_order.clear();
    for (const auto& m : m_mods) {
        if (m.enabled) lcfg.mod_load_order.push_back(m.id);
    }
    launcher_config_save(lcfg);
    refresh_mods();
    m_list->setCurrentRow(row - 1);
}

void ModsPage::on_move_down_clicked() {
    int row = m_list->currentRow();
    if (row < 0 || row >= static_cast<int>(m_mods.size()) - 1) return;
    if (!m_mods[row].enabled || !m_mods[row + 1].enabled) return;
    std::swap(m_mods[row], m_mods[row + 1]);
    LauncherConfig lcfg = launcher_config_load();
    lcfg.mod_load_order.clear();
    for (const auto& m : m_mods) {
        if (m.enabled) lcfg.mod_load_order.push_back(m.id);
    }
    launcher_config_save(lcfg);
    refresh_mods();
    m_list->setCurrentRow(row + 1);
}

void ModsPage::on_install_zip_clicked() {
    QString path = QFileDialog::getOpenFileName(
        this, "Install Mod Archive",
        QDir::homePath(),
        "Mod Archives (*.zip *.swmod);;All Files (*)"
    );

    if (!path.isEmpty()) {
        modman::ModMeta out;
        std::string err;
        if (modman::install_mod_zip(path.toStdString(), get_mods_dir(), &out, &err)) {
            QMessageBox::information(this, "Mod Installed", QString("Successfully installed mod:\n%1 (v%2)").arg(QString::fromStdString(out.name)).arg(QString::fromStdString(out.version)));
            refresh_mods();
        } else {
            QMessageBox::warning(this, "Installation Failed", QString("Could not install mod:\n%1").arg(QString::fromStdString(err)));
        }
    }
}

void ModsPage::on_open_folder_clicked() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(get_mods_dir())));
}

} // namespace swordfare::launcher
