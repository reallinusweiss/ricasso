#define NOMINMAX
#include <windows.h>

#include "platform/tool_windows_internal.h"
#include "platform/swordfare_gui.h"
#include "platform/input_config.h"
#include "platform/os_external.h"
#include "platform/gui.h"

#include <SDL3/SDL.h>

#include <QApplication>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QObject>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QAbstractItemView>
#include <QVBoxLayout>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

// Game globals used by the tool windows.
extern SwordfareGUI g_swordfare_gui;
extern InputConfig g_input_config;
extern std::string g_save_dir;
void post_gui_action(GuiAction a);

namespace tool_windows {

namespace {

// Runs every refresh function once so the first paint shows real values.
void run_refresh(ToolDialog* d) {
    for (auto& f : d->refresh) f();
}

QLabel* wrap_label(const QString& text) {
    auto* label = new QLabel(text);
    label->setWordWrap(true);
    return label;
}

// Dialog button row with only Close.
QDialogButtonBox* close_box(ToolDialog* d) {
    auto* box = new QDialogButtonBox(QDialogButtonBox::Close);
    QObject::connect(box, &QDialogButtonBox::rejected, [d] { d->hide(); });
    return box;
}

// Null when the row is out of range.
TouchButton* button_at(int row) {
    return (row >= 0 && row < g_input_config.button_count()) ? g_input_config.get_button(row) : nullptr;
}

// Controller column text, same rules as the ImGui editor.
QString pad_label_of(const TouchButton& b) {
    std::string pad_label = "-";
    if (b.gamepad_button >= 0) {
        // Xbox-style names; SDL's own strings are terse ("dpleft", "a")
        static const char* const kPadNames[] = {
            "A", "B", "X", "Y", "Back", "Guide", "Start", "L Stick", "R Stick",
            "LB", "RB", "D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right"};
        if (b.gamepad_button < (int)(sizeof(kPadNames) / sizeof(kPadNames[0])))
            pad_label = kPadNames[b.gamepad_button];
        else if (const char* s = SDL_GetGamepadStringForButton((SDL_GamepadButton)b.gamepad_button))
            pad_label = s;
        else pad_label = "Button " + std::to_string(b.gamepad_button);
    } else if (b.gamepad_axis >= 0) {
        const char* s = SDL_GetGamepadStringForAxis((SDL_GamepadAxis)b.gamepad_axis);
        if (s) pad_label = s;
        else pad_label = "Axis " + std::to_string(b.gamepad_axis);
    }
    return QString::fromStdString(pad_label);
}

// Five cells per button: action, key 1, key 2, controller, position.
QList<QStringList> controls_rows() {
    QList<QStringList> rows;
    for (int i = 0; i < g_input_config.button_count(); ++i) {
        const TouchButton* b = button_at(i);
        if (!b) continue;
        QStringList cells;
        cells << QString::fromStdString(g_input_config.display_name_of(i)) + (b->is_macro ? QStringLiteral(" (macro)") : QString());
        cells << QString::fromStdString(InputConfig::scancode_name(b->sdl_scancode));
        cells << QString::fromStdString(InputConfig::scancode_name(b->sdl_scancode_alt));
        cells << pad_label_of(*b);
        cells << QStringLiteral("%1, %2").arg(qRound(b->game_x)).arg(qRound(b->game_y));
        rows << cells;
    }
    return rows;
}

void fill_controls_table(QTableWidget* table, const QList<QStringList>& rows) {
    QSignalBlocker block(table);
    const int cur_row = table->currentRow();
    const int cur_col = table->currentColumn();
    table->setRowCount((int)rows.size());
    for (int r = 0; r < (int)rows.size(); ++r) {
        const TouchButton* b = button_at(r);
        for (int c = 0; c < 5; ++c) {
            QTableWidgetItem* item = table->item(r, c);
            if (!item) {
                item = new QTableWidgetItem;
                table->setItem(r, c, item);
            }
            item->setText(rows.at(r).at(c));
            Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
            if (c == 0 && b && b->is_custom) flags |= Qt::ItemIsEditable;
            item->setFlags(flags);
        }
    }
    if (cur_row >= 0 && cur_row < table->rowCount()) table->setCurrentCell(cur_row, cur_col);
}

// Scene Select: Go and Go Instantly use the selected entry.
void go_scene(QListWidget* list, QLineEdit* spawn, bool forced) {
    const QListWidgetItem* it = list->currentItem();
    if (!it || !it->flags().testFlag(Qt::ItemIsSelectable)) return;
    const std::string scene = it->text().toStdString();
    const std::string spawn_name = spawn->text().toStdString();
    g_swordfare_gui.scene_shifter_go(scene.c_str(), spawn_name.c_str(), forced);
}

// Lua Console input: keeps the command history and handles Up/Down recall.
class CommandLine : public QObject {
public:
    explicit CommandLine(QObject* parent) : QObject(parent) {}
    QStringList history;
    int pos = -1;  // -1 = not browsing

protected:
    bool eventFilter(QObject* obj, QEvent* e) override {
        if (e->type() == QEvent::KeyPress && !history.isEmpty()) {
            auto* edit = qobject_cast<QLineEdit*>(obj);
            auto* ke = static_cast<QKeyEvent*>(e);
            if (edit && ke->key() == Qt::Key_Up) {
                if (pos < 0) pos = (int)history.size();
                if (pos > 0) --pos;
                edit->setText(history.at(pos));
                return true;
            }
            if (edit && ke->key() == Qt::Key_Down) {
                if (pos >= 0 && pos < (int)history.size() - 1) {
                    ++pos;
                    edit->setText(history.at(pos));
                } else {
                    pos = -1;
                    edit->clear();
                }
                return true;
            }
        }
        return QObject::eventFilter(obj, e);
    }
};

void run_lua_line(QLineEdit* input, CommandLine* hist) {
    const QString text = input->text().trimmed();
    if (text.isEmpty()) return;
    g_swordfare_gui.console_run(text.toStdString());
    if (hist->history.isEmpty() || hist->history.last() != text) hist->history << text;
    hist->pos = -1;
    input->clear();
}

std::filesystem::path lua_scripts_dir() {
    return std::filesystem::path(os_external::home_dir()) / ".local" / "share" / "ricasso" / "lua_scripts";
}

// Heuristic syntax check: brackets and quotes must balance outside comments.
bool lua_brackets_balanced(const std::string& code) {
    int brackets_sq = 0, brackets_curl = 0, brackets_paren = 0;
    int quotes_single = 0, quotes_double = 0;
    bool in_comment = false;

    for (size_t i = 0; i < code.length(); i++) {
        if (in_comment && code[i] == '\n') in_comment = false;
        if (!in_comment && i + 1 < code.length() && code[i] == '-' && code[i+1] == '-') { in_comment = true; i++; continue; }
        if (in_comment) continue;

        if (code[i] == '[') brackets_sq++;
        if (code[i] == ']') brackets_sq--;
        if (code[i] == '{') brackets_curl++;
        if (code[i] == '}') brackets_curl--;
        if (code[i] == '(') brackets_paren++;
        if (code[i] == ')') brackets_paren--;
        if (code[i] == '\'') quotes_single++;
        if (code[i] == '"') quotes_double++;
    }

    return !(brackets_sq != 0 || brackets_curl != 0 || brackets_paren != 0 ||
             (quotes_single % 2) != 0 || (quotes_double % 2) != 0);
}

}  // namespace

ToolDialog* build_controls() {
    auto* d = new ToolDialog(QStringLiteral("Controls"));
    auto* root = new QVBoxLayout(d);

    root->addWidget(wrap_label(QStringLiteral("Double-click a binding, then press a key or controller button. Drag the circles on the game screen to move a control.")));

    auto* status = new QLabel;
    QFont sf = status->font();
    sf.setBold(true);
    status->setFont(sf);
    root->addWidget(status);
    d->refresh.push_back([status] {
        QString text;
        if (g_input_config.is_capturing()) {
            const QString name = QString::fromStdString(g_input_config.display_name_of(g_input_config.capture_button_index()));
            text = g_input_config.get_capture_slot() == 2
                ? QStringLiteral("Press a controller button for %1 ...").arg(name)
                : QStringLiteral("Press a key for %1 ...").arg(name);
        } else if (g_input_config.is_placing_tap()) {
            text = QStringLiteral("Click on the game screen to place the tap point.");
        }
        if (status->text() != text) status->setText(text);
    });

    auto* table = new QTableWidget(0, 5);
    table->setHorizontalHeaderLabels({QStringLiteral("Action"), QStringLiteral("Key 1"), QStringLiteral("Key 2"),
                                      QStringLiteral("Controller"), QStringLiteral("Position")});
    table->verticalHeader()->hide();
    table->setSelectionBehavior(QAbstractItemView::SelectItems);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::DoubleClicked);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->setWordWrap(false);
    root->addWidget(table, 1);

