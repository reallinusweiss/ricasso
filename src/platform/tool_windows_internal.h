#ifndef TOOL_WINDOWS_INTERNAL_H
#define TOOL_WINDOWS_INTERNAL_H

#include "platform/tool_windows.h"

#include <QDialog>
#include <QString>
#include <QStringList>
#include <QFont>

#include <functional>
#include <vector>

class QFormLayout;
class QBoxLayout;
class QCheckBox;
class QComboBox;
class QPushButton;
class QMessageBox;

namespace tool_windows {

using Refresh = std::vector<std::function<void()>>;

// Base of every tool window. No Q_OBJECT: only virtuals are overridden.
class ToolDialog : public QDialog {
public:
    explicit ToolDialog(const QString& title);
    Refresh refresh;                          // run about 10 times per second while the window is visible
    std::function<void(bool)> on_visibility;  // optional: called with true on show, false on hide
protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    bool nativeEvent(const QByteArray& type, void* message, qintptr* result) override;
};

// Rows for QFormLayout pages. Each helper registers what it needs in `refresh`
// so the widget follows changes made elsewhere (presets, hotkeys, the game).
QCheckBox* add_check(QFormLayout* form, Refresh& refresh, const QString& text, bool* value,
                     std::function<bool()> enabled = {});
QCheckBox* add_check(QFormLayout* form, Refresh& refresh, const QString& text,
                     std::function<bool()> get, std::function<void(bool)> set);
// Slider with the value shown to its right. A slider that has `enabled` is a sub-setting and is indented.
void add_slider(QFormLayout* form, Refresh& refresh, const QString& label, float* value, float lo, float hi,
                int decimals, std::function<bool()> enabled = {}, std::function<void()> changed = {});
QComboBox* add_combo(QFormLayout* form, Refresh& refresh, const QString& label, const QStringList& items,
                     std::function<int()> get, std::function<void(int)> set);
QPushButton* add_button(QBoxLayout* row, const QString& text, std::function<void()> clicked);

QFont mono_font();                  // Consolas 10pt
void focus_game_window();           // give keyboard focus back to the game window
int run_modal(QMessageBox& box);    // exec() while game frames keep running

ToolDialog* build_graphics();
ToolDialog* build_cheats();
ToolDialog* build_hotkeys();
ToolDialog* build_about();
ToolDialog* build_controls();
ToolDialog* build_scene_select();
ToolDialog* build_lua_console();
ToolDialog* build_lua_scripts();
ToolDialog* build_error_log();

}  // namespace tool_windows

#endif
