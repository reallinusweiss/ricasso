// ============================================================================
// settings_page.cpp — Settings page implementation
// ============================================================================

#include "launcher/pages/settings_page.h"
#include "launcher/launcher_theme.h"
#include "platform/launcher_config.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>

namespace swordfare::launcher {

SettingsPage::SettingsPage(QWidget* parent) : QWidget(parent) {
    setup_ui();
    load_settings();
}

void SettingsPage::setup_ui() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(28, 24, 28, 24);
    main_layout->setSpacing(20);

    // Title
    auto* title = new QLabel("ENGINE & LAUNCHER SETTINGS", this);
    title->setStyleSheet("font-size: 14px; font-weight: 800; color: #FFFFFF; letter-spacing: 0.8px;");
    main_layout->addWidget(title);

    // ── 1. Audio Settings Card ───────────────────────────────────────────────
    auto* audio_card = new QFrame(this);
    audio_card->setStyleSheet("background-color: #161B22; border: 1px solid #283141; border-radius: 12px;");
    auto* a_layout = new QVBoxLayout(audio_card);
    a_layout->setContentsMargins(20, 18, 20, 18);
    a_layout->setSpacing(14);

    auto* a_title = new QLabel("AUDIO VOLUMES", audio_card);
    a_title->setStyleSheet("font-size: 11px; font-weight: bold; color: #8B949E;");
    a_layout->addWidget(a_title);

    auto* a_grid = new QGridLayout();
    a_grid->setHorizontalSpacing(16);
    a_grid->setVerticalSpacing(12);

    // Master
    a_grid->addWidget(new QLabel("Master Volume:", audio_card), 0, 0);
    m_slider_master = new QSlider(Qt::Horizontal, audio_card);
    m_slider_master->setRange(0, 100);
    a_grid->addWidget(m_slider_master, 0, 1);
    m_lbl_master_val = new QLabel("100%", audio_card);
    m_lbl_master_val->setFixedWidth(40);
    a_grid->addWidget(m_lbl_master_val, 0, 2);

    // Music
    a_grid->addWidget(new QLabel("Music Volume:", audio_card), 1, 0);
    m_slider_music = new QSlider(Qt::Horizontal, audio_card);
    m_slider_music->setRange(0, 100);
    a_grid->addWidget(m_slider_music, 1, 1);
    m_lbl_music_val = new QLabel("85%", audio_card);
    m_lbl_music_val->setFixedWidth(40);
    a_grid->addWidget(m_lbl_music_val, 1, 2);

    // SFX
    a_grid->addWidget(new QLabel("Effects (SFX):", audio_card), 2, 0);
    m_slider_sfx = new QSlider(Qt::Horizontal, audio_card);
    m_slider_sfx->setRange(0, 100);
    a_grid->addWidget(m_slider_sfx, 2, 1);
    m_lbl_sfx_val = new QLabel("100%", audio_card);
    m_lbl_sfx_val->setFixedWidth(40);
    a_grid->addWidget(m_lbl_sfx_val, 2, 2);

    a_layout->addLayout(a_grid);
    main_layout->addWidget(audio_card);

    // Slider value changes
    connect(m_slider_master, &QSlider::valueChanged, this, [this](int v) {
        m_lbl_master_val->setText(QString("%1%").arg(v));
        save_settings();
    });
    connect(m_slider_music, &QSlider::valueChanged, this, [this](int v) {
        m_lbl_music_val->setText(QString("%1%").arg(v));
        save_settings();
    });
    connect(m_slider_sfx, &QSlider::valueChanged, this, [this](int v) {
        m_lbl_sfx_val->setText(QString("%1%").arg(v));
        save_settings();
    });

    // ── 2. Graphics & Engine Settings Card ───────────────────────────────────
    auto* gfx_card = new QFrame(this);
    gfx_card->setStyleSheet("background-color: #161B22; border: 1px solid #283141; border-radius: 12px;");
    auto* g_layout = new QVBoxLayout(gfx_card);
    g_layout->setContentsMargins(20, 18, 20, 18);
    g_layout->setSpacing(12);

    auto* g_title = new QLabel("GRAPHICS & EMULATION ENGINE", gfx_card);
    g_title->setStyleSheet("font-size: 11px; font-weight: bold; color: #8B949E;");
    g_layout->addWidget(g_title);

    m_chk_postfx = new QCheckBox("Enable Post-Processing Shaders & Glow FX", gfx_card);
    connect(m_chk_postfx, &QCheckBox::toggled, this, &SettingsPage::save_settings);
    g_layout->addWidget(m_chk_postfx);

    m_chk_slideshow = new QCheckBox("Enable Boot Loading Screen Background Slideshow", gfx_card);
    connect(m_chk_slideshow, &QCheckBox::toggled, this, &SettingsPage::save_settings);
    g_layout->addWidget(m_chk_slideshow);

    m_chk_pvr_sw = new QCheckBox("Force Software PVR Texture Decompression", gfx_card);
    connect(m_chk_pvr_sw, &QCheckBox::toggled, this, &SettingsPage::save_settings);
    g_layout->addWidget(m_chk_pvr_sw);

    main_layout->addWidget(gfx_card);
    main_layout->addStretch();
}

void SettingsPage::load_settings() {
    LauncherConfig cfg = launcher_config_load();

    m_slider_master->setValue(static_cast<int>(cfg.master_volume * 100.0f));
    m_slider_music->setValue(static_cast<int>(cfg.music_volume * 100.0f));
    m_slider_sfx->setValue(static_cast<int>(cfg.sfx_volume * 100.0f));

    m_chk_postfx->setChecked(cfg.postfx_enabled);
    m_chk_slideshow->setChecked(cfg.loading_slideshow);
    m_chk_pvr_sw->setChecked(cfg.pvr_software_decode);
}

void SettingsPage::save_settings() {
    LauncherConfig cfg = launcher_config_load();

    cfg.master_volume = m_slider_master->value() / 100.0f;
    cfg.music_volume = m_slider_music->value() / 100.0f;
    cfg.sfx_volume = m_slider_sfx->value() / 100.0f;

    cfg.postfx_enabled = m_chk_postfx->isChecked();
    cfg.loading_slideshow = m_chk_slideshow->isChecked();
    cfg.pvr_software_decode = m_chk_pvr_sw->isChecked();

    launcher_config_save(cfg);
}

} // namespace swordfare::launcher
