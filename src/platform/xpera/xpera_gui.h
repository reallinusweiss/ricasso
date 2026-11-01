// =============================================================================
// xpera_gui.h — Xpera widget toolkit
//
// The building blocks every remastered Xpera surface uses: frameless window
// chrome, nav rail items, sections, cards, pills, badges and metrics.
//
// Design rules encoded here (so panels can't accidentally re-introduce slop):
//   • No OS-style title bar, no game icon, no chunky red close button. The
//     only affordance is a slim chrome strip + a keyboard hint.
//   • Depth = value steps (surface / elevated / overlay), never glow.
//   • One accent. Semantics use the semantic colours, nothing else.
//   • Everything is flat + hairline-bordered; radius stays <= 10px.
//
// Not wired into any panel yet. Depends on Dear ImGui only.
// =============================================================================
#pragma once

#include "imgui/imgui.h"
#include "platform/xpera/xpera_theme.h"

namespace xpera {

// ---------------------------------------------------------------------------
// Window chrome
// ---------------------------------------------------------------------------
enum class WindowMode {
    Fullscreen, // edge-to-edge workspace (tool windows)
    Panel,      // centred modal card (dialogs / settings)
};

struct WindowSpec {
    WindowMode  mode        = WindowMode::Fullscreen;
    const char* eyebrow     = nullptr;   // small uppercase kicker above the title
    const char* title       = nullptr;   // primary title
    const char* subtitle    = nullptr;   // one-line description
    const char* close_hint  = "Esc";     // key hint on the right; nullptr/"" hides it
    ImVec2      panel_size  = ImVec2(980, 660);
    ImVec2      panel_pos   = ImVec2(-1, -1); // -1 => horizontally+vertically centred
};

// Frameless Xpera window. Draws the chrome strip and reserves its space, then
// leaves the cursor at the top-left of the padded content region.
// Pair every begin_window() with end_window() (even when it returns false).
bool begin_window(const char* id, const WindowSpec& spec,
                  bool* p_open = nullptr, ImGuiWindowFlags extra_flags = 0);
void end_window();

// ---------------------------------------------------------------------------
// Structure
// ---------------------------------------------------------------------------
void section_header(const char* label, const char* trailing = nullptr);
void divider(float alpha = 1.0f);
void accent_rule(float thickness = 2.0f, float alpha = 0.55f);

// Lightweight rounded panel that hosts content (replaces raw BeginChild+border).
void begin_card(const char* id, const ImVec2& size = ImVec2(0, 0));
void end_card();

// ---------------------------------------------------------------------------
// Navigation & controls
// ---------------------------------------------------------------------------
// Flat nav-rail row with an accent bar when selected. Returns true on click.
bool nav_item(const char* label, const char* meta, bool selected,
              bool disabled = false);

// Toolbar-level button. `active` paints the accent tint used for a selected
// tool. Returns true on click.
bool toolbar_button(const char* label, bool active = false,
                    const ImVec2& size = ImVec2(0, 0));

// ---------------------------------------------------------------------------
// Data display
// ---------------------------------------------------------------------------
// Live status chip with a coloured dot: green when ok, muted warning otherwise.
void status_pill(const char* label, bool ok);

// Neutral chip with an arbitrary semantic colour.
void badge(const char* label, const ImVec4& color);

// Stacked label + value, used by dashboards / inspectors.
void metric(const char* label, const char* value, const ImVec4& value_color);

// Key-cap followed by its description (used in hotkey panels).
void keyboard_hint(const char* key, const char* label);

} // namespace xpera
