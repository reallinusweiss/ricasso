#pragma once
// ============================================================================
// library_page.h — Two-column Game Library page matching ImGui launcher
// ============================================================================

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QFrame>
#include <QStyledItemDelegate>
#include "platform/binary_selector.h"
#include "platform/launcher_ui.h"

namespace swordfare::launcher {

class InstanceItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit InstanceItemDelegate(QObject* parent = nullptr);
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

class LibraryPage : public QWidget {
    Q_OBJECT

public:
    explicit LibraryPage(BinarySelector& selector, QWidget* parent = nullptr);

signals:
    void launchRequested(const LaunchConfig& config);
    void instancesChanged();

public slots:
    void refresh_instances();

private slots:
    void on_instance_selected(int row);
    void on_add_instance_clicked();
    void on_launch_clicked();
    void on_set_default_clicked();
    void on_open_folder_clicked();
    void on_remove_instance_clicked();

private:
    void setup_ui();
    void update_detail_view();

    BinarySelector& m_selector;
    int             m_selected_index = 0;

    QListWidget*    m_list_instances = nullptr;
    QLabel*         m_lbl_header_count = nullptr;

    // Detail Panel Widgets
    QLabel*         m_detail_icon = nullptr;
    QLabel*         m_detail_title = nullptr;
    QLabel*         m_detail_subtitle = nullptr;
    QLabel*         m_detail_arch_badge = nullptr;
    QLabel*         m_detail_default_badge = nullptr;
    QLabel*         m_detail_status_badge = nullptr;
    QLabel*         m_detail_path = nullptr;
    QLabel*         m_detail_assets = nullptr;
    QLabel*         m_detail_gametype = nullptr;
    QLabel*         m_detail_pref_label = nullptr;
    QComboBox*      m_combo_pref_base = nullptr;

    QPushButton*    m_btn_launch = nullptr;
    QPushButton*    m_btn_set_default = nullptr;
    QPushButton*    m_btn_open_folder = nullptr;
    QPushButton*    m_btn_remove = nullptr;
};

} // namespace swordfare::launcher
