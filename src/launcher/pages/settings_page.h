#pragma once
// ============================================================================
// settings_page.h — Persistent audio, graphics, and engine settings
// ============================================================================

#include <QWidget>
#include <QSlider>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>

namespace swordfare::launcher {

class SettingsPage : public QWidget {
    Q_OBJECT

public:
    explicit SettingsPage(QWidget* parent = nullptr);

public slots:
    void load_settings();
    void save_settings();

private:
    void setup_ui();

    // Audio Sliders
    QSlider*   m_slider_master = nullptr;
    QSlider*   m_slider_music = nullptr;
    QSlider*   m_slider_sfx = nullptr;
    QLabel*    m_lbl_master_val = nullptr;
    QLabel*    m_lbl_music_val = nullptr;
    QLabel*    m_lbl_sfx_val = nullptr;

    // Toggles
    QCheckBox* m_chk_postfx = nullptr;
    QCheckBox* m_chk_slideshow = nullptr;
    QCheckBox* m_chk_pvr_sw = nullptr;
    QCheckBox* m_chk_dynarmic = nullptr;
};

} // namespace swordfare::launcher
