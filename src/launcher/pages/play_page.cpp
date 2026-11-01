// ============================================================================
// play_page.cpp — Home page implementation (1:1 with ImGui Home)
// ============================================================================

#include "launcher/pages/play_page.h"
#include "launcher/launcher_theme.h"
#include "launcher/profile_manager.h"
#include "platform/data_path.h"
#include "platform/launcher_config.h"
#include "platform/mod_manager.h"

#include <filesystem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QPainter>
#include <QDir>
#include <algorithm>

namespace swordfare::launcher {

PlayPage::PlayPage(BinarySelector& selector, QWidget* parent)
    : QWidget(parent), m_selector(selector) {
    setup_ui();
    refresh_instances();
    update_stats();

    connect(&ProfileManager::instance(), &ProfileManager::profileChanged, this, [this]() {
        if (m_lbl_welcome_name) {
            m_lbl_welcome_name->setText(ProfileManager::instance().profile().username);
        }
    });
}

void PlayPage::setup_ui() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(28, 20, 28, 20);
    main_layout->setSpacing(14);

    // ── 1. Hero Banner with Background Watermark ─────────────────────────────
    class HeroBanner : public QFrame {
    public:
        using QFrame::QFrame;
    protected:
        void paintEvent(QPaintEvent* event) override {
            QFrame::paintEvent(event);
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);

            // Subtle gradient
            QLinearGradient grad(0, 0, width(), height());
            grad.setColorAt(0.0, QColor(14, 20, 35));
            grad.setColorAt(0.5, QColor(45, 18, 30));
            grad.setColorAt(1.0, QColor(10, 14, 25));
            painter.setBrush(grad);
            painter.setPen(QPen(QColor(40, 50, 68), 1));
            painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 12, 12);

