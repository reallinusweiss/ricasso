#define NOMINMAX
#include <windows.h>

#include "platform/tool_windows_internal.h"
#include "platform/win32_menu.h"
#include "platform/hotkeys.h"
#include "platform/gui.h"
#include "platform/fbo_scaler.h"
#include "platform/ffp_forward.h"

#include <SDL3/SDL.h>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QLineEdit>
#include <QDir>
#include <QPushButton>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QTabWidget>
#include <QDialogButtonBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QMessageBox>
#include <QElapsedTimer>
#include <QSignalBlocker>
#include <QScreen>
#include <QIcon>
#include <QPixmap>
#include <QImage>
#include <QAbstractItemView>
#include <QDesktopServices>
#include <QUrl>
#include <QComboBox>

#include <algorithm>
#include <clocale>
#include <string>

// Game globals used by the tool windows.
extern PostFXState g_postfx;
extern PostFXPreset g_postfx_preset;
extern FBOScale g_fbo_mode;
extern int GAME_W, GAME_H;
extern GuiRenderer g_gui;
extern std::string g_save_dir;
bool mod_cheats_available();
void post_gui_action(GuiAction a);

namespace tool_windows {

namespace {

QApplication* s_app = nullptr;
HWND s_owner = nullptr;
SDL_Window* s_sdl_window = nullptr;
ToolDialog* s_windows[COUNT] = {};
QElapsedTimer s_refresh_clock;

// Fullscreen mode list, filled by build_graphics(). The array is never freed.
SDL_DisplayMode** s_modes = nullptr;
int s_mode_count = 0;
int s_fullscreen_mode = 0;  // 0 = desktop resolution, n = s_modes[n - 1]

// Automated tests set this hint so no window of this app takes focus.
bool quiet() {
    static const bool q = !SDL_GetHintBoolean(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, true);
    return q;
}

void ensure_app() {
    if (s_app) return;
    if (QCoreApplication::instance()) {
        s_app = qobject_cast<QApplication*>(QCoreApplication::instance());
    } else {
        static int argc = 1;
        static char arg0[] = "swordfare";
        static char* argv[] = {arg0, nullptr};
        // Qt changes the C locale on startup; the client parses and writes numbers with the C locale.
        const std::string old_locale = setlocale(LC_ALL, nullptr);
        QCoreApplication::setAttribute(Qt::AA_PluginApplication);  // the SDL client owns the process
        s_app = new QApplication(argc, argv);
        setlocale(LC_ALL, old_locale.c_str());
    }
    if (!s_app) return;
    // windowsvista = native Win32 controls, matches the menu bar.
    if (!QApplication::setStyle(QStringLiteral("windowsvista"))) QApplication::setStyle(QStringLiteral("fusion"));
    s_app->setQuitOnLastWindowClosed(false);
    if (HICON icon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR)))
        QApplication::setWindowIcon(QIcon(QPixmap::fromImage(QImage::fromHICON(icon))));
}

// Runs every refresh function once so the first paint shows real values.
void run_refresh(ToolDialog* d) {
    for (auto& f : d->refresh) f();
}

// Dialog button row: optional action button, then Close.
QDialogButtonBox* button_box(ToolDialog* d, const QString& action_text, std::function<void()> action) {
    auto* box = new QDialogButtonBox(QDialogButtonBox::Close);
    if (!action_text.isEmpty()) {
        QPushButton* b = box->addButton(action_text, QDialogButtonBox::ActionRole);
        QObject::connect(b, &QPushButton::clicked, [action] { action(); });
    }
    QObject::connect(box, &QDialogButtonBox::rejected, [d] { d->hide(); });
    return box;
}

QFormLayout* tab_page(QTabWidget* tabs, const QString& title) {
    auto* w = new QWidget;
    auto* form = new QFormLayout(w);
    tabs->addTab(w, title);
    return form;
}

QLabel* text_label(const QString& text) {
    auto* label = new QLabel(text);
    label->setWordWrap(true);
    label->setMinimumWidth(380);
    return label;
}

