// =============================================================================
// xpera_gui.cpp — Xpera widget toolkit implementation
// =============================================================================
#include "platform/xpera/xpera_gui.h"

#include <algorithm>

namespace xpera {

// ---------------------------------------------------------------------------
// Local helpers
// ---------------------------------------------------------------------------
namespace {

void framed_rect(ImDrawList* dl, ImVec2 a, ImVec2 b,
                 ImVec4 fill, ImVec4 border, float rounding,
                 float thickness = 1.0f) {
    dl->AddRectFilled(a, b, u32(fill), rounding);
    if (border.w > 0.0f)
        dl->AddRect(a, b, u32(border), rounding, 0, thickness);
}

// Draws the Xpera chrome at the top of a window and returns its total height.
float draw_chrome(const WindowSpec& spec, ImVec2 wp, ImVec2 ws) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float lh = ImGui::GetTextLineHeight();
    const float px = wp.x + metrics::pad * 1.5f;

    float y = wp.y + 9.0f;
    if (spec.eyebrow && spec.eyebrow[0]) {
        dl->AddText(ImVec2(px, y), u32(palette.accent), spec.eyebrow);
        y += lh * 0.92f;
    }
    if (spec.title && spec.title[0]) {
        dl->AddText(ImVec2(px, y), u32(palette.text_hi), spec.title);
        y += lh + 1.0f;
    }
    if (spec.subtitle && spec.subtitle[0]) {
        dl->AddText(ImVec2(px, y), u32(palette.text_lo), spec.subtitle);
        y += lh;
    }

    // Chrome height: default strip, extended for the subtitle line.
    float h = metrics::titlebar_h;
    if (y - wp.y + 9.0f > h) h = y - wp.y + 9.0f;

    // Backdrop + hairline bottom rule with an accent segment on the left.
    dl->AddRectFilled(wp, ImVec2(wp.x + ws.x, wp.y + h), u32(palette.void_bg));
    dl->AddLine(ImVec2(wp.x, wp.y + h - 1.0f), ImVec2(wp.x + ws.x, wp.y + h - 1.0f),
                u32(palette.border), 1.0f);
    dl->AddLine(ImVec2(wp.x, wp.y + h - 1.0f), ImVec2(wp.x + 132.0f, wp.y + h - 1.0f),
                u32(palette.accent), 2.0f);

    // Keyboard close hint (never a chunky OS close button).
    if (spec.close_hint && spec.close_hint[0]) {
        ImVec2 ts = ImGui::CalcTextSize(spec.close_hint);
        float bw = ts.x + 14.0f, bh = 20.0f;
        ImVec2 b1(wp.x + ws.x - metrics::pad * 1.5f, wp.y + h * 0.5f + bh * 0.5f);
        ImVec2 a1(b1.x - bw, b1.y - bh);
        framed_rect(dl, a1, b1, palette.elevated, palette.border, 5.0f);
        dl->AddText(ImVec2(a1.x + 7.0f, a1.y + bh * 0.5f - ts.y * 0.5f),
                    u32(palette.text_mid), spec.close_hint);
    }
    return h;
}

} // namespace

// ---------------------------------------------------------------------------
// Window chrome
// ---------------------------------------------------------------------------
bool begin_window(const char* id, const WindowSpec& spec,
                  bool* p_open, ImGuiWindowFlags extra_flags) {
    ImGuiIO& io = ImGui::GetIO();
    const float sw = io.DisplaySize.x;
    const float sh = io.DisplaySize.y;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(metrics::pad * 1.5f, metrics::pad * 1.2f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,
                        spec.mode == WindowMode::Panel ? metrics::card_round : 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, palette.base);
    ImGui::PushStyleColor(ImGuiCol_Border,  palette.border);

    if (spec.mode == WindowMode::Fullscreen) {
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(sw, sh), ImGuiCond_Always);
    } else {
        ImVec2 sz  = spec.panel_size;
        ImVec2 pos = spec.panel_pos;
        if (pos.x < 0.0f) pos.x = (sw - sz.x) * 0.5f;
        if (pos.y < 0.0f) pos.y = (sh - sz.y) * 0.5f;
        ImGui::SetNextWindowPos(pos,  ImGuiCond_Always);
        ImGui::SetNextWindowSize(sz,  ImGuiCond_Always);
    }

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings | extra_flags;

    bool open = true;
    bool visible = ImGui::Begin(id, p_open ? p_open : &open, flags);
    if (visible) {
        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();
        const float chrome_h = draw_chrome(spec, wp, ws);
        ImGui::Dummy(ImVec2(0.0f, chrome_h));
    }
    return visible;
}

void end_window() {
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

// ---------------------------------------------------------------------------
// Structure
// ---------------------------------------------------------------------------
void section_header(const char* label, const char* trailing) {
    ImGui::PushStyleColor(ImGuiCol_Text, palette.text_mid);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    if (trailing && trailing[0]) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, palette.text_lo);
        ImGui::TextUnformatted(trailing);
        ImGui::PopStyleColor();
    }
    ImGui::Spacing();
    divider();
}

void divider(float alpha) {
    ImGui::PushStyleColor(ImGuiCol_Separator, with_alpha(palette.border_soft, alpha));
    ImGui::Separator();
    ImGui::PopStyleColor();
}

