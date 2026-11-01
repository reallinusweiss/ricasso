// ============================================================================
// launcher_theme.cpp — Theme styling implementation
// ============================================================================

#include "launcher/launcher_theme.h"
#include "platform/embedded_assets.h"
#include "platform/data_path.h"
#include <QPainter>
#include <QPainterPath>
#include <QFontDatabase>
#include <QFile>
#include <QFileInfo>

namespace swordfare::launcher {

QString LauncherTheme::s_fa_family;
bool LauncherTheme::s_fonts_loaded = false;

void LauncherTheme::init_fonts() {
    if (s_fonts_loaded) return;
    s_fonts_loaded = true;

    int font_id = -1;
    const unsigned char* data = nullptr;
    size_t size = 0;

    // 1. Try embedded asset
    if (embedded_asset("fonts/fa-solid-900.ttf", &data, &size) && data && size > 0) {
        font_id = QFontDatabase::addApplicationFontFromData(
            QByteArray::fromRawData(reinterpret_cast<const char*>(data), static_cast<int>(size)));
    }

    // 2. Try filesystem paths
    if (font_id == -1) {
        font_id = QFontDatabase::addApplicationFont("src/assets/fonts/fa-solid-900.ttf");
    }
    if (font_id == -1) {
        font_id = QFontDatabase::addApplicationFont("src/assets/fontawesome/otfs/Font Awesome 7 Free-Solid-900.otf");
    }
    if (font_id == -1) {
        font_id = QFontDatabase::addApplicationFont("/usr/share/swordigo-desktop/fonts/fa-solid-900.ttf");
    }

    if (font_id != -1) {
        QStringList families = QFontDatabase::applicationFontFamilies(font_id);
        if (!families.isEmpty()) {
            s_fa_family = families.first();
        }
    }

    if (s_fa_family.isEmpty()) {
        s_fa_family = "Font Awesome 6 Free";
    }
}

QFont LauncherTheme::fa_font(int pixel_size) {
    init_fonts();
    QFont font(s_fa_family);
    font.setPixelSize(pixel_size);
    return font;
}

QPixmap LauncherTheme::fa_pixmap(const char* utf8_glyph, const QColor& color, int size) {
    init_fonts();
    const int scale = 2; // high-dpi crisp rendering
    const int px = size * scale;
    QPixmap pix(px, px);
    pix.fill(Qt::transparent);

    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setPen(color);

    QFont f(s_fa_family);
    f.setPixelSize(px - 4);
    painter.setFont(f);

    painter.drawText(QRect(0, 0, px, px), Qt::AlignCenter, QString::fromUtf8(utf8_glyph));
    painter.end();

    pix.setDevicePixelRatio(scale);
    return pix;
}

QIcon LauncherTheme::fa_icon(const char* utf8_glyph, const QColor& color, int size) {
    QPixmap normal = fa_pixmap(utf8_glyph, color, size);
    QPixmap disabled = fa_pixmap(utf8_glyph, QColor(color.red(), color.green(), color.blue(), 90), size);
    QPixmap active = fa_pixmap(utf8_glyph, QColor(233, 69, 96), size);

    QIcon icon;
    icon.addPixmap(normal, QIcon::Normal, QIcon::Off);
    icon.addPixmap(active, QIcon::Normal, QIcon::On);
    icon.addPixmap(active, QIcon::Active, QIcon::Off);
    icon.addPixmap(active, QIcon::Active, QIcon::On);
    icon.addPixmap(disabled, QIcon::Disabled, QIcon::Off);
    return icon;
}

QPixmap LauncherTheme::app_logo(int size) {
    static QPixmap s_cached_logo;
    if (!s_cached_logo.isNull()) {
        return s_cached_logo.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    const unsigned char* data = nullptr;
    size_t sz = 0;
    if (embedded_asset("icon_app.png", &data, &sz) && data && sz > 0) {
        s_cached_logo.loadFromData(data, static_cast<uint>(sz));
    } else if (embedded_asset("launcer_icon.png", &data, &sz) && data && sz > 0) {
        s_cached_logo.loadFromData(data, static_cast<uint>(sz));
    }

    if (s_cached_logo.isNull()) {
        s_cached_logo.load("src/assets/icon_app.png");
    }
    if (s_cached_logo.isNull()) {
        s_cached_logo.load("src/assets/launcer_icon.png");
    }

    if (s_cached_logo.isNull()) {
        // Fallback procedural emblem
        s_cached_logo = QPixmap(size, size);
        s_cached_logo.fill(Qt::transparent);
        QPainter p(&s_cached_logo);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor(233, 69, 96));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(0, 0, size, size, 8, 8);
    }

    return s_cached_logo.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QPixmap LauncherTheme::game_icon(int size) {
    static QPixmap s_cached_game;
    if (!s_cached_game.isNull()) {
        return s_cached_game.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    const unsigned char* data = nullptr;
    size_t sz = 0;
    if (embedded_asset("icons/swordigo_default.png", &data, &sz) && data && sz > 0) {
        s_cached_game.loadFromData(data, static_cast<uint>(sz));
    }

    if (s_cached_game.isNull()) {
        QString user_data = QString::fromStdString(get_user_data_dir());
        while (user_data.endsWith('/') || user_data.endsWith('\\')) user_data.chop(1);
        for (const QString& p : {
            user_data + "/launcher/icons/swordigo_default.png",
            user_data + "/launcher/launcer_icon.png",
            QString::fromStdString(get_data_path("launcher/icons/swordigo_default.png")),
            QString::fromStdString(get_data_path("src/assets/icons/swordigo_default.png")),
            QString("src/assets/icons/swordigo_default.png")
        }) {
            if (!p.isEmpty() && QFile::exists(p)) {
                s_cached_game.load(p);
                if (!s_cached_game.isNull()) break;
            }
        }
    }
    if (s_cached_game.isNull()) {
        s_cached_game = app_logo(size);
    }

    return s_cached_game.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QPixmap LauncherTheme::rl_game_icon(int size) {
    static QPixmap s_cached_rl;
    if (!s_cached_rl.isNull()) {
        return s_cached_rl.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    const unsigned char* data = nullptr;
    size_t sz = 0;
    if (embedded_asset("icons/rl_swordigo_default.png", &data, &sz) && data && sz > 0) {
        s_cached_rl.loadFromData(data, static_cast<uint>(sz));
    }

    if (s_cached_rl.isNull()) {
        QString user_data = QString::fromStdString(get_user_data_dir());
        while (user_data.endsWith('/') || user_data.endsWith('\\')) user_data.chop(1);
        for (const QString& p : {
            user_data + "/launcher/icons/rl_swordigo_default.png",
            QString::fromStdString(get_data_path("launcher/icons/rl_swordigo_default.png")),
            QString::fromStdString(get_data_path("src/assets/icons/rl_swordigo_default.png")),
            QString("src/assets/icons/rl_swordigo_default.png")
        }) {
            if (!p.isEmpty() && QFile::exists(p)) {
                s_cached_rl.load(p);
                if (!s_cached_rl.isNull()) break;
            }
        }
    }
    if (s_cached_rl.isNull()) {
        s_cached_rl = game_icon(size);
    }

    return s_cached_rl.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QPixmap LauncherTheme::instance_icon(const QString& icon_path, const QString& assets_dir, int size,
                                     const QString& game_type, const QString& id) {
    QString user_data = QString::fromStdString(get_user_data_dir());
    while (user_data.endsWith('/') || user_data.endsWith('\\')) user_data.chop(1);

    auto try_path = [&](const QString& p) -> QPixmap {
        if (p.isEmpty()) return QPixmap();
        if (QFile::exists(p)) {
            QPixmap pm(p);
            if (!pm.isNull()) {
                return pm.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
        }
        return QPixmap();
    };

    // 1. Check explicit icon_path
    if (!icon_path.isEmpty()) {
        QPixmap pm = try_path(icon_path);
        if (!pm.isNull()) return pm;

        pm = try_path(user_data + "/" + icon_path);
        if (!pm.isNull()) return pm;

        pm = try_path(user_data + "/launcher/" + icon_path);
        if (!pm.isNull()) return pm;

        pm = try_path(user_data + "/launcher/icons/" + icon_path);
        if (!pm.isNull()) return pm;

        pm = try_path(user_data + "/icons/" + icon_path);
        if (!pm.isNull()) return pm;

        pm = try_path(user_data + "/instances/" + icon_path);
        if (!pm.isNull()) return pm;

        if (!assets_dir.isEmpty()) {
            pm = try_path(user_data + "/" + assets_dir + "/" + icon_path);
            if (!pm.isNull()) return pm;
            pm = try_path(user_data + "/" + assets_dir + "/resources/" + icon_path);
            if (!pm.isNull()) return pm;
            pm = try_path(user_data + "/" + assets_dir + "/assets/" + icon_path);
            if (!pm.isNull()) return pm;
        }

        std::string ip = icon_path.toStdString();
        pm = try_path(QString::fromStdString(get_data_path("launcher/" + ip)));
        if (!pm.isNull()) return pm;
        pm = try_path(QString::fromStdString(get_data_path("launcher/icons/" + ip)));
        if (!pm.isNull()) return pm;
        pm = try_path(QString::fromStdString(get_data_path("src/assets/icons/" + ip)));
        if (!pm.isNull()) return pm;
        pm = try_path(QString::fromStdString(get_data_path("src/assets/" + ip)));
        if (!pm.isNull()) return pm;
        pm = try_path(QString::fromStdString(get_data_path(ip)));
        if (!pm.isNull()) return pm;
    }

    // 2. Check heuristics by ID / name
    if (!id.isEmpty()) {
        QPixmap pm = try_path(user_data + "/launcher/" + id + ".png");
        if (!pm.isNull()) return pm;
        pm = try_path(user_data + "/launcher/" + id.toLower() + ".png");
        if (!pm.isNull()) return pm;
        pm = try_path(user_data + "/launcher/icons/" + id + ".png");
        if (!pm.isNull()) return pm;
        pm = try_path(user_data + "/launcher/icons/" + id.toLower() + ".png");
        if (!pm.isNull()) return pm;

        if (id.contains("combatch", Qt::CaseInsensitive)) {
            pm = try_path(user_data + "/launcher/combatch.png");
            if (!pm.isNull()) return pm;
        }
        if (id.contains("phonk", Qt::CaseInsensitive)) {
            pm = try_path(user_data + "/launcher/phonkdigo.png");
            if (!pm.isNull()) return pm;
        }
        if (id.contains("raijin", Qt::CaseInsensitive) || id.contains("as", Qt::CaseInsensitive)) {
            pm = try_path(user_data + "/launcher/as.png");
            if (!pm.isNull()) return pm;
        }
        if (id.contains("curse", Qt::CaseInsensitive) || id.compare("mc", Qt::CaseInsensitive) == 0) {
            pm = try_path(user_data + "/launcher/mc.png");
            if (!pm.isNull()) return pm;
        }
        if (id.contains("manson", Qt::CaseInsensitive) || id.contains("mini", Qt::CaseInsensitive)) {
            pm = try_path(user_data + "/launcher/icons/swmini_default.png");
            if (!pm.isNull()) return pm;
            pm = try_path(QString::fromStdString(get_data_path("src/assets/icons/swmini_default.png")));
            if (!pm.isNull()) return pm;
        }
    }

    // 3. Check inside assets_dir for standard icon files
    if (!assets_dir.isEmpty()) {
        for (const QString& name : {"icon.png", "icon.jpg", "instance.png", "logo.png", "banner.png"}) {
            QPixmap pm = try_path(user_data + "/" + assets_dir + "/" + name);
            if (!pm.isNull()) return pm;
            pm = try_path(user_data + "/" + assets_dir + "/resources/" + name);
            if (!pm.isNull()) return pm;
            pm = try_path(user_data + "/" + assets_dir + "/assets/" + name);
            if (!pm.isNull()) return pm;
        }
    }

    // 4. Default game icons by game_type
    if (game_type.compare("RLSwordigo", Qt::CaseInsensitive) == 0 ||
        id.contains("rl-", Qt::CaseInsensitive) || id.contains("rl", Qt::CaseInsensitive)) {
        return rl_game_icon(size);
    }

    return game_icon(size);
}

QPixmap LauncherTheme::background_pixmap() {
    static QPixmap s_cached_bg;
    if (!s_cached_bg.isNull()) return s_cached_bg;

    const unsigned char* data = nullptr;
    size_t sz = 0;
    if (embedded_asset("launcher_bg.png", &data, &sz) && data && sz > 0) {
        s_cached_bg.loadFromData(data, static_cast<uint>(sz));
    }
    if (s_cached_bg.isNull()) {
        s_cached_bg.load("src/assets/launcher_bg.png");
    }
    return s_cached_bg;
}

QString LauncherTheme::stylesheet() {
    return QString::fromUtf8(R"QSS(
        /* Global Window & Fonts */
        QWidget {
            background-color: #0D1117;
            color: #F0F6FC;
            font-family: "Segoe UI", "Inter", "Ubuntu", "Cantarell", sans-serif;
            font-size: 13px;
            selection-background-color: #E94560;
            selection-color: #FFFFFF;
        }

        /* Sidebar Container */
        #SidebarContainer {
            background-color: #121720;
            border-right: 1px solid #232A36;
        }

        /* Sidebar Navigation Buttons */
        QPushButton.NavBtn {
            background-color: transparent;
            color: #8B949E;
            text-align: left;
            padding: 10px 16px;
            border: none;
            border-radius: 8px;
            font-size: 14px;
            font-weight: 600;
        }
        QPushButton.NavBtn:hover {
            background-color: #1A2230;
            color: #F0F6FC;
        }
        QPushButton.NavBtn:checked, QPushButton.NavBtn.active {
            background-color: #212938;
            color: #FFFFFF;
            border-left: 3px solid #E94560;
            padding-left: 13px;
        }

        /* Cards and Group Panels */
        QFrame.CardFrame {
            background-color: #171D27;
            border: 1px solid #283141;
            border-radius: 12px;
        }
        QFrame.CardFrame:hover {
            border-color: #3B475D;
        }

        QFrame.InstanceCard {
            background-color: #171D27;
            border: 1px solid #283141;
            border-radius: 10px;
        }
        QFrame.InstanceCard:hover {
            background-color: #1C2432;
            border-color: #E94560;
        }
        QFrame.InstanceCard.selected {
            background-color: #202736;
            border: 2px solid #E94560;
        }

        /* Standard Buttons */
        QPushButton {
            background-color: #21262D;
            color: #F0F6FC;
            border: 1px solid #30363D;
            border-radius: 6px;
            padding: 6px 14px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: #30363D;
            border-color: #8B949E;
        }
        QPushButton:pressed {
            background-color: #161B22;
        }

        /* Hero Launch Button */
        QPushButton#LaunchBtn {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #E94560, stop:1 #FF5B79);
            color: #FFFFFF;
            border: none;
            border-radius: 10px;
            padding: 12px 36px;
            font-size: 17px;
            font-weight: bold;
            letter-spacing: 1px;
        }
        QPushButton#LaunchBtn:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #FF5778, stop:1 #FF7A95);
        }
        QPushButton#LaunchBtn:pressed {
            background: #D63447;
        }

        /* Secondary Action Buttons */
        QPushButton.SecondaryBtn {
            background-color: #1F2734;
            color: #F0F6FC;
            border: 1px solid #2D3748;
            border-radius: 6px;
            padding: 5px 12px;
        }
        QPushButton.SecondaryBtn:hover {
            background-color: #293447;
            border-color: #4A5568;
        }

        /* Danger Button */
        QPushButton.DangerBtn {
            background-color: #2A1719;
            color: #FF6B6B;
            border: 1px solid #4D2125;
            border-radius: 6px;
            padding: 5px 12px;
        }
        QPushButton.DangerBtn:hover {
            background-color: #3D1F23;
            border-color: #FF6B6B;
        }

        /* Input Fields */
        QLineEdit, QTextEdit {
            background-color: #12161E;
            color: #F0F6FC;
            border: 1px solid #283141;
            border-radius: 6px;
            padding: 6px 10px;
        }
        QLineEdit:focus, QTextEdit:focus {
            border: 1px solid #E94560;
            background-color: #151A24;
        }

        /* Combo Boxes */
        QComboBox {
            background-color: #1B212D;
            color: #F0F6FC;
            border: 1px solid #2D3748;
            border-radius: 6px;
            padding: 6px 12px;
            min-width: 120px;
        }
        QComboBox:hover {
            border-color: #4A5568;
        }
        QComboBox::drop-down {
            border: none;
            width: 24px;
        }
        QComboBox QAbstractItemView {
            background-color: #171D27;
            color: #F0F6FC;
            border: 1px solid #2D3748;
            selection-background-color: #E94560;
            selection-color: #FFFFFF;
            outline: none;
        }

        /* Sliders */
        QSlider::groove:horizontal {
            height: 6px;
            background: #212836;
            border-radius: 3px;
        }
        QSlider::sub-page:horizontal {
            background: #E94560;
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            background: #FFFFFF;
            border: 2px solid #E94560;
            width: 14px;
            margin-top: -4px;
            margin-bottom: -4px;
            border-radius: 7px;
        }
        QSlider::handle:horizontal:hover {
            background: #FFEDF0;
            transform: scale(1.2);
        }

        /* Checkboxes */
        QCheckBox {
            color: #F0F6FC;
            spacing: 8px;
            font-weight: 500;
        }
        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border: 1px solid #3B475D;
            border-radius: 4px;
            background: #171D27;
        }
        QCheckBox::indicator:hover {
            border-color: #E94560;
        }
        QCheckBox::indicator:checked {
            background-color: #E94560;
            border-color: #E94560;
            image: none;
        }

        /* Scrollbars */
        QScrollBar:vertical {
            background: #0D1117;
            width: 8px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #283141;
            min-height: 25px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover {
            background: #3B475D;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: none;
            height: 0px;
        }

        /* List & Table Views */
        QListWidget, QTableWidget {
            background-color: #121720;
            border: 1px solid #232A36;
            border-radius: 8px;
            outline: none;
            padding: 4px;
        }
        QListWidget::item {
            padding: 8px;
            border-radius: 6px;
            margin-bottom: 3px;
        }
        QListWidget::item:hover {
            background-color: #1A2230;
        }
        QListWidget::item:selected {
            background-color: #242D3E;
            color: #FFFFFF;
            border-left: 2px solid #E94560;
        }

        /* Progress Bar */
        QProgressBar {
            background-color: #171D27;
            border: 1px solid #283141;
            border-radius: 5px;
            text-align: center;
            color: #F0F6FC;
            font-size: 11px;
            font-weight: bold;
        }
        QProgressBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #E94560, stop:1 #FF8C42);
            border-radius: 4px;
        }

        /* Badges & Pills */
        QLabel.Badge {
            background-color: #212938;
            color: #8B949E;
            border: 1px solid #303B4E;
            border-radius: 4px;
            padding: 2px 6px;
            font-size: 11px;
            font-weight: bold;
        }
        QLabel.BadgeActive {
            background-color: rgba(233, 69, 96, 0.18);
            color: #FF6B86;
            border: 1px solid rgba(233, 69, 96, 0.4);
            border-radius: 4px;
            padding: 2px 6px;
            font-size: 11px;
            font-weight: bold;
        }
        QLabel.BadgeSuccess {
            background-color: rgba(46, 160, 67, 0.18);
            color: #3FB950;
            border: 1px solid rgba(46, 160, 67, 0.4);
            border-radius: 4px;
            padding: 2px 6px;
            font-size: 11px;
            font-weight: bold;
        }
    )QSS");
}

QPixmap LauncherTheme::make_circular_avatar(const QPixmap& src, int diameter,
                                           const QColor& border_color, int border_width) {
    if (src.isNull()) {
        return make_preset_avatar("Hero", diameter, QColor(233, 69, 96));
    }

    QPixmap result(diameter, diameter);
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const int pad = border_width;
    const int inner_diam = diameter - 2 * pad;

    // Draw circular image
    QPainterPath clip_path;
    clip_path.addEllipse(pad, pad, inner_diam, inner_diam);
    painter.setClipPath(clip_path);

    QPixmap scaled = src.scaled(inner_diam, inner_diam, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    int x_off = pad + (inner_diam - scaled.width()) / 2;
    int y_off = pad + (inner_diam - scaled.height()) / 2;
    painter.drawPixmap(x_off, y_off, scaled);

    // Reset clip and draw glowing border ring
    painter.setClipping(false);
    if (border_width > 0) {
        QPen pen(border_color, static_cast<qreal>(border_width));
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        qreal half_pen = border_width * 0.5;
        painter.drawEllipse(QRectF(half_pen, half_pen, diameter - border_width, diameter - border_width));
    }

    return result;
}

QPixmap LauncherTheme::make_preset_avatar(const QString& name, int diameter, const QColor& bg_col) {
    QPixmap pix(diameter, diameter);
    pix.fill(Qt::transparent);

    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Background circle gradient
    QRadialGradient grad(diameter * 0.5, diameter * 0.5, diameter * 0.5);
    grad.setColorAt(0.0, bg_col.lighter(130));
    grad.setColorAt(1.0, bg_col.darker(150));

    painter.setBrush(grad);
    painter.setPen(QPen(QColor(233, 69, 96), 2.5));
    painter.drawEllipse(2, 2, diameter - 4, diameter - 4);

    // Monogram letter
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setPixelSize(diameter / 2);
    font.setBold(true);
    painter.setFont(font);

    QString letter = name.isEmpty() ? "H" : QString(name.at(0).toUpper());
    painter.drawText(QRect(0, 0, diameter, diameter), Qt::AlignCenter, letter);

    return pix;
}

} // namespace swordfare::launcher
