// ============================================================================
// profile_page.cpp — Profile page implementation
// ============================================================================

#include "launcher/pages/profile_page.h"
#include "launcher/launcher_theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QFileDialog>
#include <QMessageBox>

namespace swordfare::launcher {

ProfilePage::ProfilePage(QWidget* parent) : QWidget(parent) {
    setup_ui();
    update_ui();

    connect(&ProfileManager::instance(), &ProfileManager::profileChanged, this, &ProfilePage::update_ui);
    connect(&ProfileManager::instance(), &ProfileManager::statsUpdated, this, &ProfilePage::update_ui);
}

void ProfilePage::setup_ui() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(28, 24, 28, 24);
    main_layout->setSpacing(20);

    // ── 1. Header Card (Avatar + Identity) ──────────────────────────────────
    auto* card = new QFrame(this);
    card->setObjectName("ProfileCard");
    card->setStyleSheet(
        "#ProfileCard {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #181E29, stop:1 #131720);"
        "  border: 1px solid #283141;"
        "  border-radius: 14px;"
        "}"
    );
    auto* card_layout = new QHBoxLayout(card);
    card_layout->setContentsMargins(24, 20, 24, 20);
    card_layout->setSpacing(24);

    m_lbl_avatar = new QLabel(card);
    m_lbl_avatar->setFixedSize(96, 96);
    card_layout->addWidget(m_lbl_avatar);

    auto* user_info_layout = new QVBoxLayout();
    user_info_layout->setSpacing(6);

    auto* name_row = new QHBoxLayout();
    m_lbl_username = new QLabel("Hero", card);
    m_lbl_username->setStyleSheet("font-size: 22px; font-weight: 800; color: #FFFFFF;");
    name_row->addWidget(m_lbl_username);

    m_edit_username = new QLineEdit(card);
    m_edit_username->setVisible(false);
    m_edit_username->setMaximumWidth(200);
    name_row->addWidget(m_edit_username);

    auto* badge_mode = new QLabel("Local Profile", card);
    badge_mode->setProperty("class", "BadgeActive");
    name_row->addWidget(badge_mode);

    auto* badge_status = new QLabel("Ready", card);
    badge_status->setProperty("class", "BadgeSuccess");
    name_row->addWidget(badge_status);

    name_row->addStretch();
    user_info_layout->addLayout(name_row);

    m_lbl_bio = new QLabel("Wandering swordsman in the world of Swordigo.", card);
    m_lbl_bio->setStyleSheet("color: #8B949E; font-size: 13px;");
    user_info_layout->addWidget(m_lbl_bio);

    auto* btn_row = new QHBoxLayout();
    auto* btn_upload = new QPushButton("Upload Custom Avatar", card);
    btn_upload->setProperty("class", "SecondaryBtn");
    connect(btn_upload, &QPushButton::clicked, this, &ProfilePage::on_upload_avatar_clicked);
    btn_row->addWidget(btn_upload);

    m_btn_edit_user = new QPushButton("Edit Username", card);
    m_btn_edit_user->setProperty("class", "SecondaryBtn");
    connect(m_btn_edit_user, &QPushButton::clicked, this, &ProfilePage::on_edit_username_clicked);
    btn_row->addWidget(m_btn_edit_user);

    m_btn_save_user = new QPushButton("Save", card);
    m_btn_save_user->setStyleSheet("background-color: #2EA043; color: white; border: none; padding: 5px 14px; border-radius: 6px;");
    m_btn_save_user->setVisible(false);
    connect(m_btn_save_user, &QPushButton::clicked, this, &ProfilePage::on_save_username_clicked);
    btn_row->addWidget(m_btn_save_user);

    btn_row->addStretch();
    user_info_layout->addLayout(btn_row);

    card_layout->addLayout(user_info_layout);
    main_layout->addWidget(card);

    // ── 2. Avatar Presets Row ────────────────────────────────────────────────
    auto* preset_sec = new QLabel("AVATAR PRESETS", this);
    preset_sec->setStyleSheet("font-size: 11px; font-weight: bold; color: #8B949E; letter-spacing: 0.8px;");
    main_layout->addWidget(preset_sec);

