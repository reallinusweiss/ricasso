#pragma once
// ============================================================================
// mods_page.h — Full mod manager with zip installer and VFS priority
// ============================================================================

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QTextBrowser>
#include "platform/mod_manager.h"

namespace swordfare::launcher {

class ModsPage : public QWidget {
    Q_OBJECT

public:
    explicit ModsPage(QWidget* parent = nullptr);

public slots:
    void refresh_mods();

private slots:
    void on_install_zip_clicked();
    void on_open_folder_clicked();
    void on_toggle_mod(int row);
    void on_delete_mod_clicked();
    void on_move_up_clicked();
    void on_move_down_clicked();
    void on_selection_changed(int row);

private:
    void setup_ui();
    QPixmap get_mod_icon(const modman::ModMeta& m, int size);

    QListWidget*          m_list = nullptr;
    
    // Detail Card Widgets
    QLabel*               m_lbl_detail_icon = nullptr;
    QLabel*               m_lbl_detail_title = nullptr;
    QLabel*               m_lbl_detail_meta = nullptr;
    QLabel*               m_lbl_detail_status = nullptr;
    QTextBrowser*         m_txt_desc = nullptr;

    QPushButton*          m_btn_toggle = nullptr;
    QPushButton*          m_btn_delete = nullptr;
    QPushButton*          m_btn_up = nullptr;
    QPushButton*          m_btn_down = nullptr;

    std::vector<modman::ModMeta> m_mods;
};

} // namespace swordfare::launcher