            // Watermark logo background
            QPixmap bg = LauncherTheme::background_pixmap();
            if (!bg.isNull()) {
                painter.setOpacity(0.12);
                painter.drawPixmap(width() - bg.width() * 0.45 - 20, (height() - bg.height() * 0.45) / 2,
                                   bg.scaled(bg.width() * 0.45, bg.height() * 0.45, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
        }
    };

    auto* hero_frame = new HeroBanner(this);
    hero_frame->setFixedHeight(115);
    auto* hero_layout = new QVBoxLayout(hero_frame);
    hero_layout->setContentsMargins(24, 16, 24, 16);
    hero_layout->setSpacing(4);

    auto* lbl_welcome = new QLabel("Welcome back,", hero_frame);
    lbl_welcome->setStyleSheet("font-size: 14px; font-weight: 600; color: #E6EDF3; background: transparent;");
    hero_layout->addWidget(lbl_welcome);

    m_lbl_welcome_name = new QLabel(ProfileManager::instance().profile().username, hero_frame);
    m_lbl_welcome_name->setStyleSheet("font-size: 24px; font-weight: 900; color: #E94560; letter-spacing: 0.5px; background: transparent;");
    hero_layout->addWidget(m_lbl_welcome_name);

    auto* lbl_sub = new QLabel("Ready to play Swordigo Desktop", hero_frame);
    lbl_sub->setStyleSheet("font-size: 12px; color: #8B949E; background: transparent;");
    hero_layout->addWidget(lbl_sub);

    main_layout->addWidget(hero_frame);

    // ── 2. News / Announcement Ticker ────────────────────────────────────────
    auto* ticker_frame = new QFrame(this);
    ticker_frame->setFixedHeight(36);
    ticker_frame->setStyleSheet(
        "background-color: #0E1420; border: 1px solid #1C2433; border-radius: 8px;"
    );
    auto* ticker_layout = new QHBoxLayout(ticker_frame);
    ticker_layout->setContentsMargins(12, 0, 14, 0);
    ticker_layout->setSpacing(12);

    auto* badge_news = new QLabel("UPDATES", ticker_frame);
    badge_news->setStyleSheet(
        "background-color: #E94560; color: white; font-weight: 800; font-size: 10px; border-radius: 4px; padding: 2px 6px;"
    );
    ticker_layout->addWidget(badge_news);

    auto* ticker_text = new QLabel(
        "ARM64 + Dynarmic JIT active  •  6 mods available in Mod Browser  •  Save Editor & Dev Tools ready  •  Join the community!",
        ticker_frame
    );
    ticker_text->setStyleSheet("font-size: 12px; color: #B4C3D7; font-weight: 500;");
    ticker_layout->addWidget(ticker_text, 1);
    main_layout->addWidget(ticker_frame);

    // ── 3. Quick Stats Row (3 Cards) ─────────────────────────────────────────
    auto* stats_row = new QHBoxLayout();
    stats_row->setSpacing(12);

    auto create_stat_card = [this](const char* icon, const QColor& icon_col, const QString& label, QLabel*& val_lbl) -> QFrame* {
        auto* card = new QFrame(this);
        card->setFixedHeight(58);
        card->setStyleSheet(
            "background-color: #121824; border: 1px solid #1C2433; border-radius: 10px;"
        );
        auto* l = new QHBoxLayout(card);
        l->setContentsMargins(14, 8, 14, 8);
        l->setSpacing(12);

        auto* ic = new QLabel(card);
        ic->setPixmap(LauncherTheme::fa_pixmap(icon, icon_col, 20));
        l->addWidget(ic);

        auto* text_col = new QVBoxLayout();
        text_col->setSpacing(0);
        val_lbl = new QLabel("—", card);
        val_lbl->setStyleSheet("font-size: 16px; font-weight: 800; color: #F0F6FC;");
        auto* desc = new QLabel(label, card);
        desc->setStyleSheet("font-size: 11px; color: #8B949E; font-weight: 600;");
        text_col->addWidget(val_lbl);
        text_col->addWidget(desc);
        l->addLayout(text_col, 1);

        return card;
    };

    stats_row->addWidget(create_stat_card(ICON_FA_GAMEPAD, QColor(88, 166, 255), "Instances", m_lbl_stat_instances));
    stats_row->addWidget(create_stat_card(ICON_FA_PUZZLE_PIECE, QColor(61, 184, 79), "Mods Active", m_lbl_stat_mods));
    stats_row->addWidget(create_stat_card(ICON_FA_CLOCK, QColor(209, 154, 33), "Status", m_lbl_stat_status));
    main_layout->addLayout(stats_row);

    // ── 4. Featured Instance Card ────────────────────────────────────────────
    auto* feat_header = new QHBoxLayout();
    auto* feat_sec_icon = new QLabel(this);
    feat_sec_icon->setPixmap(LauncherTheme::fa_pixmap(ICON_FA_ROCKET, QColor(233, 69, 96), 14));
    feat_header->addWidget(feat_sec_icon);

    auto* feat_sec_lbl = new QLabel("READY TO LAUNCH", this);
    feat_sec_lbl->setStyleSheet("font-size: 11px; font-weight: 800; color: #8B949E; letter-spacing: 0.8px;");
    feat_header->addWidget(feat_sec_lbl);
    feat_header->addStretch();

    auto* btn_browse_all = new QPushButton("Manage in Library", this);
    btn_browse_all->setProperty("class", "SecondaryBtn");
    connect(btn_browse_all, &QPushButton::clicked, this, &PlayPage::navigateToLibrary);
    feat_header->addWidget(btn_browse_all);
    main_layout->addLayout(feat_header);

    auto* feat_card = new QFrame(this);
    feat_card->setObjectName("FeatCard");
    feat_card->setStyleSheet(
        "#FeatCard {"
        "  background-color: #121824;"
        "  border: 1px solid #232D3F;"
        "  border-radius: 12px;"
        "}"
    );
    auto* feat_layout = new QHBoxLayout(feat_card);
    feat_layout->setContentsMargins(20, 16, 20, 16);
    feat_layout->setSpacing(16);

    m_lbl_feat_icon = new QLabel(feat_card);
    m_lbl_feat_icon->setFixedSize(54, 54);
    m_lbl_feat_icon->setPixmap(LauncherTheme::game_icon(54));
    feat_layout->addWidget(m_lbl_feat_icon);

    // Left block: Static Title "Swordigo"
    auto* feat_info = new QVBoxLayout();
    feat_info->setSpacing(4);

    m_lbl_feat_title = new QLabel("Swordigo", feat_card);
    m_lbl_feat_title->setStyleSheet("font-size: 18px; font-weight: 800; color: #FFFFFF; letter-spacing: 0.5px;");
    feat_info->addWidget(m_lbl_feat_title);

    m_lbl_feat_subtitle = new QLabel("Vanilla Base Engine • Native ARM64 Translation", feat_card);
    m_lbl_feat_subtitle->setStyleSheet("font-size: 11px; color: #8B949E;");
    feat_info->addWidget(m_lbl_feat_subtitle);

    feat_layout->addLayout(feat_info, 1);

    // Center block: Minecraft-style Base Engine Selector (ONLY Base Engines: 1.4.12 & 1.4.13)
    auto* base_engine_box = new QVBoxLayout();
    base_engine_box->setSpacing(4);
    auto* lbl_base_tag = new QLabel("VERSION SELECTOR", feat_card);
    lbl_base_tag->setStyleSheet("font-size: 10px; font-weight: 800; color: #8B949E; letter-spacing: 0.5px;");
    base_engine_box->addWidget(lbl_base_tag);

    m_combo_base_engine = new QComboBox(feat_card);
    m_combo_base_engine->setMinimumWidth(210);
    m_combo_base_engine->setStyleSheet(
        "QComboBox {"
        "  background-color: #182030; color: #58A6FF; font-weight: bold; font-size: 13px; border: 1px solid #2B3D66; border-radius: 6px; padding: 6px 12px;"
        "}"
        "QComboBox:hover { border-color: #58A6FF; }"
    );
    connect(m_combo_base_engine, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (m_combo_base_engine && m_combo_base_engine->currentIndex() >= 0) {
            LauncherConfig lcfg = launcher_config_load();
            lcfg.selected_base_version = m_combo_base_engine->currentData().toString().toStdString();
            launcher_config_save(lcfg);
        }
        update_featured_card();
    });
    base_engine_box->addWidget(m_combo_base_engine);