    auto* presets_layout = new QHBoxLayout();
    presets_layout->setSpacing(14);
    struct PresetItem { int id; const char* name; const char* col; };
    PresetItem presets[] = {
        { 1, "Hiro", "#E94560" },
        { 2, "Knight", "#3B82F6" },
        { 3, "Shadow", "#8B5CF6" },
        { 4, "Mage", "#10B981" },
        { 5, "Golden", "#F59E0B" }
    };
    for (const auto& p : presets) {
        auto* p_btn = new QPushButton(p.name, this);
        p_btn->setProperty("class", "SecondaryBtn");
        p_btn->setIcon(QIcon(LauncherTheme::make_preset_avatar(p.name, 28, QColor(p.col))));
        int pid = p.id;
        connect(p_btn, &QPushButton::clicked, this, [this, pid]() { on_preset_clicked(pid); });
        presets_layout->addWidget(p_btn);
    }
    presets_layout->addStretch();
    main_layout->addLayout(presets_layout);

    // ── 3. Character Skin Selection ──────────────────────────────────────────
    auto* skin_sec = new QLabel("CHARACTER SKIN", this);
    skin_sec->setStyleSheet("font-size: 11px; font-weight: bold; color: #8B949E; letter-spacing: 0.8px;");
    main_layout->addWidget(skin_sec);

    auto* skin_row = new QHBoxLayout();
    skin_row->setSpacing(12);
    const char* skin_names[] = { "Default", "Knight", "Shadow", "Golden", "Mage" };
    for (int i = 0; i < 5; ++i) {
        auto* s_btn = new QPushButton(skin_names[i], this);
        s_btn->setProperty("class", "SecondaryBtn");
        s_btn->setCheckable(true);
        int skin_id = i;
        connect(s_btn, &QPushButton::clicked, this, [this, skin_id]() { on_skin_clicked(skin_id); });
        m_skin_btns.push_back(s_btn);
        skin_row->addWidget(s_btn);
    }
    skin_row->addStretch();
    main_layout->addLayout(skin_row);

    // ── 4. Live Game Save Progress ───────────────────────────────────────────
    auto* stats_sec_row = new QHBoxLayout();
    auto* stats_sec = new QLabel("GAME SAVE PROGRESS", this);
    stats_sec->setStyleSheet("font-size: 11px; font-weight: bold; color: #8B949E; letter-spacing: 0.8px;");
    stats_sec_row->addWidget(stats_sec);
    stats_sec_row->addStretch();

    auto* btn_refresh = new QPushButton("Refresh Stats", this);
    btn_refresh->setProperty("class", "SecondaryBtn");
    connect(btn_refresh, &QPushButton::clicked, this, &ProfilePage::on_refresh_stats_clicked);
    stats_sec_row->addWidget(btn_refresh);
    main_layout->addLayout(stats_sec_row);

    auto* stats_card = new QFrame(this);
    stats_card->setStyleSheet("background-color: #161B22; border: 1px solid #283141; border-radius: 12px;");
    auto* stats_layout = new QVBoxLayout(stats_card);
    stats_layout->setContentsMargins(20, 16, 20, 16);
    stats_layout->setSpacing(12);

    auto* save_header = new QHBoxLayout();
    m_lbl_save_name = new QLabel("Slot 1 (Hero)", stats_card);
    m_lbl_save_name->setStyleSheet("font-size: 14px; font-weight: 700; color: #F0F6FC;");
    save_header->addWidget(m_lbl_save_name);
    save_header->addStretch();
    stats_layout->addLayout(save_header);

    m_prog_completion = new QProgressBar(stats_card);
    m_prog_completion->setRange(0, 100);
    m_prog_completion->setValue(45);
    m_prog_completion->setFixedHeight(12);
    stats_layout->addWidget(m_prog_completion);

    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(24);
    grid->setVerticalSpacing(10);

    auto make_stat_item = [stats_card](const char* fa_glyph, const QColor& color, QLabel*& lbl) -> QHBoxLayout* {
        auto* l = new QHBoxLayout();
        l->setSpacing(8);
        auto* ic = new QLabel(stats_card);
        ic->setPixmap(LauncherTheme::fa_pixmap(fa_glyph, color, 14));
        l->addWidget(ic);
        lbl = new QLabel("—", stats_card);
        lbl->setStyleSheet("font-size: 13px; font-weight: 600; color: #E6EDF3;");
        l->addWidget(lbl);
        l->addStretch();
        return l;
    };

    grid->addLayout(make_stat_item(ICON_FA_GEM, QColor(88, 166, 255), m_lbl_coins), 0, 0);
    grid->addLayout(make_stat_item(ICON_FA_HEART, QColor(233, 69, 96), m_lbl_health), 0, 1);
    grid->addLayout(make_stat_item(ICON_FA_STAR, QColor(241, 196, 15), m_lbl_level), 1, 0);
    grid->addLayout(make_stat_item(ICON_FA_GAUGE_HIGH, QColor(61, 184, 79), m_lbl_xp), 1, 1);
    stats_layout->addLayout(grid);

    main_layout->addWidget(stats_card);