void accent_rule(float thickness, float alpha) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 c0 = u32(with_alpha(palette.accent, alpha));
    const ImU32 c1 = u32(with_alpha(palette.secondary, alpha * 0.6f));
    dl->AddRectFilledMultiColor(p, ImVec2(p.x + w, p.y + thickness), c0, c1, c1, c0);
    ImGui::Dummy(ImVec2(0.0f, thickness));
}

void begin_card(const char* id, const ImVec2& size) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, palette.surface);
    ImGui::PushStyleColor(ImGuiCol_Border,  palette.border_soft);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   metrics::card_round);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(metrics::pad, metrics::pad));
    ImGui::BeginChild(id, size, ImGuiChildFlags_Borders);
}

void end_card() {
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

// ---------------------------------------------------------------------------
// Navigation & controls
// ---------------------------------------------------------------------------
bool nav_item(const char* label, const char* meta, bool selected, bool disabled) {
    ImGui::PushID(label);
    const float h = metrics::row_h;
    const float w = ImGui::GetContentRegionAvail().x;
    const ImVec2 p = ImGui::GetCursorScreenPos();

    if (disabled) ImGui::BeginDisabled();
    bool clicked = ImGui::InvisibleButton("##hit", ImVec2(w, h));
    const bool hovered = !disabled && ImGui::IsItemHovered();
    if (disabled) ImGui::EndDisabled();
    ImGui::PopID();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (selected)
        dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), u32(palette.selected), metrics::control_round);
    else if (hovered)
        dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), u32(palette.elevated), metrics::control_round);

    if (selected)
        dl->AddRectFilled(p, ImVec2(p.x + 2.5f, p.y + h), u32(palette.accent), 1.0f);

    const ImU32 tc = u32(disabled ? palette.text_lo
                                  : (selected ? palette.text_hi : palette.text_mid));
    const float ty = p.y + h * 0.5f - ImGui::GetTextLineHeight() * 0.5f;
    dl->AddText(ImVec2(p.x + metrics::pad, ty), tc, label);

    if (meta && meta[0]) {
        const ImVec2 ms = ImGui::CalcTextSize(meta);
        dl->AddText(ImVec2(p.x + w - metrics::pad - ms.x, ty), u32(palette.text_lo), meta);
    }
    return clicked;
}

bool toolbar_button(const char* label, bool active, const ImVec2& size) {
    ImGui::PushStyleColor(ImGuiCol_Button,        active ? palette.accent_soft  : palette.elevated);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active ? with_alpha(palette.accent, 0.24f)
                                                         : palette.selected);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  palette.accent_dim);
    ImGui::PushStyleColor(ImGuiCol_Text,          active ? palette.accent_hover : palette.text_mid);
    const bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleColor(4);
    return clicked;
}

// ---------------------------------------------------------------------------
// Data display
// ---------------------------------------------------------------------------
void status_pill(const char* label, bool ok) {
    ImGui::PushID(label);
    const ImVec2 ts = ImGui::CalcTextSize(label);
    const float h = 20.0f;
    const float w = ts.x + 27.0f;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const ImVec4 dot = ok ? palette.success : palette.warning;
    framed_rect(dl, p, ImVec2(p.x + w, p.y + h),
                with_alpha(dot, 0.13f), with_alpha(dot, 0.34f), h * 0.5f);
    dl->AddCircleFilled(ImVec2(p.x + 11.0f, p.y + h * 0.5f), 3.0f, u32(dot));
    dl->AddText(ImVec2(p.x + 19.0f, p.y + h * 0.5f - ts.y * 0.5f),
                u32(ok ? palette.text_hi : palette.text_mid), label);
    ImGui::Dummy(ImVec2(w, h));
    ImGui::PopID();
}

void badge(const char* label, const ImVec4& color) {
    ImGui::PushID(label);
    const ImVec2 ts = ImGui::CalcTextSize(label);
    const float h = 18.0f;
    const float w = ts.x + 16.0f;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    framed_rect(dl, p, ImVec2(p.x + w, p.y + h),
                with_alpha(color, 0.14f), with_alpha(color, 0.38f), 5.0f);
    dl->AddText(ImVec2(p.x + 8.0f, p.y + h * 0.5f - ts.y * 0.5f), u32(color), label);
    ImGui::Dummy(ImVec2(w, h));
    ImGui::PopID();
}

void metric(const char* label, const char* value, const ImVec4& value_color) {
    ImGui::PushStyleColor(ImGuiCol_Text, palette.text_lo);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Text, value_color);
    ImGui::TextUnformatted(value);
    ImGui::PopStyleColor();
}

void keyboard_hint(const char* key, const char* label) {
    ImGui::PushID(key);
    const ImVec2 ks = ImGui::CalcTextSize(key);
    const float h = 18.0f;
    const float w = ks.x + 12.0f;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    framed_rect(dl, p, ImVec2(p.x + w, p.y + h), palette.elevated, palette.border, 4.0f);
    dl->AddText(ImVec2(p.x + 6.0f, p.y + h * 0.5f - ks.y * 0.5f), u32(palette.text_hi), key);
    dl->AddText(ImVec2(p.x + w + metrics::gap,
                       p.y + h * 0.5f - ImGui::GetTextLineHeight() * 0.5f),
                u32(palette.text_mid), label);
    ImGui::Dummy(ImVec2(w, h));
    ImGui::PopID();
}

} // namespace xpera
