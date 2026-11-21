#include "platform/tool_windows_internal.h"
#include "platform/input_config.h"
#include "platform/gui.h"
#include "tas/tas.h"

#include <QAbstractTableModel>
#include <QAction>
#include <QApplication>
#include <QBrush>
#include <QCheckBox>
#include <QColor>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPoint>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Game globals used by the tool windows.
extern InputConfig g_input_config;
void post_gui_action(GuiAction a);

namespace tool_windows {

namespace {

constexpr int kMaxButtonColumns = 63;    // bit 63 is reserved for Enter
constexpr int kExtraRowsAfterEnd = 120;  // empty frames past the end that can be edited

// Number of control button columns.
int current_button_count() {
    return std::min(g_input_config.button_count(), kMaxButtonColumns);
}

bool left_down() {
    return QApplication::mouseButtons().testFlag(Qt::LeftButton);
}

QString num(uint64_t v) {
    return QString::number(qulonglong(v));
}

QString times() {
    return QString(QChar(0x00D7));  // multiplication sign
}

// What a button cell shows for one frame.
struct Cell {
    bool held = false;
    int implied = 0;  // 1 when the button went down in this frame
    int presses = 0;  // total shown in the cell: implied + extra presses
};

// Piano roll over the movie. Columns: 0 = Frame, 1..B = buttons, B+1 = Enter, B+2 = Mouse, B+3 = RNG.
class MovieModel : public QAbstractTableModel {
public:
    explicit MovieModel(QObject* parent) : QAbstractTableModel(parent) {
        load_names(current_button_count());
    }

    tas::MovieInfo info;  // snapshot of the movie, refreshed by refresh_from_game()

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : rows_;
    }
    int columnCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : buttons() + 4;
    }
    Qt::ItemFlags flags(const QModelIndex&) const override {
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() >= rows_ || index.column() >= columnCount()) return QVariant();
        const uint64_t f = frame_of(index.row());
        const int col = index.column();
        const int bit = bit_of(col);

        if (role == Qt::BackgroundRole) {
            QColor c = row_color(f);
            if (bit >= 0) {
                const Cell cell = cell_at(f, bit);
                if (cell.held) {
                    c = (f == info.current) ? QColor(0x5a, 0x9b, 0xff) : QColor(0x7f, 0xb3, 0xff);
                } else if (cell.presses >= 1) {
                    c = QColor(0xff, 0xd2, 0x7f);
                }
            }
            return c.isValid() ? QVariant(QBrush(c)) : QVariant();
        }
        if (role == Qt::TextAlignmentRole) {
            return bit >= 0 ? QVariant(int(Qt::AlignCenter)) : QVariant();
        }
        if (role == Qt::ToolTipRole && bit >= 0) {
            const Cell cell = cell_at(f, bit);
            return QStringLiteral("%1, frame %2: %3, %4 press(es)")
                .arg(full_name(bit))
                .arg(num(f))
                .arg(cell.held ? QStringLiteral("held") : QStringLiteral("not held"))
                .arg(cell.presses);
        }
        if (role != Qt::DisplayRole) return QVariant();