    // ── 5. Future Server Sync Architecture ───────────────────────────────────
    auto* server_card = new QFrame(this);
    server_card->setStyleSheet("background-color: #121720; border: 1px dashed #2D3748; border-radius: 10px;");
    auto* srv_layout = new QHBoxLayout(server_card);
    srv_layout->setContentsMargins(18, 14, 18, 14);

    auto* srv_info = new QVBoxLayout();
    auto* srv_t = new QLabel("Cloud Sync & Server Account (Ready for Server)", server_card);
    srv_t->setStyleSheet("font-size: 13px; font-weight: bold; color: #8B949E;");
    auto* srv_d = new QLabel("Currently operating in high-performance local mode. Profile data & achievements persist on your disk.", server_card);
    srv_d->setStyleSheet("font-size: 12px; color: #6E7681;");
    srv_info->addWidget(srv_t);
    srv_info->addWidget(srv_d);
    srv_layout->addLayout(srv_info);
    srv_layout->addStretch();

    auto* btn_srv = new QPushButton("Server Offline", server_card);
    btn_srv->setEnabled(false);
    btn_srv->setProperty("class", "SecondaryBtn");
    srv_layout->addWidget(btn_srv);

    main_layout->addWidget(server_card);
    main_layout->addStretch();
}

void ProfilePage::update_ui() {
    const auto& prof = ProfileManager::instance().profile();
    m_lbl_username->setText(prof.username);
    m_lbl_bio->setText(prof.bio);
    m_lbl_avatar->setPixmap(ProfileManager::instance().get_avatar_pixmap(96));

    // Update skin buttons
    for (size_t i = 0; i < m_skin_btns.size(); ++i) {
        bool selected = (static_cast<int>(i) == prof.favorite_skin);
        m_skin_btns[i]->setChecked(selected);
        if (selected) {
            m_skin_btns[i]->setStyleSheet("background-color: rgba(233, 69, 96, 0.2); border: 2px solid #E94560; color: white;");
        } else {
            m_skin_btns[i]->setStyleSheet("");
        }
    }

    // Update save stats
    const auto& st = ProfileManager::instance().stats();
    if (st.has_save) {
        m_lbl_save_name->setText(QString("Active Save: %1").arg(st.save_name.isEmpty() ? "Slot 1" : st.save_name));
        m_prog_completion->setValue(static_cast<int>(st.percent_completed * 100.0f));
        m_lbl_coins->setText(QString("Coins: %1").arg(st.coins));
        m_lbl_health->setText(QString("Health: %1").arg(st.health));
        m_lbl_level->setText(QString("Level: %1").arg(st.level));
        m_lbl_xp->setText(QString("XP: %1").arg(st.xp));
    } else {
        m_lbl_save_name->setText("No active save found. Boot the game to start your journey!");
        m_prog_completion->setValue(0);
        m_lbl_coins->setText("Coins: 0");
        m_lbl_health->setText("Health: 0");
        m_lbl_level->setText("Level: 1");
        m_lbl_xp->setText("XP: 0");
    }
}

void ProfilePage::on_upload_avatar_clicked() {
    QString file = QFileDialog::getOpenFileName(
        this, "Select Avatar Image", "",
        "Images (*.png *.jpg *.jpeg *.webp *.bmp)"
    );
    if (!file.isEmpty()) {
        if (ProfileManager::instance().import_custom_avatar(file)) {
            update_ui();
        } else {
            QMessageBox::warning(this, "Avatar Upload Failed", "Could not load or process the selected image.");
        }
    }
}

void ProfilePage::on_edit_username_clicked() {
    m_lbl_username->setVisible(false);
    m_btn_edit_user->setVisible(false);
    m_edit_username->setText(m_lbl_username->text());
    m_edit_username->setVisible(true);
    m_btn_save_user->setVisible(true);
    m_edit_username->setFocus();
}

void ProfilePage::on_save_username_clicked() {
    QString name = m_edit_username->text().trimmed();
    if (!name.isEmpty()) {
        ProfileManager::instance().set_username(name);
    }
    m_edit_username->setVisible(false);
    m_btn_save_user->setVisible(false);
    m_lbl_username->setVisible(true);
    m_btn_edit_user->setVisible(true);
}

void ProfilePage::on_preset_clicked(int id) {
    ProfileManager::instance().set_avatar_preset(id);
    update_ui();
}

void ProfilePage::on_skin_clicked(int id) {
    ProfileManager::instance().set_favorite_skin(id);
    update_ui();
}

void ProfilePage::on_refresh_stats_clicked() {
    ProfileManager::instance().refresh_game_stats();
}

} // namespace swordfare::launcher