    d->refresh.push_back([table, shown = QStringList()]() mutable {
        const QList<QStringList> rows = controls_rows();
        QStringList sig;
        for (const QStringList& r : rows)
            for (const QString& cell : r) sig << cell;
        if (sig == shown) return;
        // Do not replace the rows while a cell editor is open.
        auto* fw = qobject_cast<QLineEdit*>(QApplication::focusWidget());
        if (fw && table->isAncestorOf(fw)) return;
        fill_controls_table(table, rows);
        shown = sig;
    });

    QObject::connect(table, &QTableWidget::cellDoubleClicked, [](int row, int col) {
        if (col >= 1 && col <= 3) {
            g_input_config.begin_capture(row, col - 1);
            focus_game_window();
        }
    });
    QObject::connect(table, &QTableWidget::itemChanged, [](QTableWidgetItem* item) {
        if (item->column() != 0) return;
        TouchButton* b = button_at(item->row());
        if (!b || !b->is_custom) return;
        QString text = item->text();
        const QString suffix = QStringLiteral(" (macro)");
        if (text.endsWith(suffix)) text.chop(suffix.size());
        b->display_name = text.toStdString();
        g_input_config.commit_edit();
    });

    auto* row = new QHBoxLayout;
    add_button(row, QStringLiteral("Clear Binding"), [table] {
        const int r = table->currentRow();
        const int c = table->currentColumn();
        if (button_at(r) && c >= 1 && c <= 3) g_input_config.clear_binding(r, c - 1);
    });
    QPushButton* tap = add_button(row, QStringLiteral("Add Tap Point"), [] {
        if (g_input_config.is_placing_tap()) {
            g_input_config.cancel_place_tap();
        } else {
            g_input_config.begin_place_tap();
            focus_game_window();
        }
    });
    QPushButton* del = add_button(row, QStringLiteral("Delete Tap Point"), [table] {
        const int r = table->currentRow();
        const TouchButton* b = button_at(r);
        if (b && b->is_custom) g_input_config.remove_button(r);
    });
    add_button(row, QStringLiteral("Reset to Defaults..."), [d] {
        QMessageBox box(QMessageBox::Question, QStringLiteral("Reset controls"),
                        QStringLiteral("All bindings and tap points return to the defaults."),
                        QMessageBox::Ok | QMessageBox::Cancel, d);
        if (run_modal(box) == QMessageBox::Ok) g_input_config.reset_to_defaults();
    });
    d->refresh.push_back([tap, del, table] {
        const QString text = g_input_config.is_placing_tap() ? QStringLiteral("Cancel Tap Point")
                                                             : QStringLiteral("Add Tap Point");
        if (tap->text() != text) tap->setText(text);
        del->setEnabled(button_at(table->currentRow()) && button_at(table->currentRow())->is_custom);
    });
    row->addStretch();
    row->addWidget(close_box(d));
    root->addLayout(row);

