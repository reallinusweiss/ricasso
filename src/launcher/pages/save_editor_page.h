#pragma once
// ============================================================================
// save_editor_page.h — Built-in visual save file editor
// ============================================================================

#include <QWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include "platform/save_editor.h"

namespace swordfare::launcher {

class SaveEditorPage : public QWidget {
    Q_OBJECT

public:
    explicit SaveEditorPage(QWidget* parent = nullptr);

public slots:
    void refresh_saves();

private slots:
    void on_slot_changed(int index);
    void on_save_clicked();
    void on_backup_clicked();

private:
    void setup_ui();
    void populate_fields();

    QComboBox*   m_combo_slots = nullptr;
    QSpinBox*    m_spin_coins = nullptr;
    QSpinBox*    m_spin_health = nullptr;
    QSpinBox*    m_spin_mana = nullptr;
    QSpinBox*    m_spin_level = nullptr;
    QSpinBox*    m_spin_xp = nullptr;
    QLabel*      m_lbl_status = nullptr;
    QPushButton* m_btn_save = nullptr;
    QPushButton* m_btn_backup = nullptr;

    std::vector<std::string> m_save_paths;
    SaveFile                 m_current_save;
    bool                     m_has_save = false;
};

} // namespace swordfare::launcher
