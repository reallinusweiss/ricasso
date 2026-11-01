// ============================================================================
// library_page.cpp — Game Library implementation (1:1 with ImGui Library)
// ============================================================================

#include "launcher/pages/library_page.h"
#include "launcher/launcher_theme.h"
#include "platform/data_path.h"
#include "platform/launcher_config.h"
#include "platform/mod_manager.h"

#include <filesystem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QPainter>
#include <QRegularExpression>
#include <algorithm>

namespace swordfare::launcher {

// Roles for custom delegate
enum InstanceItemRoles {
    RoleTitle = Qt::UserRole + 1,
    RoleStatus = Qt::UserRole + 2,
    RoleArch = Qt::UserRole + 3,
    RoleIsBase = Qt::UserRole + 4,
    RoleIsDefault = Qt::UserRole + 5,
    RoleExtraTags = Qt::UserRole + 6,
    RoleIcon = Qt::UserRole + 7
};

InstanceItemDelegate::InstanceItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

QSize InstanceItemDelegate::sizeHint(const QStyleOptionViewItem& /*option*/, const QModelIndex& /*index*/) const {
    return QSize(290, 56);
}

void InstanceItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    bool selected = (option.state & QStyle::State_Selected);
    bool hovered = (option.state & QStyle::State_MouseOver);

    QRect rect = option.rect.adjusted(2, 2, -2, -2);

    // Background
    QColor bg = selected ? QColor(36, 45, 62) : (hovered ? QColor(26, 34, 48) : QColor(18, 24, 34));
    painter->setBrush(bg);
    QColor border_col = selected ? QColor(233, 69, 96) : (hovered ? QColor(59, 71, 93) : QColor(35, 42, 54));
    painter->setPen(QPen(border_col, selected ? 2 : 1));
    painter->drawRoundedRect(rect, 8, 8);