        if (col == 0) return num(f);
        if (col == mouse_column()) {
            const tas::Input in = tas::movie_frame(f);
            if (!in.mouse_down) return QString();
            return QStringLiteral("%1,%2").arg(int(in.mouse_x)).arg(int(in.mouse_y));
        }
        if (col == rng_column()) {
            const uint64_t r = tas::movie_frame(f).rng;
            if (r == 0) return QString();
            return QString::number(qulonglong(r), 16).rightJustified(8, QLatin1Char('0'));
        }
        const Cell cell = cell_at(f, bit);
        if (cell.held) {
            if (cell.presses <= 1) return short_label(bit);
            return short_label(bit) + times() + QString::number(cell.presses);
        }
        if (cell.presses >= 1) return times() + QString::number(cell.presses);
        return QString();
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return QVariant();
        if (section == 0) return QStringLiteral("Frame");
        if (section == mouse_column()) return QStringLiteral("Mouse");
        if (section == rng_column()) return QStringLiteral("RNG");
        return full_name(bit_of(section));
    }

    int buttons() const { return int(names_.size()); }
    int mouse_column() const { return buttons() + 2; }
    int rng_column() const { return buttons() + 3; }

    // Frame number of a row.
    uint64_t frame_of(int row) const { return info.first + uint64_t(row); }

    // Input bit of a column, or -1 for the Frame, Mouse and RNG columns.
    int bit_of(int col) const {
        if (col >= 1 && col <= buttons()) return col - 1;
        if (col == buttons() + 1) return tas::kButtonEnter;
        return -1;
    }

    Cell cell_at(uint64_t f, int bit) const {
        const tas::Input in = tas::movie_frame(f);
        const tas::Input prev = f == 0 ? tas::Input() : tas::movie_frame(f - 1);
        Cell c;
        c.held = ((in.buttons >> bit) & 1) != 0;
        const bool was_held = ((prev.buttons >> bit) & 1) != 0;
        c.implied = (c.held && !was_held) ? 1 : 0;
        c.presses = c.implied + in.extra[bit];
        return c;
    }

    // Pulls the movie state and updates rows, columns and cell colors.
    void refresh_from_game() {
        const tas::MovieInfo now = tas::movie_info();
        const int new_rows = rows_for(now);
        const int nb = current_button_count();
        const bool changed = now.version != info.version || now.current != info.current ||
                             now.stale_from != info.stale_from;
        const bool reset = nb != buttons() || now.first != info.first;
        info = now;

        if (reset) {
            beginResetModel();
            load_names(nb);
            rows_ = new_rows;
            endResetModel();
            return;
        }
        if (new_rows > rows_) {
            beginInsertRows(QModelIndex(), rows_, new_rows - 1);
            rows_ = new_rows;
            endInsertRows();
        } else if (new_rows < rows_) {
            beginRemoveRows(QModelIndex(), new_rows, rows_ - 1);
            rows_ = new_rows;
            endRemoveRows();
        }
        if (changed && rows_ > 0) {
            emit dataChanged(index(0, 0), index(rows_ - 1, columnCount() - 1));
        }
    }

private:
    std::vector<std::string> names_;  // control button names, indexed by bit
    int rows_ = 0;

    void load_names(int n) {
        names_.clear();
        for (int i = 0; i < n; ++i) {
            TouchButton* b = g_input_config.get_button(i);
            names_.push_back(b ? b->name : std::string("?"));
        }
    }

    int rows_for(const tas::MovieInfo& m) const {
        if (m.length == 0 && !m.has_start) return 0;
        const uint64_t end = std::max(m.length, m.current + 1);
        const uint64_t frames = end > m.first ? end - m.first : 0;
        const uint64_t extra = m.mode == tas::Mode::Record ? 0 : kExtraRowsAfterEnd;
        return int(std::min<uint64_t>(frames + extra, 0x7fffffffu));
    }

    // Background of a row, or an invalid color for none.
    QColor row_color(uint64_t f) const {
        if (f == info.current) return QColor(0xcf, 0xe3, 0xff);
        if (f >= info.stale_from && f < info.current) return QColor(0xff, 0xe0, 0xb0);
        if (f < info.current) return QColor(0xe3, 0xf5, 0xe1);
        if (f >= info.length) return QColor(0xf2, 0xf2, 0xf2);
        return QColor();
    }

    QString short_label(int bit) const {
        if (bit == tas::kButtonEnter) return QStringLiteral("Ent");
        return QString::fromStdString(names_[bit].substr(0, 3));
    }

    QString full_name(int bit) const {
        if (bit == tas::kButtonEnter) return QStringLiteral("Enter");
        if (bit < 0 || bit >= buttons()) return QString();
        return QString::fromStdString(names_[bit]);
    }
};

// Sets one held bit of frame f. Turning it off also clears its extra presses.
bool set_held(uint64_t f, int bit, bool held) {
    tas::Input in = tas::movie_frame(f);
    const uint64_t mask = uint64_t(1) << bit;
    if (held) {
        in.buttons |= mask;
    } else {
        in.buttons &= ~mask;
        in.extra[bit] = 0;
    }
    return tas::set_movie_frame(f, in);
}

