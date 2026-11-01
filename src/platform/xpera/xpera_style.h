// =============================================================================
// xpera_style.h — Xpera ImGuiStyle driver
//
// Turns the XperaDark palette into a complete Dear ImGui style (colours +
// metrics). This is the single place that touches ImGuiStyle, so every Xpera
// surface inherits an identical look. Not wired into the app yet.
// =============================================================================
#pragma once

#include "imgui/imgui.h"
#include "platform/xpera/xpera_theme.h"

namespace xpera {

struct StyleConfig {
    // Global UI scale (dpi / distance). 1.0 == 100%.
    float ui_scale = 1.0f;
    // Corner radius applied to controls and frames.
    float rounding = metrics::control_round;
    // Tighter paddings for dense tool windows.
    bool  compact  = false;
    // Force visible focus rings (accessibility / high-contrast displays).
    bool  high_contrast = false;
};

// ---- Runtime toggle ---------------------------------------------------------
// Xpera is the default look, but the legacy Swordfare theme is retained. Set
// the environment variable SWORDFARE_XPERA_UI=0 (or call set_ui_enabled(false))
// to fall back. Read once, lazily.
bool ui_enabled();
void set_ui_enabled(bool enabled);

// Applies XperaDark to the currently active ImGui context's style.
void apply_style(const StyleConfig& cfg = StyleConfig{});

// Applies XperaDark to an explicit style object (unit-testable).
void apply_style(ImGuiStyle& style, const StyleConfig& cfg);

} // namespace xpera