    auto* lbl_engine_hint = new QLabel("Select base version to play", feat_card);
    lbl_engine_hint->setStyleSheet("font-size: 11px; color: #6E7681;");
    base_engine_box->addWidget(lbl_engine_hint);

    feat_layout->addLayout(base_engine_box);

    // Right block: Prominent PLAY Button
    m_btn_play = new QPushButton("  PLAY", feat_card);
    m_btn_play->setIcon(LauncherTheme::fa_icon(ICON_FA_PLAY, Qt::white, 16));
    m_btn_play->setObjectName("LaunchBtn");
    m_btn_play->setFixedSize(145, 48);
    m_btn_play->setCursor(Qt::PointingHandCursor);
    connect(m_btn_play, &QPushButton::clicked, this, &PlayPage::on_launch_clicked);
    feat_layout->addWidget(m_btn_play);

    main_layout->addWidget(feat_card);

    // ── 5. Launch Options Row (1:1 with ImGui) ───────────────────────────────
    auto* opts_header = new QHBoxLayout();
    auto* opts_sec_icon = new QLabel(this);
    opts_sec_icon->setPixmap(LauncherTheme::fa_pixmap(ICON_FA_SLIDERS, QColor(233, 69, 96), 14));
    opts_header->addWidget(opts_sec_icon);

    auto* opts_sec_lbl = new QLabel("LAUNCH OPTIONS", this);
    opts_sec_lbl->setStyleSheet("font-size: 11px; font-weight: 800; color: #8B949E; letter-spacing: 0.8px;");
    opts_header->addWidget(opts_sec_lbl);
    opts_header->addStretch();
    main_layout->addLayout(opts_header);

