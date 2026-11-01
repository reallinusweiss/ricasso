// =============================================================================
// xpera_style.cpp — Xpera ImGuiStyle implementation
// =============================================================================
#include "platform/xpera/xpera_style.h"

#include <cstdlib>

namespace xpera {

namespace {
bool g_xpera_ui           = true;
bool g_xpera_env_resolved = false;
} // namespace

bool ui_enabled() {
    if (!g_xpera_env_resolved) {
        g_xpera_env_resolved = true;
        if (const char* e = std::getenv("SWORDFARE_XPERA_UI")) {
            const char c = e[0];
            g_xpera_ui = !(c == '0' || c == 'n' || c == 'N' || c == 'f' || c == 'F');
        }
    }
    return g_xpera_ui;
}

void set_ui_enabled(bool enabled) {
    g_xpera_ui           = enabled;
    g_xpera_env_resolved = true;
}

void apply_style(const StyleConfig& cfg) {
    apply_style(ImGui::GetStyle(), cfg);
}

void apply_style(ImGuiStyle& s, const StyleConfig& cfg) {
    const Palette& p = palette;
    const float sc   = (cfg.ui_scale > 0.1f) ? cfg.ui_scale : 1.0f;
    const float rnd  = cfg.rounding;
    const float pad  = cfg.compact ? metrics::pad * 0.75f : metrics::pad;

    // ── Rounded corners: consistent, never bubbly ──────────────────────────
    s.WindowRounding    = 0.0f;          // windows are flush surfaces
    s.ChildRounding     = metrics::card_round;
    s.FrameRounding     = rnd;
    s.PopupRounding     = metrics::card_round;
    s.ScrollbarRounding = rnd;
    s.GrabRounding      = rnd;
    s.TabRounding       = 0.0f;          // tabs are flat + underlined

    // ── Borders: hairlines, never thick ────────────────────────────────────
    s.WindowBorderSize  = 1.0f;
    s.ChildBorderSize   = 1.0f;
    s.PopupBorderSize   = 1.0f;
    s.FrameBorderSize   = 0.0f;
    s.TabBarBorderSize  = 1.0f;

    // ── Spacing on one grid ────────────────────────────────────────────────
    s.WindowPadding     = ImVec2(pad, pad);
    s.FramePadding      = ImVec2(pad * 0.95f, pad * 0.5f);
    s.CellPadding       = ImVec2(pad * 0.6f, pad * 0.4f);
    s.ItemSpacing       = ImVec2(metrics::gap, cfg.compact ? 4.0f : metrics::gap * 0.75f);
    s.ItemInnerSpacing  = ImVec2(6.0f, 5.0f);
    s.IndentSpacing     = 18.0f;
    s.ScrollbarSize     = 11.0f;
    s.GrabMinSize       = 11.0f;

    s.WindowTitleAlign          = ImVec2(0.0f, 0.5f);
    s.ButtonTextAlign           = ImVec2(0.5f, 0.5f);
    s.SelectableTextAlign       = ImVec2(0.0f, 0.5f);
    s.SeparatorTextBorderSize   = 1.0f;
    s.SeparatorTextAlign        = ImVec2(0.0f, 0.5f);
    s.SeparatorTextPadding      = ImVec2(16.0f, 7.0f);
    s.DisabledAlpha             = 0.40f;

    // ── Scale the whole metric set once ────────────────────────────────────
    s.ScaleAllSizes(sc);

    ImVec4* c = s.Colors;
    auto set = [&](ImGuiCol idx, const ImVec4& col) { c[idx] = col; };

    // ── Text ───────────────────────────────────────────────────────────────
    set(ImGuiCol_Text,                  p.text_hi);
    set(ImGuiCol_TextDisabled,          p.text_lo);
    set(ImGuiCol_TextSelectedBg,        p.accent_soft);
    set(ImGuiCol_TextLink,              p.secondary);
    set(ImGuiCol_InputTextCursor,       p.accent);

    // ── Backgrounds ────────────────────────────────────────────────────────
    set(ImGuiCol_WindowBg,              p.base);
    set(ImGuiCol_ChildBg,               ImVec4(0, 0, 0, 0));
    set(ImGuiCol_PopupBg,               p.overlay);
    set(ImGuiCol_MenuBarBg,             p.void_bg);
    set(ImGuiCol_ModalWindowDimBg,      ImVec4(0.02f, 0.02f, 0.03f, 0.60f));

    // ── Borders ────────────────────────────────────────────────────────────
    set(ImGuiCol_Border,                p.border);
    set(ImGuiCol_BorderShadow,          ImVec4(0, 0, 0, 0));

    // ── Frames / inputs ────────────────────────────────────────────────────
    set(ImGuiCol_FrameBg,               p.input);
    set(ImGuiCol_FrameBgHovered,        p.elevated);
    set(ImGuiCol_FrameBgActive,         p.selected);

    // ── Title bars (used by section windows / popups) ──────────────────────
    set(ImGuiCol_TitleBg,               p.void_bg);
    set(ImGuiCol_TitleBgActive,         p.base);
    set(ImGuiCol_TitleBgCollapsed,      p.void_bg);

    // ── Scrollbars ─────────────────────────────────────────────────────────
    set(ImGuiCol_ScrollbarBg,           ImVec4(0, 0, 0, 0));
    set(ImGuiCol_ScrollbarGrab,         p.border);
    set(ImGuiCol_ScrollbarGrabHovered,  p.border_focus);
    set(ImGuiCol_ScrollbarGrabActive,   p.accent_dim);

    // ── Checkmarks / sliders ───────────────────────────────────────────────
    set(ImGuiCol_CheckMark,             p.accent);
    set(ImGuiCol_SliderGrab,            p.accent_dim);
    set(ImGuiCol_SliderGrabActive,      p.accent);

    // ── Buttons ────────────────────────────────────────────────────────────
    set(ImGuiCol_Button,                p.elevated);
    set(ImGuiCol_ButtonHovered,         p.selected);
    set(ImGuiCol_ButtonActive,          p.accent_dim);

    // ── Headers (selectables, collapsing, menu items) ──────────────────────
    set(ImGuiCol_Header,                p.selected);
    set(ImGuiCol_HeaderHovered,         p.elevated);
    set(ImGuiCol_HeaderActive,          p.accent_soft);

    // ── Separators / resize grips ──────────────────────────────────────────
    set(ImGuiCol_Separator,             p.border_soft);
    set(ImGuiCol_SeparatorHovered,      p.accent_line);
    set(ImGuiCol_SeparatorActive,       p.accent);
    set(ImGuiCol_ResizeGrip,            ImVec4(0, 0, 0, 0));
    set(ImGuiCol_ResizeGripHovered,     p.accent_line);
    set(ImGuiCol_ResizeGripActive,      p.accent_dim);

    // ── Tabs: flat with a accent underline, not pill slop ──────────────────
    set(ImGuiCol_Tab,                        ImVec4(0, 0, 0, 0));
    set(ImGuiCol_TabHovered,                 p.elevated);
    set(ImGuiCol_TabSelected,                ImVec4(0, 0, 0, 0));
    set(ImGuiCol_TabSelectedOverline,        p.accent);
    set(ImGuiCol_TabDimmed,                  ImVec4(0, 0, 0, 0));
    set(ImGuiCol_TabDimmedSelected,          ImVec4(0, 0, 0, 0));
    set(ImGuiCol_TabDimmedSelectedOverline,  p.border_focus);

    // ── Plots ──────────────────────────────────────────────────────────────
    set(ImGuiCol_PlotLines,             p.secondary);
    set(ImGuiCol_PlotLinesHovered,      p.accent_hover);
    set(ImGuiCol_PlotHistogram,         p.accent);
    set(ImGuiCol_PlotHistogramHovered,  p.accent_hover);

    // ── Tables ─────────────────────────────────────────────────────────────
    set(ImGuiCol_TableHeaderBg,         p.surface);
    set(ImGuiCol_TableBorderStrong,     p.border);
    set(ImGuiCol_TableBorderLight,      p.border_soft);
    set(ImGuiCol_TableRowBg,            ImVec4(0, 0, 0, 0));
    set(ImGuiCol_TableRowBgAlt,         ImVec4(1, 1, 1, 0.018f));

    // ── DnD / navigation ───────────────────────────────────────────────────
    set(ImGuiCol_DragDropTarget,        p.accent);
    set(ImGuiCol_NavCursor,             cfg.high_contrast ? p.accent : p.accent_line);
    set(ImGuiCol_NavWindowingHighlight, p.text_hi);
    set(ImGuiCol_NavWindowingDimBg,     ImVec4(0.02f, 0.02f, 0.03f, 0.55f));

    // ── Trees ──────────────────────────────────────────────────────────────
    set(ImGuiCol_TreeLines,             p.border);
}

} // namespace xpera