ToolDialog* build(Id id) {
    switch (id) {
    case GRAPHICS: return build_graphics();
    case CHEATS: return build_cheats();
    case HOTKEYS: return build_hotkeys();
    case ABOUT: return build_about();
    case CONTROLS: return build_controls();
    case SCENE_SELECT: return build_scene_select();
    case LUA_CONSOLE: return build_lua_console();
    case LUA_SCRIPTS: return build_lua_scripts();
    case ERROR_LOG: return build_error_log();
    default: return nullptr;
    }
}

}  // namespace

ToolDialog::ToolDialog(const QString& title) : QDialog(nullptr) {
    setWindowTitle(title);
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
    // Qt reads this when it creates the native window, so it has to be set here.
    if (quiet()) setAttribute(Qt::WA_ShowWithoutActivating);
}

void ToolDialog::showEvent(QShowEvent* e) {
    QDialog::showEvent(e);
    if (on_visibility) on_visibility(true);
}

void ToolDialog::hideEvent(QHideEvent* e) {
    QDialog::hideEvent(e);
    if (on_visibility) on_visibility(false);
}

bool ToolDialog::nativeEvent(const QByteArray& type, void* message, qintptr* result) {
    const MSG* m = static_cast<const MSG*>(message);
    // Dragging the window runs a Windows modal loop; the game must not freeze meanwhile.
    if (m->message == WM_ENTERSIZEMOVE) win32_menu::modal_begin();
    else if (m->message == WM_EXITSIZEMOVE) win32_menu::modal_end();
    // F1-F12 pressed here go to the game window, where they are hotkeys (not Alt+F4).
    const bool key_msg = m->message == WM_KEYDOWN || m->message == WM_KEYUP ||
                         m->message == WM_SYSKEYDOWN || m->message == WM_SYSKEYUP;
    if (key_msg && m->wParam >= VK_F1 && m->wParam <= VK_F12 && !(HIWORD(m->lParam) & KF_ALTDOWN) && s_owner) {
        PostMessageW(s_owner, m->message, m->wParam, m->lParam);
        if (result) *result = 0;
        return true;
    }
    return QDialog::nativeEvent(type, message, result);
}

// ---- row helpers ----

QCheckBox* add_check(QFormLayout* form, Refresh& refresh, const QString& text, bool* value,
                     std::function<bool()> enabled) {
    auto* cb = new QCheckBox(text);
    form->addRow(cb);
    QObject::connect(cb, &QCheckBox::clicked, [value](bool on) { *value = on; });
    refresh.push_back([cb, value, enabled] {
        if (cb->isChecked() != *value) cb->setChecked(*value);
        if (enabled) cb->setEnabled(enabled());
    });
    return cb;
}

QCheckBox* add_check(QFormLayout* form, Refresh& refresh, const QString& text,
                     std::function<bool()> get, std::function<void(bool)> set) {
    auto* cb = new QCheckBox(text);
    form->addRow(cb);
    QObject::connect(cb, &QCheckBox::clicked, [set](bool on) { set(on); });
    refresh.push_back([cb, get] {
        if (cb->isChecked() != get()) cb->setChecked(get());
    });
    return cb;
}

