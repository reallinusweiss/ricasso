// ============================================================================
// save_editor_page.cpp — Save editor page implementation
// ============================================================================

#include "launcher/pages/save_editor_page.h"
#include "launcher/launcher_theme.h"
#include "platform/data_path.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QMessageBox>
#include <QFileInfo>
#include <QFile>

namespace swordfare::launcher {

SaveEditorPage::SaveEditorPage(QWidget* parent) : QWidget(parent) {
    setup_ui();
    refresh_saves();
}

void SaveEditorPage::setup_ui() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(28, 24, 28, 24);
    main_layout->setSpacing(20);

    // Header
    auto* header_layout = new QHBoxLayout();
    auto* title = new QLabel("GAME SAVE & CHARACTER EDITOR", this);
    title->setStyleSheet("font-size: 14px; font-weight: 800; color: #FFFFFF; letter-spacing: 0.8px;");
    header_layout->addWidget(title);
    header_layout->addStretch();

    auto* btn_reload = new QPushButton("Reload Saves", this);
    btn_reload->setProperty("class", "SecondaryBtn");
    connect(btn_reload, &QPushButton::clicked, this, &SaveEditorPage::refresh_saves);
    header_layout->addWidget(btn_reload);

    main_layout->addLayout(header_layout);

    // Slot selector row
    auto* slot_row = new QHBoxLayout();
    auto* slot_lbl = new QLabel("Select Save File:", this);
    slot_lbl->setStyleSheet("font-size: 13px; font-weight: bold; color: #F0F6FC;");
    slot_row->addWidget(slot_lbl);

    m_combo_slots = new QComboBox(this);
    connect(m_combo_slots, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SaveEditorPage::on_slot_changed);
    slot_row->addWidget(m_combo_slots, 1);

    m_btn_backup = new QPushButton("Create Backup Copy", this);
    m_btn_backup->setProperty("class", "SecondaryBtn");
    connect(m_btn_backup, &QPushButton::clicked, this, &SaveEditorPage::on_backup_clicked);
    slot_row->addWidget(m_btn_backup);

    main_layout->addLayout(slot_row);

    // Editor Card
    auto* card = new QFrame(this);
    card->setStyleSheet("background-color: #161B22; border: 1px solid #283141; border-radius: 12px;");
    auto* card_layout = new QVBoxLayout(card);
    card_layout->setContentsMargins(24, 20, 24, 20);
    card_layout->setSpacing(16);

    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(24);
    grid->setVerticalSpacing(14);

    // Coins
    auto* l_coins = new QHBoxLayout();
    auto* ic_coins = new QLabel(card);
    ic_coins->setPixmap(LauncherTheme::fa_pixmap(ICON_FA_GEM, QColor(88, 166, 255), 14));
    l_coins->addWidget(ic_coins);
    l_coins->addWidget(new QLabel("Soul Shards / Coins:", card));
    l_coins->addStretch();
    grid->addLayout(l_coins, 0, 0);
    m_spin_coins = new QSpinBox(card);
    m_spin_coins->setRange(0, 999999);
    grid->addWidget(m_spin_coins, 0, 1);

    // Health
    auto* l_health = new QHBoxLayout();
    auto* ic_health = new QLabel(card);
    ic_health->setPixmap(LauncherTheme::fa_pixmap(ICON_FA_HEART, QColor(233, 69, 96), 14));
    l_health->addWidget(ic_health);
    l_health->addWidget(new QLabel("Health Points:", card));
    l_health->addStretch();
    grid->addLayout(l_health, 1, 0);
    m_spin_health = new QSpinBox(card);
    m_spin_health->setRange(1, 100);
    grid->addWidget(m_spin_health, 1, 1);

    // Mana
    auto* l_mana = new QHBoxLayout();
    auto* ic_mana = new QLabel(card);
    ic_mana->setPixmap(LauncherTheme::fa_pixmap(ICON_FA_BOLT, QColor(88, 166, 255), 14));
    l_mana->addWidget(ic_mana);
    l_mana->addWidget(new QLabel("Mana Points:", card));
    l_mana->addStretch();
    grid->addLayout(l_mana, 2, 0);
    m_spin_mana = new QSpinBox(card);
    m_spin_mana->setRange(0, 100);
    grid->addWidget(m_spin_mana, 2, 1);

    // Level
    auto* l_level = new QHBoxLayout();
    auto* ic_level = new QLabel(card);
    ic_level->setPixmap(LauncherTheme::fa_pixmap(ICON_FA_STAR, QColor(241, 196, 15), 14));
    l_level->addWidget(ic_level);
    l_level->addWidget(new QLabel("Hero Level:", card));
    l_level->addStretch();
    grid->addLayout(l_level, 3, 0);
    m_spin_level = new QSpinBox(card);
    m_spin_level->setRange(1, 50);
    grid->addWidget(m_spin_level, 3, 1);