    auto* opts_card = new QFrame(this);
    opts_card->setStyleSheet("background-color: #121824; border: 1px solid #1C2433; border-radius: 10px;");
    auto* opts_layout = new QHBoxLayout(opts_card);
    opts_layout->setContentsMargins(16, 12, 16, 12);
    opts_layout->setSpacing(20);

    // Graphics API
    auto* api_box = new QVBoxLayout();
    auto* api_lbl = new QLabel("Graphics API", opts_card);
    api_lbl->setStyleSheet("font-size: 11px; font-weight: bold; color: #8B949E;");
    m_combo_api = new QComboBox(opts_card);
    m_combo_api->addItem("OpenGL 3.3", static_cast<int>(GraphicsAPI::OPENGL));
    m_combo_api->addItem("Vulkan (Experimental)", static_cast<int>(GraphicsAPI::VULKAN));
    api_box->addWidget(api_lbl);
    api_box->addWidget(m_combo_api);
    opts_layout->addLayout(api_box);

    // CPU Engine
    auto* cpu_box = new QVBoxLayout();
    auto* cpu_lbl = new QLabel("CPU Engine", opts_card);
    cpu_lbl->setStyleSheet("font-size: 11px; font-weight: bold; color: #8B949E;");
    m_combo_jit = new QComboBox(opts_card);
    m_combo_jit->addItem("Dynarmic JIT (Fastest)", true);
    m_combo_jit->addItem("Unicorn Engine", false);
    cpu_box->addWidget(cpu_lbl);
    cpu_box->addWidget(m_combo_jit);
    opts_layout->addLayout(cpu_box);

    // Toggles
    auto* toggles_box = new QVBoxLayout();
    m_chk_sre = new QCheckBox("Enable SRE Hooks", opts_card);
    m_chk_sre->setChecked(true);
    m_chk_redstell = new QCheckBox("Redstell Optimizations", opts_card);
    m_chk_redstell->setChecked(false);
    toggles_box->addWidget(m_chk_sre);
    toggles_box->addWidget(m_chk_redstell);
    opts_layout->addLayout(toggles_box);

    opts_layout->addStretch();
    main_layout->addWidget(opts_card);

    // ── 6. Recent Activity Section ───────────────────────────────────────────
    auto* rec_header = new QHBoxLayout();
    auto* rec_sec_icon = new QLabel(this);
    rec_sec_icon->setPixmap(LauncherTheme::fa_pixmap(ICON_FA_NEWSPAPER, QColor(233, 69, 96), 14));
    rec_header->addWidget(rec_sec_icon);

    auto* rec_sec_lbl = new QLabel("RECENT ACTIVITY", this);
    rec_sec_lbl->setStyleSheet("font-size: 11px; font-weight: 800; color: #8B949E; letter-spacing: 0.8px;");
    rec_header->addWidget(rec_sec_lbl);
    rec_header->addStretch();
    main_layout->addLayout(rec_header);

    auto* rec_card = new QFrame(this);
    rec_card->setStyleSheet("background-color: #0E1420; border: 1px solid #1C2433; border-radius: 8px;");
    auto* rec_layout = new QHBoxLayout(rec_card);
    rec_layout->setContentsMargins(14, 10, 14, 10);
    rec_layout->setSpacing(10);

    auto* rec_icon = new QLabel(rec_card);
    rec_icon->setPixmap(LauncherTheme::fa_pixmap(ICON_FA_CLOCK_ROTATE_LEFT, QColor(110, 118, 129), 14));
    rec_layout->addWidget(rec_icon);

    auto* rec_lbl = new QLabel("No recent sessions. Played sessions will appear here.", rec_card);
    rec_lbl->setStyleSheet("font-size: 12px; color: #6E7681;");
    rec_layout->addWidget(rec_lbl, 1);

    main_layout->addWidget(rec_card);
    main_layout->addStretch();
}