void add_slider(QFormLayout* form, Refresh& refresh, const QString& label, float* value, float lo, float hi,
                int decimals, std::function<bool()> enabled, std::function<void()> changed) {
    auto* name = new QLabel(label);
    if (enabled) name->setContentsMargins(18, 0, 0, 0);
    auto* box = new QWidget;
    auto* row = new QHBoxLayout(box);
    row->setContentsMargins(0, 0, 0, 0);
    auto* slider = new QSlider(Qt::Horizontal);
    slider->setRange(0, 1000);
    slider->setMinimumWidth(220);
    auto* text = new QLabel;
    text->setFixedWidth(52);
    text->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    row->addWidget(slider, 1);
    row->addWidget(text);
    form->addRow(name, box);
    QObject::connect(slider, &QSlider::valueChanged, [=](int pos) {
        *value = lo + (hi - lo) * pos / 1000.0f;
        text->setText(QString::number(*value, 'f', decimals));
        if (changed) changed();
    });
    refresh.push_back([=] {
        int pos = qRound((*value - lo) / (hi - lo) * 1000.0f);
        pos = qBound(0, pos, 1000);
        if (!slider->isSliderDown() && slider->value() != pos) {
            QSignalBlocker b(slider);
            slider->setValue(pos);
        }
        const QString s = QString::number(*value, 'f', decimals);
        if (text->text() != s) text->setText(s);
        if (enabled) {
            const bool on = enabled();
            name->setEnabled(on);
            box->setEnabled(on);
        }
    });
}

QComboBox* add_combo(QFormLayout* form, Refresh& refresh, const QString& label, const QStringList& items,
                     std::function<int()> get, std::function<void(int)> set) {
    auto* combo = new QComboBox;
    combo->addItems(items);
    form->addRow(label, combo);
    QObject::connect(combo, QOverload<int>::of(&QComboBox::activated), [set](int i) { set(i); });
    refresh.push_back([combo, get] {
        if (!combo->view()->isVisible() && combo->currentIndex() != get()) {
            QSignalBlocker b(combo);
            combo->setCurrentIndex(get());
        }
    });
    return combo;
}

QPushButton* add_button(QBoxLayout* row, const QString& text, std::function<void()> clicked) {
    auto* b = new QPushButton(text);
    row->addWidget(b);
    QObject::connect(b, &QPushButton::clicked, [clicked] { clicked(); });
    return b;
}

// ---- helpers ----

QFont mono_font() {
    QFont f(QStringLiteral("Consolas"), 10);
    f.setStyleHint(QFont::Monospace);
    return f;
}

void focus_game_window() {
    if (s_owner && !quiet()) SetForegroundWindow(s_owner);
}

int run_modal(QMessageBox& box) {
    win32_menu::modal_begin();
    int r = box.exec();
    win32_menu::modal_end();
    return r;
}

// ---- public API ----

void set_owner(SDL_Window* game_window) {
    s_sdl_window = game_window;
    if (game_window) {
        s_owner = static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(game_window),
                                                           SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    }
}

void show(Id id) {
    ensure_app();
    if (!s_app) return;
    if (!s_windows[id]) s_windows[id] = build(id);
    ToolDialog* d = s_windows[id];
    if (!d) return;
    if (!d->isVisible()) {
        const HWND h = reinterpret_cast<HWND>(d->winId());  // creates the native window
        if (s_owner) SetWindowLongPtrW(h, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(s_owner));  // stays above the game, no taskbar button
        if (!d->property("placed").toBool()) {
            d->setProperty("placed", true);
            if (!d->testAttribute(Qt::WA_Resized)) d->adjustSize();
            POINT origin{0, 0};
            if (s_owner) ClientToScreen(s_owner, &origin);
            const qreal dpr = d->devicePixelRatioF();
            QPoint pos(int(origin.x / dpr) + 32 + 24 * id, int(origin.y / dpr) + 32 + 24 * id);
            if (QScreen* scr = QGuiApplication::screenAt(pos)) {  // keep it on that screen
                const QRect a = scr->availableGeometry();
                pos.setX(qBound(a.left(), pos.x(), qMax(a.left(), a.right() - d->width())));
                pos.setY(qBound(a.top(), pos.y(), qMax(a.top(), a.bottom() - d->height() - 40)));
            }
            d->move(pos);
        }
        d->show();
        // The Ricasso starter launches the client with "hide window"; Windows then applies
        // that to the first ShowWindow call of the process and the window stays hidden.
        if (!IsWindowVisible(h)) ShowWindow(h, quiet() ? SW_SHOWNOACTIVATE : SW_SHOWNORMAL);
    }
    if (!quiet()) {
        d->raise();
        d->activateWindow();
    }
}