    // Icon (38x38)
    QPixmap icon_pm = index.data(RoleIcon).value<QPixmap>();
    int icon_size = 38;
    int icon_x = rect.left() + 9;
    int icon_y = rect.top() + (rect.height() - icon_size) / 2;
    if (!icon_pm.isNull()) {
        painter->drawPixmap(icon_x, icon_y, icon_pm.scaled(icon_size, icon_size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    int text_x = icon_x + icon_size + 10;
    int text_w = rect.right() - text_x - 10;

    // Line 1: Clean Title
    QString title = index.data(RoleTitle).toString();
    bool is_default = index.data(RoleIsDefault).toBool();

    painter->setFont(QFont("Segoe UI, Inter, sans-serif", 10, QFont::Bold));
    painter->setPen(selected ? QColor(255, 255, 255) : QColor(240, 246, 252));

    QRect title_rect(text_x, rect.top() + 6, text_w - (is_default ? 24 : 0), 20);
    QString elided_title = painter->fontMetrics().elidedText(title, Qt::ElideRight, title_rect.width());
    painter->drawText(title_rect, Qt::AlignVCenter | Qt::AlignLeft, elided_title);

    if (is_default) {
        painter->setPen(QColor(241, 196, 15));
        painter->drawText(QRect(rect.right() - 20, rect.top() + 6, 16, 20), Qt::AlignCenter, "★");
    }

    // Line 2: Badges (Status bar, Arch bar, Instance / GV / custom tag bars)
    int badge_x = text_x;
    int badge_y = rect.top() + 29;
    int badge_h = 18;

    auto draw_badge = [&](const QString& text, const QColor& bg_col, const QColor& text_col, const QColor& border_c) {
        QFont badge_font("Segoe UI, Inter, sans-serif", 8, QFont::Bold);
        painter->setFont(badge_font);
        QFontMetrics fm(badge_font);
        int bw = fm.horizontalAdvance(text) + 12;
        QRect brect(badge_x, badge_y, bw, badge_h);

        painter->setBrush(bg_col);
        painter->setPen(QPen(border_c, 1));
        painter->drawRoundedRect(brect, 4, 4);

        painter->setPen(text_col);
        painter->drawText(brect, Qt::AlignCenter, text);

        badge_x += bw + 5;
    };

    // 1. Status badge [TESTED] / [TESTING] / [UNKNOWN]
    QString status = index.data(RoleStatus).toString();
    if (status == "Stable" || status == "TESTED") {
        draw_badge("TESTED", QColor(22, 46, 28), QColor(63, 185, 80), QColor(36, 76, 46));
    } else if (status == "Testing" || status == "TESTING") {
        draw_badge("TESTING", QColor(54, 41, 18), QColor(209, 154, 33), QColor(92, 69, 29));
    } else {
        draw_badge("UNKNOWN", QColor(36, 45, 62), QColor(139, 148, 158), QColor(54, 68, 92));
    }

    // 2. Type badge: [BASE] or [INSTANCE]
    bool is_base = index.data(RoleIsBase).toBool();
    if (is_base) {
        draw_badge("BASE", QColor(40, 26, 60), QColor(187, 128, 255), QColor(70, 45, 105));
    } else {
        draw_badge("INSTANCE", QColor(20, 36, 56), QColor(88, 166, 255), QColor(35, 62, 98));
    }

    // 3. Extra tags like [GV], [MOD], [RP], [RL], etc.
    QStringList extra_tags = index.data(RoleExtraTags).toStringList();
    for (const QString& tag : extra_tags) {
        if (badge_x + 30 > rect.right()) break;
        draw_badge(tag, QColor(48, 24, 32), QColor(233, 69, 96), QColor(85, 34, 48));
    }

    // 4. Arch badge (ARM64 / ARM32)
    QString arch = index.data(RoleArch).toString();
    if (badge_x + 40 <= rect.right()) {
        draw_badge(arch, QColor(25, 32, 44), QColor(139, 148, 158), QColor(43, 55, 75));
    }

    painter->restore();
}

LibraryPage::LibraryPage(BinarySelector& selector, QWidget* parent)
    : QWidget(parent), m_selector(selector) {
    setup_ui();
    refresh_instances();
}

void LibraryPage::setup_ui() {
    auto* root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(28, 22, 28, 22);
    root_layout->setSpacing(16);

    // ── Header Row ───────────────────────────────────────────────────────────
    auto* header_layout = new QHBoxLayout();
    
    auto* title_box = new QHBoxLayout();
    title_box->setSpacing(10);
    auto* icon_title = new QLabel(this);
    icon_title->setPixmap(LauncherTheme::fa_pixmap(ICON_FA_LAYER_GROUP, QColor(233, 69, 96), 20));
    title_box->addWidget(icon_title);

    auto* title_text = new QLabel("Game Library", this);
    title_text->setStyleSheet("font-size: 20px; font-weight: 800; color: #FFFFFF; letter-spacing: 0.5px;");
    title_box->addWidget(title_text);

    m_lbl_header_count = new QLabel(this);
    m_lbl_header_count->setStyleSheet("font-size: 13px; color: #8B949E; margin-left: 8px; font-weight: 600;");
    title_box->addWidget(m_lbl_header_count);
    header_layout->addLayout(title_box);

    header_layout->addStretch();

    auto* btn_add = new QPushButton("  Add Instance", this);
    btn_add->setIcon(LauncherTheme::fa_icon(ICON_FA_PLUS, Qt::white, 14));
    btn_add->setStyleSheet(
        "QPushButton {"
        "  background-color: #2EA043; color: white; border: none; font-weight: bold; border-radius: 6px;"
        "  padding: 8px 16px; font-size: 13px;"
        "}"
        "QPushButton:hover { background-color: #3FB950; }"
        "QPushButton:pressed { background-color: #238636; }"
    );
    btn_add->setCursor(Qt::PointingHandCursor);
    connect(btn_add, &QPushButton::clicked, this, &LibraryPage::on_add_instance_clicked);
    header_layout->addWidget(btn_add);

    root_layout->addLayout(header_layout);

    // ── Two-Column Split Layout ──────────────────────────────────────────────
    auto* split_layout = new QHBoxLayout();
    split_layout->setSpacing(18);

    // Left Column: Instance List
    auto* list_frame = new QFrame(this);
    list_frame->setObjectName("ListFrame");
    list_frame->setStyleSheet(
        "#ListFrame {"
        "  background-color: #0E1420;"
        "  border: 1px solid #1C2433;"
        "  border-radius: 10px;"
        "}"
    );
    list_frame->setFixedWidth(330);
    auto* list_col_layout = new QVBoxLayout(list_frame);
    list_col_layout->setContentsMargins(12, 14, 12, 14);
    list_col_layout->setSpacing(10);

    auto* col_header = new QLabel("INSTALLED VERSIONS", list_frame);
    col_header->setStyleSheet("font-size: 11px; font-weight: bold; color: #6E7681; letter-spacing: 0.8px; margin-left: 4px;");
    list_col_layout->addWidget(col_header);

    m_list_instances = new QListWidget(list_frame);
    m_list_instances->setItemDelegate(new InstanceItemDelegate(this));
    m_list_instances->setSpacing(6);
    m_list_instances->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_list_instances, &QListWidget::currentRowChanged, this, &LibraryPage::on_instance_selected);
    list_col_layout->addWidget(m_list_instances, 1);

    split_layout->addWidget(list_frame);

    // Right Column: Detail Panel
    auto* detail_frame = new QFrame(this);
    detail_frame->setObjectName("DetailFrame");
    detail_frame->setStyleSheet(
        "#DetailFrame {"
        "  background-color: #121824;"
        "  border: 1px solid #1F2737;"
        "  border-radius: 12px;"
        "}"
    );
    auto* dt_layout = new QVBoxLayout(detail_frame);
    dt_layout->setContentsMargins(26, 24, 26, 24);
    dt_layout->setSpacing(18);

    // Header with big Icon + Title + Badges
    auto* dt_top = new QHBoxLayout();
    dt_top->setSpacing(18);

    m_detail_icon = new QLabel(detail_frame);
    m_detail_icon->setFixedSize(68, 68);
    m_detail_icon->setPixmap(LauncherTheme::game_icon(68));
    dt_top->addWidget(m_detail_icon);

    auto* dt_meta = new QVBoxLayout();
    dt_meta->setSpacing(6);
    m_detail_title = new QLabel("Select an Instance", detail_frame);
    m_detail_title->setStyleSheet("font-size: 19px; font-weight: 800; color: #FFFFFF;");
    dt_meta->addWidget(m_detail_title);

    m_detail_subtitle = new QLabel(detail_frame);
    m_detail_subtitle->setStyleSheet("font-size: 12px; color: #8B949E;");
    dt_meta->addWidget(m_detail_subtitle);

    auto* badges_row = new QHBoxLayout();
    badges_row->setSpacing(8);

    m_detail_arch_badge = new QLabel(detail_frame);
    m_detail_arch_badge->setStyleSheet("background-color: #1D2A44; color: #58A6FF; border: 1px solid #2B3D66; border-radius: 4px; padding: 2px 8px; font-size: 11px; font-weight: bold;");
    badges_row->addWidget(m_detail_arch_badge);

    m_detail_default_badge = new QLabel(detail_frame);
    m_detail_default_badge->setStyleSheet("background-color: #382E0B; color: #F1C40F; border: 1px solid #5C4B13; border-radius: 4px; padding: 2px 8px; font-size: 11px; font-weight: bold;");
    badges_row->addWidget(m_detail_default_badge);

    m_detail_status_badge = new QLabel(detail_frame);
    m_detail_status_badge->setStyleSheet("background-color: #162E1C; color: #3FB950; border: 1px solid #244C2E; border-radius: 4px; padding: 2px 8px; font-size: 11px; font-weight: bold;");
    badges_row->addWidget(m_detail_status_badge);

    badges_row->addStretch();
    dt_meta->addLayout(badges_row);

    dt_top->addLayout(dt_meta, 1);
    dt_layout->addLayout(dt_top);

    // Separator line
    auto* sep = new QFrame(detail_frame);
    sep->setFixedHeight(1);
    sep->setStyleSheet("background-color: #1F2737;");
    dt_layout->addWidget(sep);

    // Detailed Paths & Specs Table
    auto* info_grid = new QGridLayout();
    info_grid->setHorizontalSpacing(16);
    info_grid->setVerticalSpacing(10);

    auto add_row = [info_grid, detail_frame](int row, const QString& label, QLabel*& value_lbl) {
        auto* l = new QLabel(label, detail_frame);
        l->setStyleSheet("font-size: 11px; font-weight: 700; color: #6E7681; letter-spacing: 0.5px;");
        value_lbl = new QLabel("—", detail_frame);
        value_lbl->setStyleSheet("font-size: 12px; color: #E6EDF3; font-family: monospace;");
        value_lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        info_grid->addWidget(l, row, 0);
        info_grid->addWidget(value_lbl, row, 1);
    };

    add_row(0, "BINARY PATH", m_detail_path);
    add_row(1, "ASSETS DIR", m_detail_assets);
    add_row(2, "GAME ENGINE", m_detail_gametype);

    // Preferred Base Engine row
    m_detail_pref_label = new QLabel("PREFERRED BASE", detail_frame);
    m_detail_pref_label->setStyleSheet("font-size: 11px; font-weight: 700; color: #6E7681; letter-spacing: 0.5px;");
    m_combo_pref_base = new QComboBox(detail_frame);
    m_combo_pref_base->addItem("Swordigo v1.4.12 (SRE12)", "1.4.12");
    m_combo_pref_base->addItem("Swordigo v1.4.13 (SRE13)", "1.4.13");
    m_combo_pref_base->setStyleSheet(
        "QComboBox { background-color: #161D2A; color: #58A6FF; font-weight: bold; border: 1px solid #2B3D66; border-radius: 4px; padding: 4px 8px; }"
    );
    connect(m_combo_pref_base, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (idx < 0) return;
        const auto& binaries = m_selector.get_binaries();
        if (m_selected_index < 0 || m_selected_index >= static_cast<int>(binaries.size())) return;
        if (binaries[m_selected_index].is_base) return;

        std::string new_base = m_combo_pref_base->currentData().toString().toStdString();
        m_selector.set_instance_preferred_base(m_selected_index, new_base);

        // Update detail path label immediately
        const auto& updated_b = m_selector.get_binaries()[m_selected_index];
        m_detail_path->setText(QString::fromStdString(updated_b.filepath));
        emit instancesChanged();
    });
    info_grid->addWidget(m_detail_pref_label, 3, 0);
    info_grid->addWidget(m_combo_pref_base, 3, 1);

    dt_layout->addLayout(info_grid);
    dt_layout->addStretch();

    // Action Buttons Row (1:1 with ImGui actions)
    auto* actions_layout = new QHBoxLayout();
    actions_layout->setSpacing(12);

    m_btn_launch = new QPushButton("  Launch Instance", detail_frame);
    m_btn_launch->setIcon(LauncherTheme::fa_icon(ICON_FA_PLAY, Qt::white, 14));
    m_btn_launch->setObjectName("LaunchBtn");
    m_btn_launch->setCursor(Qt::PointingHandCursor);
    connect(m_btn_launch, &QPushButton::clicked, this, &LibraryPage::on_launch_clicked);
    actions_layout->addWidget(m_btn_launch);

    m_btn_set_default = new QPushButton("  Set as Default", detail_frame);
    m_btn_set_default->setIcon(LauncherTheme::fa_icon(ICON_FA_STAR, QColor(241, 196, 15), 14));
    m_btn_set_default->setProperty("class", "SecondaryBtn");
    m_btn_set_default->setCursor(Qt::PointingHandCursor);
    connect(m_btn_set_default, &QPushButton::clicked, this, &LibraryPage::on_set_default_clicked);
    actions_layout->addWidget(m_btn_set_default);

    m_btn_open_folder = new QPushButton("  Open Folder", detail_frame);
    m_btn_open_folder->setIcon(LauncherTheme::fa_icon(ICON_FA_FOLDER_OPEN, QColor(139, 148, 158), 14));
    m_btn_open_folder->setProperty("class", "SecondaryBtn");
    m_btn_open_folder->setCursor(Qt::PointingHandCursor);
    connect(m_btn_open_folder, &QPushButton::clicked, this, &LibraryPage::on_open_folder_clicked);
    actions_layout->addWidget(m_btn_open_folder);

    m_btn_remove = new QPushButton("  Remove", detail_frame);
    m_btn_remove->setIcon(LauncherTheme::fa_icon(ICON_FA_TRASH, QColor(255, 107, 107), 14));
    m_btn_remove->setProperty("class", "DangerBtn");
    m_btn_remove->setCursor(Qt::PointingHandCursor);
    connect(m_btn_remove, &QPushButton::clicked, this, &LibraryPage::on_remove_instance_clicked);
    actions_layout->addWidget(m_btn_remove);

    dt_layout->addLayout(actions_layout);
    split_layout->addWidget(detail_frame, 1);

    root_layout->addLayout(split_layout, 1);
}

void LibraryPage::refresh_instances() {
    m_list_instances->clear();
    const auto& binaries = m_selector.get_binaries();
    m_lbl_header_count->setText(QString("(%1 total)").arg(binaries.size()));

    if (binaries.empty()) {
        m_btn_launch->setEnabled(false);
        m_btn_set_default->setEnabled(false);
        m_btn_open_folder->setEnabled(false);
        m_btn_remove->setEnabled(false);
        m_detail_title->setText("No Game Instances");
        m_detail_subtitle->setText("Click '+ Add Instance' to import an APK or libswordigo.so");
        return;
    }

    m_btn_launch->setEnabled(true);
    m_btn_set_default->setEnabled(true);
    m_btn_open_folder->setEnabled(true);

    for (size_t i = 0; i < binaries.size(); ++i) {
        const auto& b = binaries[i];
        QString raw_label = QString::fromStdString(b.label.empty() ? b.filename : b.label);

        // Extract bracketed tags: e.g. [Custom], [GV], [MOD], [RP], [RL], etc.
        QStringList extra_tags;
        static const QRegularExpression bracket_rx(R"(\[([^\]]+)\])");
        auto match_it = bracket_rx.globalMatch(raw_label);
        while (match_it.hasNext()) {
            auto m = match_it.next();
            QString tag = m.captured(1).trimmed();
            // Filter out arch and generic status or base
            if (tag.compare("ARM64", Qt::CaseInsensitive) != 0 &&
                tag.compare("ARM32", Qt::CaseInsensitive) != 0 &&
                tag.compare("Stable", Qt::CaseInsensitive) != 0 &&
                tag.compare("Tested", Qt::CaseInsensitive) != 0 &&
                tag.compare("Testing", Qt::CaseInsensitive) != 0 &&
                tag.compare("Unknown", Qt::CaseInsensitive) != 0 &&
                tag.compare("Latest", Qt::CaseInsensitive) != 0 &&
                tag.compare("Custom", Qt::CaseInsensitive) != 0 &&
                tag.compare("Base", Qt::CaseInsensitive) != 0 &&
                tag.compare("Base Engine", Qt::CaseInsensitive) != 0 &&
                tag.compare("Instance", Qt::CaseInsensitive) != 0) {
                extra_tags.append(tag);
            }
        }

        // Clean line 1 title by stripping ALL bracketed tokens [ ... ] and parentheses ( ... )
        QString clean_title = raw_label;
        clean_title.remove(QRegularExpression(R"(\[[^\]]*\])"));
        clean_title.remove(QRegularExpression(R"(\([^\)]*\))"));
        clean_title = clean_title.trimmed();
        if (clean_title.isEmpty()) {
            clean_title = QString::fromStdString(b.filename);
        }

        QString arch_str = (b.arch == BinaryArch::ARM64) ? "ARM64" : "ARM32";
        QString status_str = (b.status == BinaryStatus::TESTED) ? "Stable" : (b.status == BinaryStatus::TESTING ? "Testing" : "Unknown");

        auto* item = new QListWidgetItem();
        item->setData(Qt::UserRole, static_cast<int>(i));
        item->setData(RoleTitle, clean_title);
        item->setData(RoleStatus, status_str);
        item->setData(RoleArch, arch_str);
        item->setData(RoleIsBase, b.is_base);
        item->setData(RoleIsDefault, b.is_default);
        item->setData(RoleExtraTags, extra_tags);

        QPixmap ic = LauncherTheme::instance_icon(QString::fromStdString(b.icon_path),
                                                  QString::fromStdString(b.assets_dir), 38,
                                                  QString::fromStdString(b.game_type),
                                                  QString::fromStdString(b.id));
        item->setData(RoleIcon, ic);

        m_list_instances->addItem(item);
    }

    int sel = std::clamp(m_selected_index, 0, static_cast<int>(binaries.size()) - 1);
    m_list_instances->setCurrentRow(sel);
    update_detail_view();
}

void LibraryPage::on_instance_selected(int row) {
    if (row < 0 || row >= static_cast<int>(m_selector.get_binaries().size())) return;
    m_selected_index = row;
    update_detail_view();
}

void LibraryPage::update_detail_view() {
    const auto& binaries = m_selector.get_binaries();
    if (m_selected_index < 0 || m_selected_index >= static_cast<int>(binaries.size())) return;

    const auto& b = binaries[m_selected_index];
    QString title = QString::fromStdString(b.label.empty() ? b.filename : b.label);
    m_detail_title->setText(title);

    QPixmap detail_ic = LauncherTheme::instance_icon(QString::fromStdString(b.icon_path),
                                                     QString::fromStdString(b.assets_dir), 68,
                                                     QString::fromStdString(b.game_type),
                                                     QString::fromStdString(b.id));
    m_detail_icon->setPixmap(detail_ic);

    QString sub = QString("Version: %1  |  Architecture: %2")
                      .arg(QString::fromStdString(b.version.empty() ? "Custom" : b.version))
                      .arg(b.arch == BinaryArch::ARM64 ? "ARM64 (v8a)" : "ARM32 (v7a)");
    m_detail_subtitle->setText(sub);

    m_detail_arch_badge->setText(b.arch == BinaryArch::ARM64 ? "ARM64" : "ARM32");
    m_detail_default_badge->setVisible(b.is_default);
    m_detail_default_badge->setText("DEFAULT INSTANCE");

    switch (b.status) {
        case BinaryStatus::TESTED:
            m_detail_status_badge->setText("TESTED & VERIFIED");
            m_detail_status_badge->setStyleSheet("background-color: #162E1C; color: #3FB950; border: 1px solid #244C2E; border-radius: 4px; padding: 2px 8px; font-size: 11px; font-weight: bold;");
            break;
        case BinaryStatus::TESTING:
            m_detail_status_badge->setText("IN TESTING");
            m_detail_status_badge->setStyleSheet("background-color: #362912; color: #D19A21; border: 1px solid #5C451D; border-radius: 4px; padding: 2px 8px; font-size: 11px; font-weight: bold;");
            break;
        default:
            m_detail_status_badge->setText("UNKNOWN STATUS");
            m_detail_status_badge->setStyleSheet("background-color: #242D3E; color: #8B949E; border: 1px solid #36445C; border-radius: 4px; padding: 2px 8px; font-size: 11px; font-weight: bold;");
            break;
    }

    m_detail_path->setText(QString::fromStdString(b.filepath));
    m_detail_assets->setText(QString::fromStdString(b.assets_dir.empty() ? "Default VFS assets" : b.assets_dir));
    m_detail_gametype->setText(QString::fromStdString(b.game_type.empty() ? "Swordigo" : b.game_type));

    // Preferred base dropdown sync
    m_combo_pref_base->blockSignals(true);
    std::string pref = b.preferred_base.empty() ? b.version : b.preferred_base;
    if (pref == "1.4.13" || pref == "v1.4.13") {
        m_combo_pref_base->setCurrentIndex(1);
    } else {
        m_combo_pref_base->setCurrentIndex(0);
    }
    m_combo_pref_base->blockSignals(false);

    // Disable for base instances as requested
    bool can_edit_base = !b.is_base;
    m_combo_pref_base->setEnabled(can_edit_base);
    m_detail_pref_label->setEnabled(can_edit_base);

    // Can only delete custom non-vanilla instances
    m_btn_remove->setEnabled(!b.is_base);
}

void LibraryPage::on_launch_clicked() {
    const auto& binaries = m_selector.get_binaries();
    if (m_selected_index < 0 || m_selected_index >= static_cast<int>(binaries.size())) return;

    const auto& b = binaries[m_selected_index];
    LaunchConfig cfg;
    std::string pref = b.preferred_base.empty() ? b.version : b.preferred_base;
    cfg.selected_binary = m_selector.resolve_launch_binary(b, pref);
    cfg.assets_dir = b.assets_dir;
    cfg.game_type = b.game_type;
    cfg.selected_base_version = pref;
    cfg.instance_name = b.label;
    cfg.graphics_api = GraphicsAPI::OPENGL;

    if (b.arch == BinaryArch::ARM32) {
        cfg.use_dynarmic = true;
        cfg.use_sre = false;     // SRE is ARM64 guest hooks
    } else {
        cfg.use_dynarmic = true;
        cfg.use_sre = true;
    }
    cfg.advanced_redstell_opts = false;
    cfg.should_launch = true;

    // Resolve active mod (prioritizing LauncherConfig load order, then first enabled mod on disk)
    LauncherConfig lcfg = launcher_config_load();
    std::string data_dir = get_user_data_dir();
    for (const auto& mod_id : lcfg.mod_load_order) {
        std::string mod_dir = data_dir + "mods/" + mod_id;
        if (std::filesystem::exists(mod_dir) && std::filesystem::is_directory(mod_dir)) {
            cfg.selected_mod = mod_id;
            break;
        }
    }
    if (cfg.selected_mod.empty()) {
        auto mods = modman::list_mods(data_dir + "mods");
        for (const auto& m : mods) {
            if (m.enabled) {
                cfg.selected_mod = m.id;
                break;
            }
        }
    }

    emit launchRequested(cfg);
}

void LibraryPage::on_set_default_clicked() {
    const auto& binaries = m_selector.get_binaries();
    if (m_selected_index < 0 || m_selected_index >= static_cast<int>(binaries.size())) return;
    m_selector.set_default(binaries[m_selected_index].filepath);
    refresh_instances();
    emit instancesChanged();
}

void LibraryPage::on_open_folder_clicked() {
    const auto& binaries = m_selector.get_binaries();
    if (m_selected_index < 0 || m_selected_index >= static_cast<int>(binaries.size())) return;

    QString fp = QString::fromStdString(binaries[m_selected_index].filepath);
    QFileInfo fi(fp);
    QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
}

void LibraryPage::on_remove_instance_clicked() {
    const auto& binaries = m_selector.get_binaries();
    if (m_selected_index < 0 || m_selected_index >= static_cast<int>(binaries.size())) return;

    const auto& b = binaries[m_selected_index];
    auto reply = QMessageBox::question(
        this, "Remove Game Instance",
        QString("Are you sure you want to remove the instance \"%1\"?").arg(QString::fromStdString(b.label)),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        m_selector.remove_instance(m_selected_index);
        m_selected_index = std::max(0, m_selected_index - 1);
        refresh_instances();
        emit instancesChanged();
    }
}

void LibraryPage::on_add_instance_clicked() {
    QString path = QFileDialog::getOpenFileName(
        this, "Add Game Binary / APK",
        QString::fromStdString(get_user_data_dir()),
        "Game Files (*.so *.apk);;All Files (*)"
    );

    if (!path.isEmpty()) {
        if (path.endsWith(".apk", Qt::CaseInsensitive)) {
            std::string err;
            m_selector.import_apk_instance(path.toStdString(), "Imported APK", &err);
        } else {
            m_selector.add_custom_instance(path.toStdString(), "Custom", "assets");
        }
        refresh_instances();
        emit instancesChanged();
    }
}

} // namespace swordfare::launcher