void PlayPage::refresh_instances() {
    auto bases = m_selector.get_base_engines();
    if (bases.empty()) {
        m_btn_play->setEnabled(false);
        m_lbl_feat_subtitle->setText("No base engines found (v1.4.12 / v1.4.13). Check engine/ folder.");
        return;
    }

    m_btn_play->setEnabled(true);

    // Populate Minecraft-Style Base Engine Selector (ONLY Base Engines: v1.4.12 & v1.4.13)
    m_combo_base_engine->blockSignals(true);
    m_combo_base_engine->clear();

    m_combo_base_engine->addItem("Swordigo v1.4.12 (SRE12)", "1.4.12");
    m_combo_base_engine->addItem("Swordigo v1.4.13 (SRE13)", "1.4.13");

    LauncherConfig lcfg = launcher_config_load();
    int default_idx = 1; // Default to 1.4.13
    if (lcfg.selected_base_version == "1.4.12") {
        default_idx = 0;
    } else if (lcfg.selected_base_version == "1.4.13") {
        default_idx = 1;
    } else {
        std::string def_path = m_selector.get_default();
        if (def_path.find("1.4.12") != std::string::npos && def_path.find("1.4.13") == std::string::npos) {
            default_idx = 0;
        }
    }

    m_combo_base_engine->setCurrentIndex(default_idx);
    m_combo_base_engine->blockSignals(false);

    update_featured_card();
}

void PlayPage::update_featured_card() {
    LauncherConfig lcfg = launcher_config_load();
    std::string sel_ver = lcfg.selected_base_version.empty() ? "1.4.13" : lcfg.selected_base_version;
    if (m_combo_base_engine && m_combo_base_engine->currentIndex() >= 0) {
        sel_ver = m_combo_base_engine->currentData().toString().toStdString();
    }

    m_lbl_feat_title->setText("Swordigo");

    QString sub = QString("ARM64 Engine • Vanilla Base v%1 (%2)")
                      .arg(QString::fromStdString(sel_ver))
                      .arg(sel_ver == "1.4.13" ? "SRE13 Active" : "SRE12 Active");
    m_lbl_feat_subtitle->setText(sub);
}

void PlayPage::update_stats() {
    const auto& binaries = m_selector.get_binaries();
    m_lbl_stat_instances->setText(QString::number(binaries.size()));

    // Check mods folder
    std::string mods_dir = get_user_data_dir() + "/mods";
    QDir dir(QString::fromStdString(mods_dir));
    int mod_count = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot).size();
    m_lbl_stat_mods->setText(QString::number(std::max(0, mod_count)));

    m_lbl_stat_status->setText("Ready");
}

LaunchConfig PlayPage::get_launch_config() const {
    LaunchConfig cfg;

    LauncherConfig lcfg = launcher_config_load();
    std::string sel_ver = lcfg.selected_base_version.empty() ? "1.4.13" : lcfg.selected_base_version;
    if (m_combo_base_engine && m_combo_base_engine->currentIndex() >= 0) {
        sel_ver = m_combo_base_engine->currentData().toString().toStdString();
    }

    BinaryInfo base_info;
    base_info.version = sel_ver;
    base_info.preferred_base = sel_ver;
    base_info.is_base = true;

    cfg.selected_binary = m_selector.resolve_launch_binary(base_info, sel_ver);
    cfg.assets_dir = (sel_ver == "1.4.13") ? "assets13" : "assets";
    cfg.game_type = "Swordigo";
    cfg.selected_base_version = sel_ver;
    cfg.instance_name = "Swordigo v" + sel_ver;

    cfg.graphics_api = static_cast<GraphicsAPI>(m_combo_api->currentData().toInt());
    cfg.use_dynarmic = m_combo_jit->currentData().toBool();
    cfg.use_sre = m_chk_sre->isChecked();
    cfg.advanced_redstell_opts = m_chk_redstell->isChecked();
    cfg.should_launch = true;

    // Resolve active mod (prioritizing LauncherConfig load order, then first enabled mod on disk)
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

    return cfg;
}

void PlayPage::on_launch_clicked() {
    emit launchRequested(get_launch_config());
}

} // namespace swordfare::launcher