// Sets the total presses shown in a cell. The implied press is not stored, so it is subtracted.
bool set_presses(uint64_t f, int bit, int total, int implied) {
    tas::Input in = tas::movie_frame(f);
    in.extra[bit] = uint8_t(std::clamp(total - implied, 0, tas::kMaxExtra));
    return tas::set_movie_frame(f, in);
}

QString status_text(const tas::MovieInfo& m) {
    if (m.length == 0 && !m.has_start) {
        return QStringLiteral("No movie. Record one with F9 (File > Movie > Record).");
    }
    QString mode = QStringLiteral("PLAY");
    if (m.mode == tas::Mode::Live) mode = QStringLiteral("LIVE");
    if (m.mode == tas::Mode::Record) mode = QStringLiteral("REC");

    QString s = QStringLiteral("Frame ") + num(m.current) + QStringLiteral("   ") + mode +
                QStringLiteral("   movie ") + num(m.first) + QChar(0x2013) + num(m.length) +
                QStringLiteral("   slot ") + QString::number(tas::slot());
    if (m.seek_target != UINT64_MAX) {
        s += QStringLiteral("   SEEK ") + QChar(0x2192) + QStringLiteral(" ") + num(m.seek_target);
    }
    if (m.stale_from < m.current) {
        s += QStringLiteral("   edited from ") + num(m.stale_from) + QStringLiteral(": replay to apply");
    }
    return s;
}

}  // namespace