void toggle(Id id) {
    if (s_windows[id] && s_windows[id]->isVisible()) s_windows[id]->hide();
    else show(id);
}

bool visible(Id id) {
    return s_windows[id] && s_windows[id]->isVisible();
}

void tick() {
    if (!s_app) return;
    // Let queued Qt events (paint, timers) run; Qt does not pump messages otherwise.
    QCoreApplication::sendPostedEvents();
    // Frames may run inside a Windows modal loop, so no processEvents() here.
    if (s_refresh_clock.isValid() && s_refresh_clock.elapsed() < 100) return;
    s_refresh_clock.restart();
    for (ToolDialog* d : s_windows) {
        if (d && d->isVisible()) {
            for (auto& f : d->refresh) f();
        }
    }
}

void shutdown() {
    for (ToolDialog*& d : s_windows) {
        if (d) {
            d->hide();
            delete d;
        }
        d = nullptr;
    }
}

// ---- windows ----

ToolDialog* build_graphics() {
    auto* d = new ToolDialog(QStringLiteral("Graphics"));
    auto* root = new QVBoxLayout(d);
    auto* tabs = new QTabWidget;
    root->addWidget(tabs);
    Refresh& r = d->refresh;

    // Quality
    QFormLayout* q = tab_page(tabs, QStringLiteral("Quality"));
    add_combo(q, r, QStringLiteral("Quality tier"),
              {QStringLiteral("Low"), QStringLiteral("Medium"), QStringLiteral("High"),
               QStringLiteral("SW+"), QStringLiteral("SW+ High")},
              [] { return static_cast<int>(g_postfx.graphics_tier); },
              [](int i) {
                  graphics_apply_tier(g_postfx, static_cast<GraphicsTier>(i));
                  postfx_apply_filter(g_postfx, g_postfx.postfx_filter, g_postfx.postfx_filter_intensity);
              });
    add_combo(q, r, QStringLiteral("Style preset"),
              {QStringLiteral("Off"), QStringLiteral("SW+ Medium"), QStringLiteral("SW+ High (PBR)"),
               QStringLiteral("Atmospheric"), QStringLiteral("Ethereal"), QStringLiteral("Cinematic"),
               QStringLiteral("Retro"), QStringLiteral("Fantasy"), QStringLiteral("Noir"), QStringLiteral("Custom")},
              [] { return static_cast<int>(g_postfx_preset); },
              [](int i) {
                  g_postfx_preset = static_cast<PostFXPreset>(i);
                  postfx_apply_preset(g_postfx, g_postfx_preset);
              });
    add_combo(q, r, QStringLiteral("Lighting model"),
              {QStringLiteral("Off (original fixed-function)"), QStringLiteral("Per-vertex (original look)"),
               QStringLiteral("Per-fragment with normals"), QStringLiteral("Parallax occlusion mapping"),
               QStringLiteral("Full PBR (heaviest)")},
              [] {
                  const int l = g_postfx.ffp_lighting_level >= 0 ? g_postfx.ffp_lighting_level : ffp::quality();
                  return std::clamp(l, 0, 4);
              },
              [](int i) {
                  g_postfx.ffp_lighting_level = i;
                  ffp::set_quality(i);
                  postfx_save_lighting_level(i);
              });
    add_combo(q, r, QStringLiteral("Screen filter"),
              {QStringLiteral("None"), QStringLiteral("Retro TV (CRT)"), QStringLiteral("Comic outline (mild)"),
               QStringLiteral("Anime outline (heavy)"), QStringLiteral("VHS glitch"),
               QStringLiteral("Vignette and film grain")},
              [] { return static_cast<int>(g_postfx.postfx_filter); },
              [](int i) { postfx_apply_filter(g_postfx, static_cast<PostEffectFilter>(i), g_postfx.postfx_filter_intensity); });
    add_slider(q, r, QStringLiteral("Filter intensity"), &g_postfx.postfx_filter_intensity, 0.0f, 2.0f, 2, {},
               [] { postfx_apply_filter(g_postfx, g_postfx.postfx_filter, g_postfx.postfx_filter_intensity); });

    // Display
    QFormLayout* disp = tab_page(tabs, QStringLiteral("Display"));
    add_combo(disp, r, QStringLiteral("Upscale filter"),
              {QStringLiteral("Sharp bilinear"), QStringLiteral("Nearest neighbor"),
               QStringLiteral("CRT scanline"), QStringLiteral("FSR 1.0")},
              [] { return static_cast<int>(g_fbo_mode); },
              [](int i) { g_fbo_mode = static_cast<FBOScale>(i); });
    add_check(disp, r, QStringLiteral("VSync"),
              [] {
                  int iv = 0;
                  SDL_GL_GetSwapInterval(&iv);
                  return iv != 0;
              },
              [](bool on) { SDL_GL_SetSwapInterval(on ? 1 : 0); });
    {
        s_modes = nullptr;
        s_mode_count = 0;
        if (s_sdl_window) s_modes = SDL_GetFullscreenDisplayModes(SDL_GetDisplayForWindow(s_sdl_window), &s_mode_count);
        QStringList items;
        items << QStringLiteral("Desktop resolution");
        for (int i = 0; i < s_mode_count; ++i) {
            const SDL_DisplayMode* m = s_modes[i];
            items << QStringLiteral("%1 x %2 @ %3 Hz").arg(m->w).arg(m->h).arg(qRound(m->refresh_rate));
        }
        add_combo(disp, r, QStringLiteral("Fullscreen mode"), items,
                  [] { return s_fullscreen_mode; },
                  [](int i) {
                      s_fullscreen_mode = i;
                      if (s_sdl_window)
                          SDL_SetWindowFullscreenMode(s_sdl_window, i == 0 || !s_modes ? nullptr : s_modes[i - 1]);
                  });
    }
    auto* info = new QLabel;
    disp->addRow(info);
    r.push_back([info] {
        if (!s_sdl_window) return;
        const SDL_DisplayMode* m = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(s_sdl_window));
        if (!m) return;
        const QString t = QStringLiteral("Display: %1 x %2 @ %3 Hz    Render: %4 x %5")
                              .arg(m->w).arg(m->h).arg(qRound(m->refresh_rate)).arg(GAME_W).arg(GAME_H);
        if (info->text() != t) info->setText(t);
    });
    auto* note = new QLabel(QStringLiteral("Window size and render resolution are in the View menu."));
    note->setWordWrap(true);
    disp->addRow(note);

    // Lighting
    QFormLayout* light = tab_page(tabs, QStringLiteral("Lighting"));
    add_check(light, r, QStringLiteral("PBR shading"), &g_postfx.pbr_enabled);
    add_check(light, r, QStringLiteral("Procedural surface detail"), &g_postfx.procedural_detail);
    add_slider(light, r, QStringLiteral("Strength"), &g_postfx.procedural_strength, 0.0f, 2.0f, 2,
               [] { return g_postfx.procedural_detail; });
    add_check(light, r, QStringLiteral("Water reflections"), &g_postfx.reflections_enabled);
    add_slider(light, r, QStringLiteral("Strength"), &g_postfx.reflection_intensity, 0.0f, 1.0f, 2,
               [] { return g_postfx.reflections_enabled; });
    add_check(light, r, QStringLiteral("Contact shadows"), &g_postfx.shadows);
    add_slider(light, r, QStringLiteral("Darkness"), &g_postfx.shadow_intensity, 0.0f, 1.0f, 2,
               [] { return g_postfx.shadows; });
    add_slider(light, r, QStringLiteral("Softness"), &g_postfx.shadow_softness, 0.0f, 0.02f, 4,
               [] { return g_postfx.shadows; });
    add_check(light, r, QStringLiteral("Ambient occlusion"), &g_postfx.ssao);
    add_slider(light, r, QStringLiteral("Intensity"), &g_postfx.ssao_intensity, 0.0f, 3.0f, 2,
               [] { return g_postfx.ssao; });
    add_slider(light, r, QStringLiteral("Radius"), &g_postfx.ssao_radius, 0.005f, 0.1f, 3,
               [] { return g_postfx.ssao; });
    add_check(light, r, QStringLiteral("God rays"), &g_postfx.god_rays);
    add_slider(light, r, QStringLiteral("Intensity"), &g_postfx.god_rays_intensity, 0.0f, 1.5f, 2,
               [] { return g_postfx.god_rays; });
    add_slider(light, r, QStringLiteral("Decay"), &g_postfx.god_rays_decay, 0.8f, 1.0f, 3,
               [] { return g_postfx.god_rays; });
    add_check(light, r, QStringLiteral("Volumetric light"), &g_postfx.volumetric_light);
    add_slider(light, r, QStringLiteral("Intensity"), &g_postfx.volumetric_intensity, 0.0f, 1.0f, 2,
               [] { return g_postfx.volumetric_light; });

    // Bloom
    QFormLayout* bloom = tab_page(tabs, QStringLiteral("Bloom"));
    add_check(bloom, r, QStringLiteral("Bloom"), &g_postfx.bloom);
    add_slider(bloom, r, QStringLiteral("Intensity"), &g_postfx.bloom_intensity, 0.0f, 1.5f, 2,
               [] { return g_postfx.bloom; });
    add_slider(bloom, r, QStringLiteral("Threshold"), &g_postfx.bloom_threshold, 0.0f, 1.0f, 2,
               [] { return g_postfx.bloom && !g_postfx.bloom_emitter_key; });
    add_check(bloom, r, QStringLiteral("Keep the HUD out of the light pass"), &g_postfx.ui_exempt,
              [] { return g_postfx.bloom; })
        ->setToolTip(QStringLiteral("HUD, text and buttons are not bloomed or relit."));
    add_check(bloom, r, QStringLiteral("Bloom only the game's own light sources"), &g_postfx.bloom_emitter_key,
              [] { return g_postfx.bloom; })
        ->setToolTip(QStringLiteral("Torches, lanterns and lava glow; bright flat surfaces such as sky and ice do not."));
    add_slider(bloom, r, QStringLiteral("Emitter level floor"), &g_postfx.bloom_emitter_luma, 0.0f, 0.6f, 2,
               [] { return g_postfx.bloom && g_postfx.bloom_emitter_key; });
    add_slider(bloom, r, QStringLiteral("Emitter saturation"), &g_postfx.bloom_emitter_sat, 0.0f, 1.5f, 2,
               [] { return g_postfx.bloom && g_postfx.bloom_emitter_key; });
    add_slider(bloom, r, QStringLiteral("Emitter local contrast"), &g_postfx.bloom_emitter_contrast, 1.0f, 2.0f, 2,
               [] { return g_postfx.bloom && g_postfx.bloom_emitter_key; });
    add_slider(bloom, r, QStringLiteral("Emitter gain"), &g_postfx.bloom_emitter_gain, 0.0f, 3.0f, 2,
               [] { return g_postfx.bloom && g_postfx.bloom_emitter_key; });

    // Color && Lens
    QFormLayout* color = tab_page(tabs, QStringLiteral("Color && Lens"));
    add_check(color, r, QStringLiteral("Color adjustment"), &g_postfx.color_adjust);
    add_slider(color, r, QStringLiteral("Saturation"), &g_postfx.saturation, 0.0f, 3.0f, 2,
               [] { return g_postfx.color_adjust; });
    add_slider(color, r, QStringLiteral("Contrast"), &g_postfx.contrast, 0.0f, 3.0f, 2,
               [] { return g_postfx.color_adjust; });
    add_slider(color, r, QStringLiteral("Brightness"), &g_postfx.brightness, -0.5f, 0.5f, 2,
               [] { return g_postfx.color_adjust; });
    add_slider(color, r, QStringLiteral("Warmth"), &g_postfx.warmth, -1.0f, 1.0f, 2,
               [] { return g_postfx.color_adjust; });
    add_check(color, r, QStringLiteral("Vignette"), &g_postfx.vignette);
    add_slider(color, r, QStringLiteral("Strength"), &g_postfx.vignette_intensity, 0.0f, 1.0f, 2,
               [] { return g_postfx.vignette; });
    add_check(color, r, QStringLiteral("Film grain"), &g_postfx.film_grain);
    add_slider(color, r, QStringLiteral("Strength"), &g_postfx.grain_intensity, 0.0f, 0.5f, 3,
               [] { return g_postfx.film_grain; });
    add_check(color, r, QStringLiteral("Chromatic aberration"), &g_postfx.chromatic_aberration);
    add_slider(color, r, QStringLiteral("Offset"), &g_postfx.ca_offset, 0.0f, 0.02f, 4,
               [] { return g_postfx.chromatic_aberration; });
    add_check(color, r, QStringLiteral("Sharpen"), &g_postfx.sharpen);
    add_slider(color, r, QStringLiteral("Strength"), &g_postfx.sharpen_strength, 0.0f, 2.0f, 2,
               [] { return g_postfx.sharpen; });
    add_check(color, r, QStringLiteral("Outlines"), &g_postfx.outlines);
    add_slider(color, r, QStringLiteral("Thickness"), &g_postfx.outline_thickness, 0.5f, 4.0f, 1,
               [] { return g_postfx.outlines; });
    add_slider(color, r, QStringLiteral("Opacity"), &g_postfx.outline_intensity, 0.0f, 1.0f, 2,
               [] { return g_postfx.outlines; });

    root->addWidget(button_box(d, QStringLiteral("Reload Config File"),
                               [] { post_gui_action(GUI_RELOAD_POSTFX); }));
    d->layout()->setSizeConstraint(QLayout::SetFixedSize);
    run_refresh(d);
    return d;
}