    d->on_visibility = [](bool on) {
        g_input_config.set_editing(on);
        if (!on) g_input_config.commit_edit();
    };
    d->resize(640, 520);
    run_refresh(d);
    return d;
}

ToolDialog* build_scene_select() {
    auto* d = new ToolDialog(QStringLiteral("Scene Select"));
    auto* root = new QVBoxLayout(d);

    auto* current = new QLabel;
    root->addWidget(current);
    auto* filter = new QLineEdit;
    filter->setPlaceholderText(QStringLiteral("Filter"));
    root->addWidget(filter);
    auto* list = new QListWidget;
    root->addWidget(list, 1);

    auto* spawn_row = new QHBoxLayout;
    spawn_row->addWidget(new QLabel(QStringLiteral("Spawn point:")));
    auto* spawn = new QLineEdit(QStringLiteral("start"));
    spawn_row->addWidget(spawn, 1);
    root->addLayout(spawn_row);

    auto* status = new QLabel;
    root->addWidget(status);

    auto* row = new QHBoxLayout;
    QPushButton* go = add_button(row, QStringLiteral("Go"), [list, spawn] { go_scene(list, spawn, false); });
    go->setDefault(true);
    go->setToolTip(QStringLiteral("Changes the scene with the normal loading screen."));
    QPushButton* go_now = add_button(row, QStringLiteral("Go Instantly"), [list, spawn] { go_scene(list, spawn, true); });
    go_now->setToolTip(QStringLiteral("Changes the scene without the loading screen."));
    add_button(row, QStringLiteral("Refresh List"), [] { g_swordfare_gui.scene_shifter_rescan(); });
    row->addStretch();
    row->addWidget(close_box(d));
    root->addLayout(row);

    QObject::connect(list, &QListWidget::itemDoubleClicked, [list, spawn](QListWidgetItem*) {
        go_scene(list, spawn, false);
    });

    d->refresh.push_back([current, filter, list, status, go, go_now, shown = QStringList()]() mutable {
        const auto s = g_swordfare_gui.scene_shifter_state();

        const QString scene = (s.current && s.current[0]) ? QString::fromUtf8(s.current) : QString();
        const QString label = QStringLiteral("Current scene: ") + (scene.isEmpty() ? QStringLiteral("(unknown)") : scene);
        if (current->text() != label) current->setText(label);

        QStringList want;
        if (s.count == 0 || !s.names) {
            want << QStringLiteral("No scenes found. Use Refresh.");
        } else {
            const QString needle = filter->text();
            for (int i = 0; i < s.count; ++i) {
                const QString name = QString::fromUtf8(s.names[i]);
                if (needle.isEmpty() || name.contains(needle, Qt::CaseInsensitive)) want << name;
            }
        }
        if (want != shown) {
            const QListWidgetItem* prev_item = list->currentItem();
            const QString prev = prev_item ? prev_item->text() : QString();
            shown = want;
            list->clear();
            for (const QString& name : want) list->addItem(name);
            if (s.count == 0 || !s.names) {
                list->item(0)->setFlags(Qt::NoItemFlags);
            } else {
                for (int i = 0; i < list->count(); ++i) {
                    if (list->item(i)->text() == prev) {
                        list->setCurrentRow(i);
                        break;
                    }
                }
            }
        }

        const QListWidgetItem* it = list->currentItem();
        const bool selected = it && it->flags().testFlag(Qt::ItemIsSelectable);
        go->setEnabled(selected && !s.busy);
        go_now->setEnabled(selected && !s.busy);

        const QString st = s.busy ? QStringLiteral("Loading ...") : QString::fromUtf8(s.error);
        if (status->text() != st) status->setText(st);
    });

    d->resize(360, 520);
    g_swordfare_gui.scene_shifter_rescan();
    run_refresh(d);
    return d;
}