ToolDialog* build_tas_studio() {
    auto* d = new ToolDialog(QStringLiteral("TAS Studio"));
    d->resize(900, 600);
    auto* root = new QVBoxLayout(d);

    // Status line.
    auto* status = new QLabel;
    auto* top = new QHBoxLayout;
    top->addWidget(status, 1);
    root->addLayout(top);

    // Movie control buttons.
    auto* row = new QHBoxLayout;
    add_button(row, QStringLiteral("Play from Start"), [] { post_gui_action(GUI_TAS_PLAY); });
    add_button(row, QStringLiteral("Record / Continue"), [] { post_gui_action(GUI_TAS_RECORD); });
    add_button(row, QStringLiteral("Stop"), [] { post_gui_action(GUI_TAS_STOP_MOVIE); });
    add_button(row, QStringLiteral("Pause"), [] { post_gui_action(GUI_PAUSE); });
    add_button(row, QStringLiteral("Frame Advance"), [] { post_gui_action(GUI_STEP_FRAME); })
        ->setAutoRepeat(true);
    add_button(row, QStringLiteral("Write Movie"), [] { post_gui_action(GUI_TAS_SAVE_MOVIE); });
    row->addStretch();
    auto* follow = new QCheckBox(QStringLiteral("Follow current frame"));
    follow->setChecked(true);
    row->addWidget(follow);
    root->addLayout(row);

    // Piano roll.
    auto* view = new QTableView;
    auto* model = new MovieModel(view);
    view->setModel(model);
    view->setFont(mono_font());
    view->setSelectionMode(QAbstractItemView::NoSelection);
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->setMouseTracking(true);
    view->setContextMenuPolicy(Qt::CustomContextMenu);
    view->verticalHeader()->hide();
    view->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    view->verticalHeader()->setDefaultSectionSize(18);
    view->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    view->horizontalHeader()->setDefaultSectionSize(52);
    root->addWidget(view, 1);

    // Hint and close button.
    auto* bottom = new QHBoxLayout;
    bottom->addWidget(new QLabel(QStringLiteral("Click: toggle held ") + QChar(0x00B7) +
                                 QStringLiteral(" drag: paint ") + QChar(0x00B7) +
                                 QStringLiteral(" right-click: presses, insert, delete ") + QChar(0x00B7) +
                                 QStringLiteral(" double-click or Frame column: go to frame ") + QChar(0x00B7) +
                                 QStringLiteral(" right-click: RNG")));
    bottom->addStretch();
    add_button(bottom, QStringLiteral("Close"), [d] { d->close(); });
    root->addLayout(bottom);

    // Drag painting state, shared by the pressed and entered handlers.
    struct Paint {
        bool active = false;
        bool value = false;
        int bit = -1;
        // What the last press changed, so a double-click (which starts with
        // a press) can take the toggle back before it goes to the frame.
        bool undo = false;
        uint64_t undo_frame = 0;
        uint64_t undo_length = 0;
        tas::Input undo_input;
    };
    auto paint = std::make_shared<Paint>();

    auto refresh_all = [d] {
        for (auto& f : d->refresh) f();
    };

    // Click toggles a held cell; the press starts a drag that paints the same column.
    QObject::connect(view, &QAbstractItemView::pressed, [model, paint, refresh_all](const QModelIndex& idx) {
        paint->undo = false;
        if (!left_down()) return;
        const int bit = model->bit_of(idx.column());
        const uint64_t f = model->frame_of(idx.row());
        if (idx.column() == 0) {  // Frame column: go to that frame
            tas::seek(f);
            refresh_all();
            return;
        }
        if (bit < 0) return;
        if (!tas::editable(f)) return;
        const bool held = model->cell_at(f, bit).held;
        paint->active = true;
        paint->bit = bit;
        paint->value = !held;
        paint->undo = true;
        paint->undo_frame = f;
        paint->undo_length = tas::movie_info().length;
        paint->undo_input = tas::movie_frame(f);
        set_held(f, bit, !held);
        refresh_all();
    });

    QObject::connect(view, &QAbstractItemView::entered, [model, paint, refresh_all](const QModelIndex& idx) {
        if (!left_down()) {
            paint->active = false;
            return;
        }
        if (!paint->active || model->bit_of(idx.column()) != paint->bit) return;
        const uint64_t f = model->frame_of(idx.row());
        if (!tas::editable(f) || model->cell_at(f, paint->bit).held == paint->value) return;
        set_held(f, paint->bit, paint->value);
        refresh_all();
    });

    QObject::connect(view, &QAbstractItemView::doubleClicked, [model, paint, refresh_all](const QModelIndex& idx) {
        const uint64_t f = model->frame_of(idx.row());
        if (idx.column() == 0) return;  // the press already went there
        if (paint->undo && paint->undo_frame == f) {
            // Take back the toggle of the double-click's first press.
            tas::set_movie_frame(f, paint->undo_input);
            const tas::MovieInfo m = tas::movie_info();
            if (m.length > paint->undo_length && m.mode != tas::Mode::Record)
                tas::delete_movie_frames(paint->undo_length, m.length - paint->undo_length);
        }
        paint->undo = false;
        paint->active = false;
        tas::seek(f);
        refresh_all();
    });

    QObject::connect(view, &QWidget::customContextMenuRequested, [view, model, refresh_all](const QPoint& pos) {
        const QModelIndex idx = view->indexAt(pos);
        if (!idx.isValid()) return;
        const uint64_t f = model->frame_of(idx.row());
        const int bit = model->bit_of(idx.column());
        const bool edit = tas::editable(f);
        const bool movable = edit && tas::movie_info().mode != tas::Mode::Record;
        const QString n = num(f);

        QMenu menu;
        if (bit >= 0) {
            const Cell cell = model->cell_at(f, bit);
            QMenu* sub = menu.addMenu(QStringLiteral("Presses in frame ") + n);
            for (int p = 0; p <= 9; ++p) {
                QAction* a = sub->addAction(p == 0 ? QStringLiteral("0 (only the held state)") : QString::number(p));
                a->setCheckable(true);
                a->setChecked(cell.presses == p);
                a->setEnabled(edit && p >= cell.implied);
                QObject::connect(a, &QAction::triggered, [f, bit, p, implied = cell.implied, refresh_all] {
                    set_presses(f, bit, p, implied);
                    refresh_all();
                });
            }
            QAction* toggle = menu.addAction(QStringLiteral("Toggle held"));
            toggle->setEnabled(edit);
            QObject::connect(toggle, &QAction::triggered, [f, bit, held = cell.held, refresh_all] {
                set_held(f, bit, !held);
                refresh_all();
            });
            menu.addSeparator();
        }

        QAction* clear = menu.addAction(QStringLiteral("Clear frame ") + n);
        clear->setEnabled(edit);
        QObject::connect(clear, &QAction::triggered, [f, refresh_all] {
            // Keeps the pointer position of the frame.
            const tas::Input old = tas::movie_frame(f);
            tas::Input cleared;
            cleared.mouse_x = old.mouse_x;
            cleared.mouse_y = old.mouse_y;
            tas::set_movie_frame(f, cleared);
            refresh_all();
        });

        QAction* reroll = menu.addAction(QStringLiteral("Reroll RNG at frame ") + n);
        reroll->setEnabled(edit);
        QObject::connect(reroll, &QAction::triggered, [f, refresh_all] {
            tas::Input in = tas::movie_frame(f);
            in.rng = tas::new_rng_value();
            tas::set_movie_frame(f, in);
            refresh_all();
        });

        QAction* set_rng = menu.addAction(QStringLiteral("Set RNG value at frame ") + n + QStringLiteral("..."));
        set_rng->setEnabled(edit);
        QObject::connect(set_rng, &QAction::triggered, [f, n, view, refresh_all] {
            const uint64_t current = tas::movie_frame(f).rng;
            const QString prefill =
                current ? QString::number(qulonglong(current), 16).rightJustified(8, QLatin1Char('0')) : QString();
            bool ok = false;
            const QString text = QInputDialog::getText(view, QStringLiteral("RNG value"),
                                                       QStringLiteral("Random seed for frame ") + n +
                                                           QStringLiteral(" (hex):"),
                                                       QLineEdit::Normal, prefill, &ok);
            if (!ok) return;
            const uint64_t v = text.trimmed().toULongLong(&ok, 16);
            if (!ok || v == 0 || v > 0xffffffffull) return;
            tas::Input in = tas::movie_frame(f);
            in.rng = v;
            tas::set_movie_frame(f, in);
            refresh_all();
        });

        QAction* remove_rng = menu.addAction(QStringLiteral("Remove RNG change at frame ") + n);
        remove_rng->setEnabled(edit && tas::movie_frame(f).rng != 0);
        QObject::connect(remove_rng, &QAction::triggered, [f, refresh_all] {
            tas::Input in = tas::movie_frame(f);
            in.rng = 0;
            tas::set_movie_frame(f, in);
            refresh_all();
        });

        QAction* ins = menu.addAction(QStringLiteral("Insert frame before ") + n);
        ins->setEnabled(movable);
        QObject::connect(ins, &QAction::triggered, [f, refresh_all] {
            tas::insert_movie_frames(f, 1);
            refresh_all();
        });

        QAction* del = menu.addAction(QStringLiteral("Delete frame ") + n);
        del->setEnabled(movable);
        QObject::connect(del, &QAction::triggered, [f, refresh_all] {
            tas::delete_movie_frames(f, 1);
            refresh_all();
        });

        menu.addSeparator();
        QAction* go = menu.addAction(QStringLiteral("Go to frame ") + n);
        QObject::connect(go, &QAction::triggered, [f] { tas::seek(f); });

        menu.exec(view->viewport()->mapToGlobal(pos));
    });

    // Runs about 10 times per second while the window is visible.
    // Frame the view last scrolled to; reset on show so an opened window starts at the current frame.
    auto last_current = std::make_shared<uint64_t>(UINT64_MAX);
    d->on_visibility = [last_current](bool shown) {
        if (shown) *last_current = UINT64_MAX;
    };
    d->refresh.push_back([view, model, status, follow, last_current, last_cols = -1]() mutable {
        model->refresh_from_game();
        const tas::MovieInfo& mi = model->info;
        status->setText(status_text(mi));
        if (model->columnCount() != last_cols) {
            last_cols = model->columnCount();
            view->setColumnWidth(0, 70);
            view->setColumnWidth(model->mouse_column(), 90);
            view->setColumnWidth(model->rng_column(), 130);
        }
        if (follow->isChecked() && mi.current != *last_current && mi.current >= mi.first) {
            const uint64_t r = mi.current - mi.first;
            if (r < uint64_t(model->rowCount())) {
                view->scrollTo(model->index(int(r), 0), QAbstractItemView::PositionAtCenter);
            }
        }
        *last_current = mi.current;
    });

    refresh_all();
    return d;
}

}  // namespace tool_windows