ToolDialog* build_cheats() {
    auto* d = new ToolDialog(QStringLiteral("Cheats"));
    Refresh& r = d->refresh;
    const bool ok = mod_cheats_available();
    auto* root = new QVBoxLayout(d);
    if (!ok) {
        root->addWidget(text_label(QStringLiteral("Player and physics cheats are not available for this game version.")));
    }

    auto* player = new QGroupBox(QStringLiteral("Player"));
    auto* pform = new QFormLayout(player);
    player->setEnabled(ok);
    add_check(pform, r, QStringLiteral("Infinite health"), &g_gui.mod_god_mode);
    add_check(pform, r, QStringLiteral("Infinite mana"), &g_gui.mod_infinite_mana);
    add_check(pform, r, QStringLiteral("Infinite jump"), &g_gui.mod_infinite_jump);
    root->addWidget(player);

    auto* stats = new QGroupBox(QStringLiteral("Stats"));
    auto* grid = new QGridLayout(stats);
    auto stat_button = [grid](int row, int col, const QString& text, GuiAction a) {
        auto* b = new QPushButton(text);
        grid->addWidget(b, row, col);
        QObject::connect(b, &QPushButton::clicked, [a] { post_gui_action(a); });
    };
    stat_button(0, 0, QStringLiteral("Heal"), GUI_MOD_HEAL_FULL);
    stat_button(0, 1, QStringLiteral("Refill Mana"), GUI_MOD_REFILL_MANA);
    stat_button(0, 2, QStringLiteral("+100 Coins"), GUI_MOD_ADD_COINS);
    stat_button(1, 0, QStringLiteral("Level +1"), GUI_MOD_LEVEL_UP);
    stat_button(1, 1, QStringLiteral("Level -1"), GUI_MOD_LEVEL_DOWN);
    stat_button(2, 0, QStringLiteral("XP +500"), GUI_MOD_EXP_UP);
    stat_button(2, 1, QStringLiteral("XP -500"), GUI_MOD_EXP_DOWN);
    root->addWidget(stats);

    auto* physics = new QGroupBox(QStringLiteral("Physics"));
    auto* phform = new QFormLayout(physics);
    physics->setEnabled(ok);
    add_slider(phform, r, QStringLiteral("Walk speed"), &g_gui.mod_walk_speed, 0.5f, 10.0f, 1);
    add_slider(phform, r, QStringLiteral("Run speed"), &g_gui.mod_run_speed, 0.5f, 10.0f, 1);
    add_slider(phform, r, QStringLiteral("Jump height"), &g_gui.mod_jump_height, 0.5f, 10.0f, 1);
    auto* reset = new QPushButton(QStringLiteral("Reset"));
    phform->addRow(reset);
    QObject::connect(reset, &QPushButton::clicked, [] {
        g_gui.mod_walk_speed = 1.0f;
        g_gui.mod_run_speed = 1.0f;
        g_gui.mod_jump_height = 1.0f;
    });
    root->addWidget(physics);

    root->addWidget(button_box(d, QString(), nullptr));
    d->layout()->setSizeConstraint(QLayout::SetFixedSize);
    run_refresh(d);
    return d;
}