ToolDialog* build_lua_console() {
    auto* d = new ToolDialog(QStringLiteral("Lua Console"));
    auto* root = new QVBoxLayout(d);

    auto* output = new QPlainTextEdit;
    output->setReadOnly(true);
    output->setFont(mono_font());
    output->setLineWrapMode(QPlainTextEdit::NoWrap);
    root->addWidget(output, 1);

    auto* input_row = new QHBoxLayout;
    auto* input = new QLineEdit;
    input->setFont(mono_font());
    input->setPlaceholderText(QStringLiteral("Lua code, Enter runs it"));
    input_row->addWidget(input, 1);
    auto* run = new QPushButton(QStringLiteral("Run"));
    run->setDefault(true);
    input_row->addWidget(run);
    root->addLayout(input_row);

    auto* hist = new CommandLine(input);
    input->installEventFilter(hist);
    QObject::connect(input, &QLineEdit::returnPressed, [input, hist] { run_lua_line(input, hist); });
    QObject::connect(run, &QPushButton::clicked, [input, hist] { run_lua_line(input, hist); });

    auto* row = new QHBoxLayout;
    add_button(row, QStringLiteral("Clear"), [] { g_swordfare_gui.console_clear(); });
    add_button(row, QStringLiteral("Scripts..."), [] { post_gui_action(GUI_TOGGLE_LUA_SCRIPTS); });
    row->addStretch();
    QDialogButtonBox* close = close_box(d);
    close->button(QDialogButtonBox::Close)->setAutoDefault(false);
    row->addWidget(close);
    root->addLayout(row);

    d->refresh.push_back([output, input, run, shown_n = size_t(-1), shown_last = std::string()]() mutable {
        const bool ready = g_swordfare_gui.is_lua_console_ready();
        input->setEnabled(ready);
        run->setEnabled(ready);

        const auto& h = g_swordfare_gui.console_history();
        const std::string last = h.empty() ? std::string() : h.back().text;
        if (h.size() == shown_n && last == shown_last) return;
        shown_n = h.size();
        shown_last = last;

        QString text;
        if (h.empty() && !ready) {
            text = QStringLiteral("The Lua console is not available for this game version.");
        } else {
            QStringList lines;
            for (const auto& e : h) lines << QString::fromUtf8(e.text.data(), (int)e.text.size());
            text = lines.join(QLatin1Char('\n'));
        }
        output->setPlainText(text);
        output->verticalScrollBar()->setValue(output->verticalScrollBar()->maximum());
    });

    d->resize(680, 440);
    run_refresh(d);
    return d;
}

