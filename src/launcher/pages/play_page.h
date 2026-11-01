#pragma once
// ============================================================================
// play_page.h — Home page matching ImGui launcher (hero, ticker, featured card, options)
// ============================================================================

#include <QWidget>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include "platform/binary_selector.h"
#include "platform/launcher_ui.h"

namespace swordfare::launcher {

class PlayPage : public QWidget {
    Q_OBJECT

public:
    explicit PlayPage(BinarySelector& selector, QWidget* parent = nullptr);
    LaunchConfig get_launch_config() const;

signals:
    void launchRequested(const LaunchConfig& config);
    void navigateToLibrary();

public slots:
    void refresh_instances();
    void update_stats();

private slots:
    void on_launch_clicked();

private:
    void setup_ui();
    void update_featured_card();

    BinarySelector& m_selector;
    int             m_featured_index = 0;

    // Hero Banner widgets
    QLabel*         m_lbl_welcome_name = nullptr;

    // Stats widgets
    QLabel*         m_lbl_stat_instances = nullptr;
    QLabel*         m_lbl_stat_mods = nullptr;
    QLabel*         m_lbl_stat_status = nullptr;

    // Active Launch Card widgets (Minecraft Launcher style)
    QLabel*         m_lbl_feat_icon = nullptr;
    QLabel*         m_lbl_feat_title = nullptr;
    QLabel*         m_lbl_feat_subtitle = nullptr;
    QLabel*         m_lbl_feat_arch = nullptr;
    QLabel*         m_lbl_feat_default = nullptr;
    QComboBox*      m_combo_base_engine = nullptr; // Minecraft-style base engine dropdown (ONLY v1.4.12 & v1.4.13)
    QPushButton*    m_btn_play = nullptr;

    // Launch Options
    QComboBox*      m_combo_api = nullptr;
    QComboBox*      m_combo_jit = nullptr;
    QCheckBox*      m_chk_sre = nullptr;
    QCheckBox*      m_chk_redstell = nullptr;
};

} // namespace swordfare::launcher