ToolDialog* build_hotkeys() {
    auto* d = new ToolDialog(QStringLiteral("Hotkeys"));
    auto* root = new QVBoxLayout(d);
    auto* table = new QTableWidget(0, 2);
    table->setHorizontalHeaderLabels({QStringLiteral("Shortcut"), QStringLiteral("Function")});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setShowGrid(false);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setStretchLastSection(true);
    table->setWordWrap(false);
    table->verticalHeader()->setDefaultSectionSize(table->fontMetrics().height() + 8);
    int n = 0;
    for (const Hotkey& h : hotkey_table()) {
        if (!h.label) continue;
        table->insertRow(n);
        table->setItem(n, 0, new QTableWidgetItem(QString::fromUtf8(h.label)));
        table->setItem(n, 1, new QTableWidgetItem(QString::fromUtf8(h.description)));
        ++n;
    }
    table->resizeColumnToContents(0);
    root->addWidget(table);
    root->addWidget(button_box(d, QString(), nullptr));
    d->resize(440, 520);
    return d;
}

ToolDialog* build_about() {
    auto* d = new ToolDialog(QStringLiteral("About Ricasso"));
    auto* root = new QVBoxLayout(d);

    auto* title = text_label(QStringLiteral("Ricasso"));
    QFont tf = title->font();
    tf.setBold(true);
    tf.setPointSize(tf.pointSize() + 5);
    title->setFont(tf);
    root->addWidget(title);
    root->addWidget(text_label(QStringLiteral("An unofficial Windows compatibility layer for your own copy of Swordigo.")));

    auto* link = text_label(QStringLiteral("<a href=\"https://github.com/reallinusweiss/ricasso\">github.com/reallinusweiss/ricasso</a>"));
    link->setOpenExternalLinks(true);
    root->addWidget(link);
    root->addWidget(text_label(QStringLiteral("Built on SwordigoDesktop by TheAevoraLabs: TheMegineBraine, TheCorrectSynovian, MrSinup, X Dukinja and ETPV.")));
    root->addWidget(text_label(QStringLiteral("Swordigo is (c) Touch Foo. Ricasso contains no game files. Free software under the GNU GPL version 3.")));

    // A long path does not wrap well in a label; a read-only field scrolls and can be copied.
    root->addWidget(text_label(QStringLiteral("Save folder:")));
    auto* save = new QLineEdit(QDir::toNativeSeparators(QString::fromStdString(g_save_dir)));
    save->setReadOnly(true);
    root->addWidget(save);

    root->addWidget(button_box(d, QStringLiteral("Open Save Folder"), [] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(g_save_dir)));
    }));
    d->layout()->setSizeConstraint(QLayout::SetFixedSize);
    run_refresh(d);
    return d;
}

}  // namespace tool_windows
