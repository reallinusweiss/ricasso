#pragma once
// ============================================================================
// profile_page.h — Full user profile, avatar customization, and save stats
// ============================================================================

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QProgressBar>
#include "launcher/profile_manager.h"

namespace swordfare::launcher {

class ProfilePage : public QWidget {
    Q_OBJECT

public:
    explicit ProfilePage(QWidget* parent = nullptr);

public slots:
    void update_ui();

private slots:
    void on_upload_avatar_clicked();
    void on_edit_username_clicked();
    void on_save_username_clicked();
    void on_preset_clicked(int id);
    void on_skin_clicked(int id);
    void on_refresh_stats_clicked();

private:
    void setup_ui();

    QLabel*       m_lbl_avatar = nullptr;
    QLabel*       m_lbl_username = nullptr;
    QLabel*       m_lbl_bio = nullptr;
    QLineEdit*    m_edit_username = nullptr;
    QPushButton*  m_btn_edit_user = nullptr;
    QPushButton*  m_btn_save_user = nullptr;

    // Stats
    QLabel*       m_lbl_save_name = nullptr;
    QProgressBar* m_prog_completion = nullptr;
    QLabel*       m_lbl_coins = nullptr;
    QLabel*       m_lbl_health = nullptr;
    QLabel*       m_lbl_level = nullptr;
    QLabel*       m_lbl_xp = nullptr;

    std::vector<QPushButton*> m_skin_btns;
};

} // namespace swordfare::launcher
