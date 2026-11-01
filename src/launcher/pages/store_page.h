#pragma once
// ============================================================================
// store_page.h — Community Mod Catalog & Download Browser
// ============================================================================

#include <QWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QProgressBar>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "platform/mod_manager.h"

namespace swordfare::launcher {

class StorePage : public QWidget {
    Q_OBJECT

public:
    explicit StorePage(QWidget* parent = nullptr);

signals:
    void modInstalled();

public slots:
    void refresh_catalog();

private slots:
    void on_search_changed(const QString& query);
    void on_category_changed(int index);
    void on_mod_selected(int row);
    void on_install_clicked();
    void on_catalog_downloaded(QNetworkReply* reply);
    void on_icon_downloaded(const QString& mod_id, const QByteArray& data);

private:
    void setup_ui();
    void filter_list();
    void load_catalog_data(const std::string& json, bool is_remote);
    void download_remote_icon(const QString& mod_id, const QString& url);
    QPixmap get_cached_or_default_icon(const modman::StoreMod& m, int size);

    QNetworkAccessManager*      m_net_mgr = nullptr;
    QLineEdit*                  m_edit_search = nullptr;
    QComboBox*                  m_combo_cat = nullptr;
    QPushButton*                m_btn_refresh = nullptr;
    QListWidget*                m_list = nullptr;

    // Detail Panel Widgets
    QLabel*                     m_lbl_detail_icon = nullptr;
    QLabel*                     m_lbl_title = nullptr;
    QLabel*                     m_lbl_author_cat = nullptr;
    QLabel*                     m_lbl_status_badge = nullptr;
    QTextBrowser*               m_txt_desc = nullptr;
    QProgressBar*               m_progress_bar = nullptr;
    QPushButton*                m_btn_install = nullptr;

    std::vector<modman::StoreMod> m_all_mods;
    std::vector<int>            m_filtered_indices;
    std::vector<bool>           m_installed_mask;
    std::set<QString>           m_pending_icon_downloads;
};

} // namespace swordfare::launcher
