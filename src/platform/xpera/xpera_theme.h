// =============================================================================
// xpera_theme.h — Xpera visual language (palette + colour helpers)
//
// Xpera is a NEW, standalone GUI style for SwordfareDesktop. It is intentionally
// NOT wired into any panel yet — it exists alongside the existing Swordfare
// style so it can be adopted incrementally (see docs/xpera/IMPLEMENTATION_PLAN.md).
//
// Design language (mirrors the Ruby GG "Dark Studio" reference in
// src/ruby/theme/ruby_theme.h, pushed darker and more restrained):
//   • near-black neutral greys, one accent, one secondary — no rainbow.
//   • flat surfaces separated by hairline borders, never glow/gradient slop.
//   • generous negative space, small uppercase labels, one type scale.
//   • depth comes from value steps, not from drop shadows or bevels.
//
// This header depends on Dear ImGui only.
// =============================================================================
#pragma once

#include "imgui/imgui.h"

namespace xpera {

// ---------------------------------------------------------------------------
// XperaDark — the flagship palette
//
// A neutral charcoal ramp (#08090B → #232A34) with a single indigo accent and a
// teal secondary. Deliberately has NO red/blue duotone (the look we are moving
// away from) and NO saturated "game HUD" colours.
// ---------------------------------------------------------------------------
struct Palette {
    // ── Surfaces (value ladder; the ONLY thing that creates depth) ──────────
    ImVec4 void_bg     = ImVec4(0.031f, 0.035f, 0.043f, 1.00f); // #08090B  app backdrop
    ImVec4 base        = ImVec4(0.055f, 0.063f, 0.075f, 1.00f); // #0E1013  window
    ImVec4 surface     = ImVec4(0.078f, 0.090f, 0.110f, 1.00f); // #14171C  card / panel
    ImVec4 elevated    = ImVec4(0.102f, 0.118f, 0.141f, 1.00f); // #1A1E24  hover / raised
    ImVec4 overlay     = ImVec4(0.122f, 0.141f, 0.173f, 1.00f); // #1F242C  popup / menu
    ImVec4 input       = ImVec4(0.071f, 0.082f, 0.102f, 1.00f); // #12151A  field wells
    ImVec4 selected    = ImVec4(0.137f, 0.165f, 0.204f, 1.00f); // #232A34  active row

    // ── Hairlines ───────────────────────────────────────────────────────────
    ImVec4 border_soft = ImVec4(0.133f, 0.149f, 0.180f, 1.00f); // #22262E
    ImVec4 border      = ImVec4(0.173f, 0.196f, 0.231f, 1.00f); // #2C323B
    ImVec4 border_focus= ImVec4(0.239f, 0.275f, 0.325f, 1.00f); // #3D4653

    // ── Text ────────────────────────────────────────────────────────────────
    ImVec4 text_hi     = ImVec4(0.906f, 0.918f, 0.941f, 1.00f); // #E7EAF0
    ImVec4 text_mid    = ImVec4(0.627f, 0.659f, 0.706f, 1.00f); // #A0A8B4
    ImVec4 text_lo     = ImVec4(0.380f, 0.416f, 0.467f, 1.00f); // #616A77
    ImVec4 text_on_acc = ImVec4(0.043f, 0.047f, 0.063f, 1.00f); // ink on accent

    // ── Accents ─────────────────────────────────────────────────────────────
    ImVec4 accent      = ImVec4(0.486f, 0.424f, 1.000f, 1.00f); // #7C6CFF  indigo
    ImVec4 accent_hover= ImVec4(0.569f, 0.518f, 1.000f, 1.00f); // #9184FF
    ImVec4 accent_dim  = ImVec4(0.357f, 0.310f, 0.878f, 1.00f); // #5B4FE0
    ImVec4 accent_soft = ImVec4(0.486f, 0.424f, 1.000f, 0.16f);
    ImVec4 accent_line = ImVec4(0.486f, 0.424f, 1.000f, 0.55f);
    ImVec4 secondary   = ImVec4(0.220f, 0.780f, 0.753f, 1.00f); // #38C7C0  teal

    // ── Semantic ────────────────────────────────────────────────────────────
    ImVec4 info        = ImVec4(0.298f, 0.761f, 1.000f, 1.00f); // #4CC2FF
    ImVec4 success     = ImVec4(0.275f, 0.784f, 0.541f, 1.00f); // #46C88A
    ImVec4 warning     = ImVec4(0.902f, 0.698f, 0.243f, 1.00f); // #E6B23E
    ImVec4 danger      = ImVec4(0.941f, 0.322f, 0.373f, 1.00f); // #F0525F
};

// The live palette. Swap by assigning a different Palette to xpera::palette.
inline Palette palette{};

// ---------------------------------------------------------------------------
// Colour helpers
// ---------------------------------------------------------------------------
inline ImVec4 with_alpha(ImVec4 c, float a) { c.w = a; return c; }
inline ImVec4 scale_rgb(ImVec4 c, float s) {
    c.x *= s; c.y *= s; c.z *= s;
    return c;
}
inline ImVec4 mix(ImVec4 a, ImVec4 b, float t) {
    return ImVec4(a.x + (b.x - a.x) * t,
                  a.y + (b.y - a.y) * t,
                  a.z + (b.z - a.z) * t,
                  a.w + (b.w - a.w) * t);
}
inline ImU32 u32(ImVec4 c) { return ImGui::ColorConvertFloat4ToU32(c); }

// ---------------------------------------------------------------------------
// Metrics — one spacing scale so the whole app stays on-grid.
// ---------------------------------------------------------------------------
namespace metrics {
    inline constexpr float row_h        = 26.0f;  // list row height
    inline constexpr float control_h    = 28.0f;  // button / input height
    inline constexpr float titlebar_h   = 44.0f;  // Xpera window header strip
    inline constexpr float card_round   = 10.0f;
    inline constexpr float control_round= 7.0f;
    inline constexpr float pad          = 12.0f;
    inline constexpr float gap          = 8.0f;
    inline constexpr float nav_w        = 208.0f; // nav rail width
} // namespace metrics

} // namespace xpera