ToolDialog* build_lua_scripts() {
    auto* d = new ToolDialog(QStringLiteral("Lua Scripts"));
    auto* root = new QVBoxLayout(d);

    auto* split = new QSplitter(Qt::Horizontal);
    auto* list = new QListWidget;
    split->addWidget(list);

    auto* right = new QWidget;
    auto* right_col = new QVBoxLayout(right);
    auto* name_row = new QHBoxLayout;
    name_row->addWidget(new QLabel(QStringLiteral("File name:")));
    auto* name = new QLineEdit(QStringLiteral("mod_script.lua"));
    name_row->addWidget(name, 1);
    right_col->addLayout(name_row);
    auto* editor = new QPlainTextEdit;
    editor->setFont(mono_font());
    editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    editor->setTabStopDistance(4 * QFontMetrics(editor->font()).horizontalAdvance(QLatin1Char(' ')));
    right_col->addWidget(editor, 1);
    split->addWidget(right);
    split->setStretchFactor(1, 1);
    root->addWidget(split, 1);

    auto* status = new QLabel;
    root->addWidget(status);

    auto rescan = [list] {
        list->clear();
        QStringList names;
        std::error_code ec;
        for (std::filesystem::directory_iterator it(lua_scripts_dir(), ec), end; !ec && it != end; it.increment(ec)) {
            const std::filesystem::path p = it->path();
            if (p.extension() == ".lua") names << QString::fromStdString(p.filename().string());
        }
        names.sort();
        for (const QString& n : names) list->addItem(n);
    };

    QObject::connect(list, &QListWidget::currentItemChanged, [=](QListWidgetItem* cur, QListWidgetItem*) {
        if (!cur) return;
        std::ifstream in(lua_scripts_dir() / cur->text().toStdString());
        const std::string code((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        editor->setPlainText(QString::fromStdString(code));
        name->setText(cur->text());
        status->setText(QStringLiteral("Loaded ") + cur->text());
    });

    auto* row = new QHBoxLayout;
    add_button(row, QStringLiteral("New"), [=] {
        editor->clear();
        name->setText(QStringLiteral("mod_script.lua"));
        list->setCurrentRow(-1);  // clears the selection
        status->clear();
    });
    add_button(row, QStringLiteral("Save"), [=] {
        std::string fname = name->text().trimmed().toStdString();
        if (fname.empty()) {
            status->setText(QStringLiteral("Enter a file name."));
            return;
        }
        if (std::filesystem::path(fname).extension().empty()) fname += ".lua";
        const QString shown = QString::fromStdString(fname);

        const std::filesystem::path dir = lua_scripts_dir();
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        const std::string code = editor->toPlainText().toStdString();
        std::ofstream out(dir / fname);
        out << code;
        out.close();
        if (!out) {
            status->setText(QStringLiteral("Could not save ") + shown);
        } else if (!lua_brackets_balanced(code)) {
            status->setText(QStringLiteral("Saved ") + shown + QStringLiteral(" (warning: unmatched bracket or quote)"));
        } else {
            status->setText(QStringLiteral("Saved ") + shown);
        }
        rescan();
    });
    QPushButton* run = add_button(row, QStringLiteral("Run"), [=] {
        g_swordfare_gui.console_run(editor->toPlainText().toStdString());
        status->setText(QStringLiteral("Sent to the Lua console"));
    });
    add_button(row, QStringLiteral("Refresh List"), [=] { rescan(); });
    row->addStretch();
    row->addWidget(close_box(d));
    root->addLayout(row);

    d->refresh.push_back([run] {
        run->setEnabled(g_swordfare_gui.is_lua_console_ready());
    });

    d->resize(760, 520);
    rescan();
    run_refresh(d);
    return d;
}

ToolDialog* build_error_log() {
    auto* d = new ToolDialog(QStringLiteral("Lua Error Log"));
    auto* root = new QVBoxLayout(d);

    const std::string path = g_save_dir + "/external/sre_lua_errors.log";

    auto* view = new QPlainTextEdit;
    view->setReadOnly(true);
    view->setFont(mono_font());
    view->setLineWrapMode(QPlainTextEdit::NoWrap);
    root->addWidget(view, 1);

    auto* row = new QHBoxLayout;
    add_button(row, QStringLiteral("Clear Log"), [path] {
        if (FILE* f = fopen(path.c_str(), "w")) fclose(f);
    });
    row->addStretch();
    row->addWidget(close_box(d));
    root->addLayout(row);

    // Reads the file about once per second (every 10th refresh call, the first one included).
    d->refresh.push_back([view, path, shown = QString(), tick = 0u]() mutable {
        const bool due = (tick++ % 10) == 0;
        if (!due) return;
        std::string data;
        std::ifstream in(path, std::ios::binary);
        if (in) {
            in.seekg(0, std::ios::end);
            std::streamoff size = in.tellg();
            if (size < 0) size = 0;
            const std::streamoff start = size > 262144 ? size - 262144 : 0;
            in.seekg(start);
            data.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        }
        const QString text = data.empty() ? QStringLiteral("No errors recorded.") : QString::fromStdString(data);
        if (text == shown) return;
        shown = text;
        view->setPlainText(text);
        view->verticalScrollBar()->setValue(view->verticalScrollBar()->maximum());
    });

    d->resize(680, 420);
    run_refresh(d);
    return d;
}

}  // namespace tool_windows