    // XP
    auto* l_xp = new QHBoxLayout();
    auto* ic_xp = new QLabel(card);
    ic_xp->setPixmap(LauncherTheme::fa_pixmap(ICON_FA_GAUGE_HIGH, QColor(61, 184, 79), 14));
    l_xp->addWidget(ic_xp);
    l_xp->addWidget(new QLabel("Experience (XP):", card));
    l_xp->addStretch();
    grid->addLayout(l_xp, 4, 0);
    m_spin_xp = new QSpinBox(card);
    m_spin_xp->setRange(0, 999999);
    grid->addWidget(m_spin_xp, 4, 1);

    card_layout->addLayout(grid);

    m_lbl_status = new QLabel(card);
    m_lbl_status->setStyleSheet("color: #8B949E; font-size: 12px;");
    card_layout->addWidget(m_lbl_status);

    auto* btn_row = new QHBoxLayout();
    btn_row->addStretch();

    m_btn_save = new QPushButton("Save Changes to Disk", card);
    m_btn_save->setIcon(LauncherTheme::fa_icon(ICON_FA_FLOPPY_DISK, Qt::white, 14));
    m_btn_save->setStyleSheet("background-color: #2EA043; color: white; border: none; font-weight: bold; border-radius: 6px; padding: 8px 20px;");
    connect(m_btn_save, &QPushButton::clicked, this, &SaveEditorPage::on_save_clicked);
    btn_row->addWidget(m_btn_save);

    card_layout->addLayout(btn_row);
    main_layout->addWidget(card);
    main_layout->addStretch();
}

void SaveEditorPage::refresh_saves() {
    m_combo_slots->clear();
    m_save_paths = save_list_dir(get_vfs_save_dir());

    if (m_save_paths.empty()) {
        m_combo_slots->addItem("No .gplayer save files found");
        m_btn_save->setEnabled(false);
        m_btn_backup->setEnabled(false);
        m_spin_coins->setEnabled(false);
        m_spin_health->setEnabled(false);
        m_spin_mana->setEnabled(false);
        m_spin_level->setEnabled(false);
        m_spin_xp->setEnabled(false);
        m_lbl_status->setText("No saves present in save directory.");
        m_has_save = false;
        return;
    }

    m_btn_save->setEnabled(true);
    m_btn_backup->setEnabled(true);
    m_spin_coins->setEnabled(true);
    m_spin_health->setEnabled(true);
    m_spin_mana->setEnabled(true);
    m_spin_level->setEnabled(true);
    m_spin_xp->setEnabled(true);

    for (const auto& p : m_save_paths) {
        m_combo_slots->addItem(QFileInfo(QString::fromStdString(p)).fileName());
    }

    on_slot_changed(0);
}

void SaveEditorPage::on_slot_changed(int index) {
    if (index < 0 || index >= static_cast<int>(m_save_paths.size())) return;
    const std::string& path = m_save_paths[index];
    if (save_load(path, m_current_save)) {
        m_has_save = true;
        populate_fields();
        m_lbl_status->setText(QString("Loaded '%1' — %2% completed")
                                  .arg(QString::fromStdString(m_current_save.name))
                                  .arg(static_cast<int>(m_current_save.percent_completed * 100.0f)));
    } else {
        m_has_save = false;
        m_lbl_status->setText("Failed to parse save file.");
    }
}

void SaveEditorPage::populate_fields() {
    if (!m_has_save) return;
    const auto& ch = m_current_save.game_state.character;
    m_spin_coins->setValue(ch.coins);
    m_spin_health->setValue(ch.health);
    m_spin_mana->setValue(ch.mana);
    m_spin_level->setValue(ch.level);
    m_spin_xp->setValue(ch.xp);
}

void SaveEditorPage::on_save_clicked() {
    int idx = m_combo_slots->currentIndex();
    if (idx < 0 || idx >= static_cast<int>(m_save_paths.size()) || !m_has_save) return;

    auto& ch = m_current_save.game_state.character;
    ch.coins = m_spin_coins->value();
    ch.health = m_spin_health->value();
    ch.mana = m_spin_mana->value();
    ch.level = m_spin_level->value();
    ch.xp = m_spin_xp->value();

    if (save_write(m_save_paths[idx], m_current_save)) {
        QMessageBox::information(this, "Save Updated", "Character stats saved to file successfully!");
    } else {
        QMessageBox::critical(this, "Save Error", "Failed to write updated save to disk.");
    }
}

void SaveEditorPage::on_backup_clicked() {
    int idx = m_combo_slots->currentIndex();
    if (idx < 0 || idx >= static_cast<int>(m_save_paths.size())) return;

    QString src = QString::fromStdString(m_save_paths[idx]);
    QString dest = src + ".backup";
    if (QFile::copy(src, dest)) {
        QMessageBox::information(this, "Backup Created", QString("Saved backup copy to:\n%1").arg(dest));
    } else {
        QMessageBox::warning(this, "Backup Warning", "A backup already exists or could not be written.");
    }
}

} // namespace swordfare::launcher
