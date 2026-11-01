#pragma once
// ============================================================================
// launcher_theme.h — Sleek Modern Gaming Aesthetic for Swordfare Launcher
// ============================================================================

#include <QString>
#include <QColor>
#include <QPixmap>
#include <QIcon>
#include <QFont>
#include "platform/IconsFontAwesome6.h"

namespace swordfare::launcher {

class LauncherTheme {
public:
    // Palette constants (1:1 with ImGui edition)
    static constexpr const char* COLOR_BG_OBSIDIAN    = "#0A0D14";
    static constexpr const char* COLOR_BG_SIDEBAR     = "#0E1420";
    static constexpr const char* COLOR_BG_CARD        = "#141A26";
    static constexpr const char* COLOR_BG_CARD_HOVER  = "#1B2232";
    static constexpr const char* COLOR_BORDER         = "#232D3F";
    static constexpr const char* COLOR_BORDER_FOCUSED = "#E94560";
    static constexpr const char* COLOR_ACCENT_CRIMSON = "#E94560";
    static constexpr const char* COLOR_ACCENT_HOVER   = "#FF5B79";
    static constexpr const char* COLOR_ACCENT_GLOW    = "rgba(233, 69, 96, 0.45)";
    static constexpr const char* COLOR_TEXT_PRIMARY   = "#E6EDF3";
    static constexpr const char* COLOR_TEXT_MUTED     = "#8B949E";
    static constexpr const char* COLOR_SUCCESS        = "#3DB84F";
    static constexpr const char* COLOR_WARNING        = "#D19A21";
    static constexpr const char* COLOR_GOLD           = "#F1C40F";

    // Initializes fonts (loads FontAwesome 6/7)
    static void init_fonts();

    // Returns a QFont initialized with the loaded FontAwesome font family
    static QFont fa_font(int pixel_size = 14);

    // Creates a QIcon rendered natively from a FontAwesome UTF-8 glyph
    static QIcon fa_icon(const char* utf8_glyph, const QColor& color = QColor(180, 195, 215), int size = 18);

    // Creates a QPixmap from a FontAwesome UTF-8 glyph
    static QPixmap fa_pixmap(const char* utf8_glyph, const QColor& color = QColor(180, 195, 215), int size = 18);

    // Returns the official Swordigo Desktop App Logo
    static QPixmap app_logo(int size = 48);

    // Returns the Swordigo game icon
    static QPixmap game_icon(int size = 48);

    // Returns the RLSwordigo game icon
    static QPixmap rl_game_icon(int size = 48);

    // Resolves and returns custom instance icon, falling back to game_icon / rl_game_icon
    static QPixmap instance_icon(const QString& icon_path, const QString& assets_dir, int size = 48,
                                 const QString& game_type = QString(), const QString& id = QString());

    // Returns the starry night background texture (faint watermark)
    static QPixmap background_pixmap();

    // Returns the complete global QSS stylesheet
    static QString stylesheet();

    // Helper to generate a crisp circular avatar with an accent border
    static QPixmap make_circular_avatar(const QPixmap& src, int diameter,
                                        const QColor& border_color = QColor(233, 69, 96),
                                        int border_width = 3);

    // Creates a placeholder avatar with stylized monogram
    static QPixmap make_preset_avatar(const QString& name, int diameter, const QColor& bg_col);

private:
    static QString s_fa_family;
    static bool s_fonts_loaded;
};

} // namespace swordfare::launcher
