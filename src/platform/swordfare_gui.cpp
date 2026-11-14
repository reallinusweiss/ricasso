// =============================================================================
// Swordfare GUI — ImGui layer drawn on top of the running game
//
// Dear ImGui (SDL3 + OpenGL 3.3 or Vulkan) with its own context. What is drawn
// here stays inside the game picture: the F3 debug overlay and the buttons and
// overlays that Lua mods register. Everything with controls is a tool window
// (tool_windows.cpp, tool_windows_extra.cpp); this class only keeps the
// backends those windows use: the Lua console bridge, its TCP server and the
// scene shifter.
// =============================================================================

#if defined(_WIN32)
// Winsock must precede <windows.h>; swordfare_gui.h -> gl_inc.h pulls it.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "platform/swordfare_gui.h"
#include "platform/IconsFontAwesome6.h"
#include "platform/embedded_assets.h"
#include "platform/swordfare_theme.h"
#include "platform/xpera/xpera_style.h"
#include "platform/xpera/xpera_gui.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_sdl3.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#ifdef VULKAN_BACKEND
#define VK_NO_PROTOTYPES
#include "volk.h"
#include "platform/vulkan_backend.h"
#include "imgui/backends/imgui_impl_vulkan.h"
#endif
#include "platform/rgc.h"
#include "platform/os_external.h"

#include <cstring>
#include "display.h"
#include "data_path.h"
#include "fbo_scaler.h"
#include "platform/launcher_config.h"
#include "platform/mod_manager.h"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <cstdlib>
#include <string>
#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#else
typedef SSIZE_T ssize_t;
#ifndef SHUT_RD
#define SHUT_RD   0
#define SHUT_WR   1
#define SHUT_RDWR 2
#endif
/* This TU only ever reads/writes/closes sockets (free-function shims, so
   std::ofstream::close()/write() members are untouched). */
inline int close(SOCKET s)                       { return closesocket(s); }
inline int read(int s, void *b, size_t n)        { return recv((SOCKET)s, (char *)b, (int)n, 0); }
inline int write(int s, const void *b, size_t n) { return send((SOCKET)s, (const char *)b, (int)n, 0); }
#endif
#include <cmath>

bool g_sre_overlay_blocking = false;

// Guest heap size reporters (defined in jni_bridge_arm64.cpp / jni_bridge.cpp)
extern "C" uint32_t get_guest_heap_size_64();
extern "C" uint32_t get_guest_heap_size_32();

// -----------------------------------------------------------------------------
//  F3: SRE13 texture-identity telemetry  (additive, removable)
// -----------------------------------------------------------------------------
//  Declared here, not through jni_bridge.h, matching the two externs above.
//  Layout: [0] bind_calls [1] identified [2] unique_names
//          [3] trusted    [4] rejected   [5] level_generation
//  Delete this extern and the matching F3 block together.
extern "C" void sre13_tex_telemetry_snapshot(uint32_t out[6]);
// F3 diagnostic for the authored-emitter bloom key; defined in fbo_scaler.cpp.
// REMOVABLE together with the bloom block below and the function itself.
extern "C" int swordigo_bloom_emitter_report(char* out, int cap);


// Weak fallback definitions for SRE Scene Shifter symbols (resolved from guest space at runtime)
extern "C" {
    SWORDIGO_WEAK int    g_sre_scene_list_count = 0;
    SWORDIGO_WEAK char   g_sre_scene_list[256][128] = {{0}};
    SWORDIGO_WEAK volatile int   g_sre_scene_shift_pending = 0;
    SWORDIGO_WEAK char   g_sre_scene_shift_target[128] = {0};
    SWORDIGO_WEAK char   g_sre_scene_shift_spawn[64] = "start";
    SWORDIGO_WEAK char   g_sre_scene_shift_last_error[256] = {0};
    SWORDIGO_WEAK volatile int   g_sre_scene_shift_active = 0;
    SWORDIGO_WEAK char   g_sre_current_scene_name[128] = {0};
    SWORDIGO_WEAK void   sre_scene_shifter_scan_scenes(void) {}
    // AnimateIn hook trampoline pointer — set by host after relay install
    SWORDIGO_WEAK void*  g_orig_SceneLoadingView_AnimateIn = NULL;
}

// ---------------------------------------------------------------------------
// Helpers — no stdlib in SRE land, but this is host-side C++ so fine
// ---------------------------------------------------------------------------

static void swardfare_push_fps(float* history, int& idx, float value, int size) {
    history[idx] = value;
    idx = (idx + 1) % size;
}

// ---------------------------------------------------------------------------
// Theme — Swordfare Dark  (mirrors the launcher palette)
// ---------------------------------------------------------------------------

void SwordfareGUI::apply_swordfare_theme() {
    // Start from the shared design system so the in-game overlays (F1 mod
    // overlay, F3 debug/tools) share rounding, spacing, and the base palette
    // with the launcher, loading screen, and crash dialog. The Swordfare
    // crimson-accent overrides below then refine the look on top.
    sf_theme::ApplyTheme();
    ImGuiStyle& s = ImGui::GetStyle();

    // --- Geometry ---
    s.WindowRounding    = 10.0f;
    s.ChildRounding     =  8.0f;
    s.FrameRounding     =  6.0f;
    s.PopupRounding     =  8.0f;
    s.GrabRounding      =  4.0f;
    s.TabRounding       =  6.0f;
    s.ScrollbarRounding =  6.0f;

    s.WindowPadding     = ImVec2(16, 14);
    s.FramePadding      = ImVec2(10, 5);
    s.ItemSpacing       = ImVec2(10, 6);
    s.ItemInnerSpacing  = ImVec2(6,  4);
    s.IndentSpacing     = 16.0f;
    s.ScrollbarSize     = 10.0f;
    s.GrabMinSize       = 10.0f;

    s.WindowBorderSize  = 1.0f;
    s.ChildBorderSize   = 1.0f;
    s.FrameBorderSize   = 0.0f;
    s.PopupBorderSize   = 1.0f;
    s.TabBorderSize     = 0.0f;

    // --- Colours ---
    ImVec4* c = s.Colors;

    // Backgrounds
    c[ImGuiCol_WindowBg]          = ImVec4(0.04f,  0.05f,  0.07f,  0.92f);  // deep near-black, semi-transparent
    c[ImGuiCol_ChildBg]           = ImVec4(0.07f,  0.09f,  0.12f,  1.00f);  // #121720
    c[ImGuiCol_PopupBg]           = ImVec4(0.06f,  0.08f,  0.11f,  0.97f);
    c[ImGuiCol_FrameBg]           = ImVec4(0.10f,  0.13f,  0.18f,  1.00f);
    c[ImGuiCol_FrameBgHovered]    = ImVec4(0.13f,  0.17f,  0.24f,  1.00f);
    c[ImGuiCol_FrameBgActive]     = ImVec4(0.16f,  0.20f,  0.28f,  1.00f);

    // Title bar
    c[ImGuiCol_TitleBg]           = ImVec4(0.04f,  0.05f,  0.07f,  1.00f);
    c[ImGuiCol_TitleBgActive]     = ImVec4(0.07f,  0.08f,  0.10f,  1.00f);
    c[ImGuiCol_TitleBgCollapsed]  = ImVec4(0.04f,  0.05f,  0.07f,  0.70f);

    // Borders
    c[ImGuiCol_Border]            = ImVec4(0.914f, 0.271f, 0.376f, 0.25f);  // subtle crimson outline
    c[ImGuiCol_BorderShadow]      = ImVec4(0.000f, 0.000f, 0.000f, 0.00f);

    // Scrollbar
    c[ImGuiCol_ScrollbarBg]       = ImVec4(0.04f,  0.05f,  0.07f,  0.40f);
    c[ImGuiCol_ScrollbarGrab]     = ImVec4(0.20f,  0.23f,  0.27f,  0.80f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.28f, 0.32f, 0.38f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.38f, 0.42f, 0.48f, 1.00f);

    // Accent — Swordfare Crimson (#e94560)
    ImVec4 accent      = ImVec4(0.914f, 0.271f, 0.376f, 1.00f);
    ImVec4 accent_dim  = ImVec4(0.914f, 0.271f, 0.376f, 0.50f);
    ImVec4 accent_pale = ImVec4(0.914f, 0.271f, 0.376f, 0.20f);

    c[ImGuiCol_CheckMark]         = accent;
    c[ImGuiCol_SliderGrab]        = ImVec4(0.35f,  0.65f,  1.00f,  0.80f);
    c[ImGuiCol_SliderGrabActive]  = ImVec4(0.35f,  0.65f,  1.00f,  1.00f);

    c[ImGuiCol_Button]            = accent;
    c[ImGuiCol_ButtonHovered]     = ImVec4(1.00f,  0.38f,  0.48f,  1.00f);
    c[ImGuiCol_ButtonActive]      = ImVec4(0.78f,  0.20f,  0.29f,  1.00f);

    c[ImGuiCol_Header]            = accent_pale;
    c[ImGuiCol_HeaderHovered]     = accent_dim;
    c[ImGuiCol_HeaderActive]      = accent;

    c[ImGuiCol_Separator]         = ImVec4(0.914f, 0.271f, 0.376f, 0.18f);
    c[ImGuiCol_SeparatorHovered]  = accent_dim;
    c[ImGuiCol_SeparatorActive]   = accent;

    c[ImGuiCol_ResizeGrip]        = accent_pale;
    c[ImGuiCol_ResizeGripHovered] = accent_dim;
    c[ImGuiCol_ResizeGripActive]  = accent;

    c[ImGuiCol_Tab]               = ImVec4(0.10f,  0.13f,  0.18f,  1.00f);
    c[ImGuiCol_TabHovered]        = accent_dim;
    c[ImGuiCol_TabSelected]       = ImVec4(0.914f, 0.271f, 0.376f, 0.80f);

    // Text
    c[ImGuiCol_Text]              = ImVec4(0.90f,  0.93f,  0.95f,  1.00f);
    c[ImGuiCol_TextDisabled]      = ImVec4(0.54f,  0.58f,  0.62f,  1.00f);

    // Table
    c[ImGuiCol_TableHeaderBg]     = ImVec4(0.10f,  0.13f,  0.18f,  1.00f);
    c[ImGuiCol_TableBorderStrong] = ImVec4(0.20f,  0.23f,  0.27f,  0.60f);
    c[ImGuiCol_TableBorderLight]  = ImVec4(0.20f,  0.23f,  0.27f,  0.30f);
    c[ImGuiCol_TableRowBg]        = ImVec4(0.00f,  0.00f,  0.00f,  0.00f);
    c[ImGuiCol_TableRowBgAlt]     = ImVec4(1.00f,  1.00f,  1.00f,  0.02f);

    // Plot
    c[ImGuiCol_PlotLines]         = ImVec4(0.35f,  0.65f,  1.00f,  1.00f);  // blue FPS graph
    c[ImGuiCol_PlotLinesHovered]  = ImVec4(0.914f, 0.271f, 0.376f, 1.00f);
    c[ImGuiCol_PlotHistogram]     = ImVec4(0.35f,  0.65f,  1.00f,  0.80f);
    c[ImGuiCol_PlotHistogramHovered] = ImVec4(0.914f, 0.271f, 0.376f, 1.00f);

    // ── Xpera skin (opt-out via SWORDFARE_XPERA_UI=0) ─────────────────────
    // When enabled this fully overrides the Swordfare palette/metrics above.
    // The original theme is intentionally kept intact as the fallback.
    if (xpera::ui_enabled()) {
        xpera::apply_style();
    }
}

// ---------------------------------------------------------------------------
// init / shutdown
// ---------------------------------------------------------------------------

void SwordfareGUI::init(SDL_Window* window, SDL_GLContext gl_ctx) {
    if (m_initialized) return;

    m_window  = window;
    m_gl_ctx  = gl_ctx;

    // Create a dedicated ImGui context — does not share state with the launcher
    m_imgui_ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(m_imgui_ctx));

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;          // no imgui.ini on disk for the in-game overlay
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;  // game cursor is hidden

    // Compute DPI Scale
    float dpi_scale = 1.0f;
    int ww, wh, pw, ph;
    SDL_GetWindowSize(window, &ww, &wh);
    SDL_GetWindowSizeInPixels(window, &pw, &ph);
    if (ww > 0 && pw > 0) {
        dpi_scale = (float)pw / (float)ww;
    }
    if (dpi_scale < 1.0f) dpi_scale = 1.0f;

    // Intelligent layout scaling relative to window size
    float layout_scale = 1.0f;
    if (wh >= 1440) {
        layout_scale = 1.5f;
    } else if (wh >= 1080) {
        layout_scale = 1.25f;
    } else {
        layout_scale = 1.0f;
    }

    // Find font files
    std::string main_font_path, button_font_path, fa_path;
    std::string home_dir = os_external::home_dir();

    std::vector<std::string> main_font_candidates = {
        // dist/ portable layout (standalone release — SWORDIGO_DATA_DIR)
        get_data_path("fonts/MegalopolisExtra-Regular.otf"),
        get_data_path("fonts/SpaceGrotesk-VariableFont_wght.ttf"),
        get_data_path("fonts/Inter-Regular.ttf"),
        "./data/fonts/MegalopolisExtra-Regular.otf",
        "./data/fonts/SpaceGrotesk-VariableFont_wght.ttf",
        "./data/fonts/Inter-Regular.ttf",
        "./fonts/MegalopolisExtra-Regular.otf",
        "./fonts/SpaceGrotesk-VariableFont_wght.ttf",
        "./fonts/Inter-Regular.ttf",
        "../fonts/MegalopolisExtra-Regular.otf",
        "../fonts/SpaceGrotesk-VariableFont_wght.ttf",
        "../fonts/Inter-Regular.ttf",
        // Dev paths
        "src/assets/fonts/MegalopolisExtra-Regular.otf",
        "src/assets/fonts/Redaction10-Regular.otf",
        "src/assets/fonts/Inter-Regular.ttf",
        "/usr/share/ricasso/launcher/fonts/MegalopolisExtra-Regular.otf",
        "/usr/share/ricasso/launcher/fonts/Redaction10-Regular.otf",
        "/usr/share/ricasso/launcher/fonts/Inter-Regular.ttf",
        "/usr/local/share/ricasso/launcher/fonts/MegalopolisExtra-Regular.otf",
        "/usr/local/share/ricasso/launcher/fonts/Redaction10-Regular.otf",
        "/usr/local/share/ricasso/launcher/fonts/Inter-Regular.ttf",
        "/usr/share/ricasso/src/assets/fonts/Redaction10-Regular.otf",
        "/usr/share/ricasso/src/assets/fonts/Inter-Regular.ttf"
    };
    if (!home_dir.empty()) {
        main_font_candidates.push_back(home_dir + "/.local/share/ricasso/launcher/fonts/MegalopolisExtra-Regular.otf");
        main_font_candidates.push_back(home_dir + "/.local/share/ricasso/launcher/fonts/Redaction10-Regular.otf");
        main_font_candidates.push_back(home_dir + "/.local/share/ricasso/launcher/fonts/Inter-Regular.ttf");
        main_font_candidates.push_back(home_dir + "/.local/share/ricasso/src/assets/fonts/MegalopolisExtra-Regular.otf");
        main_font_candidates.push_back(home_dir + "/.local/share/ricasso/src/assets/fonts/Redaction10-Regular.otf");
        main_font_candidates.push_back(home_dir + "/.local/share/ricasso/src/assets/fonts/Inter-Regular.ttf");
        main_font_candidates.push_back(home_dir + "/.local/share/ricasso-launcher/src/assets/fonts/Redaction10-Regular.otf");
        main_font_candidates.push_back(home_dir + "/.local/share/ricasso-launcher/src/assets/fonts/Inter-Regular.ttf");
        main_font_candidates.push_back(home_dir + "/.local/share/swordigo-launcher/src/assets/fonts/Redaction10-Regular.otf");
        main_font_candidates.push_back(home_dir + "/.local/share/swordigo-launcher/src/assets/fonts/Inter-Regular.ttf");
        main_font_candidates.push_back(home_dir + "/.local/share/launcher/src/assets/fonts/Redaction10-Regular.otf");
        main_font_candidates.push_back(home_dir + "/.local/share/launcher/src/assets/fonts/Inter-Regular.ttf");
    }
    for (const auto& fp : main_font_candidates) {
        if (std::filesystem::exists(fp)) {
            main_font_path = fp;
            break;
        }
    }

    std::vector<std::string> button_font_candidates = {
        "src/assets/fonts/MegalopolisExtra-Regular.otf",
        "src/assets/fonts/Redaction10-Bold.otf",
        "src/assets/fonts/Inter-Regular.ttf",
        "/usr/share/ricasso/launcher/fonts/MegalopolisExtra-Regular.otf",
        "/usr/share/ricasso/launcher/fonts/Redaction10-Bold.otf",
        "/usr/share/ricasso/launcher/fonts/Inter-Regular.ttf",
        "/usr/local/share/ricasso/launcher/fonts/MegalopolisExtra-Regular.otf",
        "/usr/local/share/ricasso/launcher/fonts/Redaction10-Bold.otf",
        "/usr/local/share/ricasso/launcher/fonts/Inter-Regular.ttf",
        "/usr/share/ricasso/src/assets/fonts/Redaction10-Bold.otf",
        "/usr/share/ricasso/src/assets/fonts/Inter-Regular.ttf"
    };
    if (!home_dir.empty()) {
        button_font_candidates.push_back(home_dir + "/.local/share/ricasso/launcher/fonts/MegalopolisExtra-Regular.otf");
        button_font_candidates.push_back(home_dir + "/.local/share/ricasso/launcher/fonts/Redaction10-Bold.otf");
        button_font_candidates.push_back(home_dir + "/.local/share/ricasso/launcher/fonts/Inter-Regular.ttf");
        button_font_candidates.push_back(home_dir + "/.local/share/ricasso/src/assets/fonts/MegalopolisExtra-Regular.otf");
        button_font_candidates.push_back(home_dir + "/.local/share/ricasso/src/assets/fonts/Redaction10-Bold.otf");
        button_font_candidates.push_back(home_dir + "/.local/share/ricasso/src/assets/fonts/Inter-Regular.ttf");
        button_font_candidates.push_back(home_dir + "/.local/share/ricasso-launcher/src/assets/fonts/Redaction10-Bold.otf");
        button_font_candidates.push_back(home_dir + "/.local/share/ricasso-launcher/src/assets/fonts/Inter-Regular.ttf");
        button_font_candidates.push_back(home_dir + "/.local/share/swordigo-launcher/src/assets/fonts/Redaction10-Bold.otf");
        button_font_candidates.push_back(home_dir + "/.local/share/swordigo-launcher/src/assets/fonts/Inter-Regular.ttf");
        button_font_candidates.push_back(home_dir + "/.local/share/launcher/src/assets/fonts/Redaction10-Bold.otf");
        button_font_candidates.push_back(home_dir + "/.local/share/launcher/src/assets/fonts/Inter-Regular.ttf");
    }
    for (const auto& fp : button_font_candidates) {
        if (std::filesystem::exists(fp)) {
            button_font_path = fp;
            break;
        }
    }

    std::vector<std::string> fa_candidates = {
        // dist/ portable layout (standalone release — SWORDIGO_DATA_DIR)
        get_data_path("fonts/Font Awesome 7 Free-Solid-900.otf"),
        get_data_path("fonts/fa-solid-900.ttf"),
        "./data/fonts/Font Awesome 7 Free-Solid-900.otf",
        "./data/fonts/fa-solid-900.ttf",
        "./fonts/Font Awesome 7 Free-Solid-900.otf",
        "./fonts/fa-solid-900.ttf",
        "../fonts/Font Awesome 7 Free-Solid-900.otf",
        "../fonts/fa-solid-900.ttf",
        // Dev paths
        "src/assets/fontawesome/otfs/Font Awesome 7 Free-Solid-900.otf",
        "src/assets/fonts/fa-solid-900.ttf",
        "/usr/share/ricasso/launcher/fontawesome/otfs/Font Awesome 7 Free-Solid-900.otf",
        "/usr/share/ricasso/launcher/fonts/fa-solid-900.ttf",
        "/usr/local/share/ricasso/launcher/fontawesome/otfs/Font Awesome 7 Free-Solid-900.otf",
        "/usr/local/share/ricasso/launcher/fonts/fa-solid-900.ttf",
        "/usr/share/ricasso/src/assets/fontawesome/otfs/Font Awesome 7 Free-Solid-900.otf",
        "/usr/share/ricasso/src/assets/fonts/fa-solid-900.ttf"
    };
    if (!home_dir.empty()) {
        fa_candidates.push_back(home_dir + "/.local/share/ricasso/launcher/fontawesome/otfs/Font Awesome 7 Free-Solid-900.otf");
        fa_candidates.push_back(home_dir + "/.local/share/ricasso/launcher/fonts/fa-solid-900.ttf");
        fa_candidates.push_back(home_dir + "/.local/share/ricasso/src/assets/fontawesome/otfs/Font Awesome 7 Free-Solid-900.otf");
        fa_candidates.push_back(home_dir + "/.local/share/ricasso/src/assets/fonts/fa-solid-900.ttf");
        fa_candidates.push_back(home_dir + "/.local/share/ricasso-launcher/src/assets/fontawesome/otfs/Font Awesome 7 Free-Solid-900.otf");
        fa_candidates.push_back(home_dir + "/.local/share/ricasso-launcher/src/assets/fonts/fa-solid-900.ttf");
        fa_candidates.push_back(home_dir + "/.local/share/swordigo-launcher/src/assets/fontawesome/otfs/Font Awesome 7 Free-Solid-900.otf");
        fa_candidates.push_back(home_dir + "/.local/share/swordigo-launcher/src/assets/fonts/fa-solid-900.ttf");
        fa_candidates.push_back(home_dir + "/.local/share/launcher/src/assets/fontawesome/otfs/Font Awesome 7 Free-Solid-900.otf");
        fa_candidates.push_back(home_dir + "/.local/share/launcher/src/assets/fonts/fa-solid-900.ttf");
    }
    for (const auto& fp : fa_candidates) {
        if (std::filesystem::exists(fp)) {
            fa_path = fp;
            break;
        }
    }

    // ── Embedded fonts FIRST (permanent fix — baked into the binary) ──
    const unsigned char* emb_main = nullptr;    size_t emb_main_size = 0;
    const unsigned char* emb_button = nullptr;  size_t emb_button_size = 0;
    const unsigned char* emb_fa = nullptr;      size_t emb_fa_size = 0;
    const char* emb_main_names[] = {
        "fonts/MegalopolisExtra-Regular.otf", "fonts/Redaction10-Regular.otf",
        "fonts/Inter-Regular.ttf",
    };
    for (const char* n : emb_main_names)
        if (embedded_asset(n, &emb_main, &emb_main_size)) break;
    const char* emb_button_names[] = {
        "fonts/MegalopolisExtra-Regular.otf", "fonts/Redaction10-Bold.otf",
        "fonts/Inter-Regular.ttf",
    };
    for (const char* n : emb_button_names)
        if (embedded_asset(n, &emb_button, &emb_button_size)) break;
    if (!embedded_asset("fonts/fa-solid-900.ttf", &emb_fa, &emb_fa_size))
        embedded_asset("fonts/fa-solid-900.otf", &emb_fa, &emb_fa_size);

    if (emb_main && emb_main_size > 0) {
        float main_font_size = 14.0f * dpi_scale;
        // Embedded fonts are static .rodata — the atlas must NOT free() them.
        ImFontConfig emb_cfg;
        emb_cfg.FontDataOwnedByAtlas = false;
        m_font_main = io.Fonts->AddFontFromMemoryTTF((void*)emb_main, (int)emb_main_size,
                                                     main_font_size, &emb_cfg);

        // Merge FontAwesome into main font
        if (m_font_main && emb_fa && emb_fa_size > 0) {
            static const ImWchar icon_ranges[] = { ICON_FA_MIN, ICON_FA_MAX, 0 };
            ImFontConfig icon_cfg;
            icon_cfg.MergeMode = true;
            icon_cfg.PixelSnapH = true;
            icon_cfg.GlyphMinAdvanceX = main_font_size;
            icon_cfg.GlyphOffset = ImVec2(0, 1);
            icon_cfg.FontDataOwnedByAtlas = false;
            io.Fonts->AddFontFromMemoryTTF((void*)emb_fa, (int)emb_fa_size,
                                           main_font_size * 0.85f, &icon_cfg, icon_ranges);
        }
        std::cout << "[SwordfareGUI] Embedded main font loaded" << std::endl;
    } else if (!main_font_path.empty()) {
        float main_font_size = 14.0f * dpi_scale;
        m_font_main = io.Fonts->AddFontFromFileTTF(main_font_path.c_str(), main_font_size);

        // Merge FontAwesome into main font
        if (m_font_main && !fa_path.empty()) {
            static const ImWchar icon_ranges[] = { ICON_FA_MIN, ICON_FA_MAX, 0 };
            ImFontConfig icon_cfg;
            icon_cfg.MergeMode = true;
            icon_cfg.PixelSnapH = true;
            icon_cfg.GlyphMinAdvanceX = main_font_size;
            icon_cfg.GlyphOffset = ImVec2(0, 1);
            io.Fonts->AddFontFromFileTTF(fa_path.c_str(), main_font_size * 0.85f, &icon_cfg, icon_ranges);
        }
    } else {
        m_font_main = io.Fonts->AddFontDefault();
    }

    if (emb_button && emb_button_size > 0) {
        float button_font_size = 20.0f * dpi_scale;
        ImFontConfig emb_btn_cfg;
        emb_btn_cfg.FontDataOwnedByAtlas = false;
        m_font_button = io.Fonts->AddFontFromMemoryTTF((void*)emb_button, (int)emb_button_size,
                                                       button_font_size, &emb_btn_cfg);

        // Merge FontAwesome into button font
        if (m_font_button && emb_fa && emb_fa_size > 0) {
            static const ImWchar icon_ranges[] = { ICON_FA_MIN, ICON_FA_MAX, 0 };
            ImFontConfig icon_cfg;
            icon_cfg.MergeMode = true;
            icon_cfg.PixelSnapH = true;
            icon_cfg.GlyphMinAdvanceX = button_font_size;
            icon_cfg.GlyphOffset = ImVec2(0, 2);
            icon_cfg.FontDataOwnedByAtlas = false;
            io.Fonts->AddFontFromMemoryTTF((void*)emb_fa, (int)emb_fa_size,
                                           button_font_size * 0.85f, &icon_cfg, icon_ranges);
        }
    } else if (!button_font_path.empty()) {
        float button_font_size = 20.0f * dpi_scale;
        m_font_button = io.Fonts->AddFontFromFileTTF(button_font_path.c_str(), button_font_size);

        // Merge FontAwesome into button font
        if (m_font_button && !fa_path.empty()) {
            static const ImWchar icon_ranges[] = { ICON_FA_MIN, ICON_FA_MAX, 0 };
            ImFontConfig icon_cfg;
            icon_cfg.MergeMode = true;
            icon_cfg.PixelSnapH = true;
            icon_cfg.GlyphMinAdvanceX = button_font_size;
            icon_cfg.GlyphOffset = ImVec2(0, 2);
            io.Fonts->AddFontFromFileTTF(fa_path.c_str(), button_font_size * 0.85f, &icon_cfg, icon_ranges);
        }
    } else {
        m_font_button = io.Fonts->AddFontDefault();
    }

    // ── Tool typography: a real UI face + a real monospace face ─────────────
    //
    // m_font_mono was DECLARED but never assigned, so every `if (m_font_mono)
    // PushFont(...)` in the codebase was dead code and the console fell back to
    // m_font_main — which is Megalopolis Extra, Swordigo's display lettering
    // font.  A research tool rendering addresses and hex in a comic font is not
    // just ugly: fixed-width alignment is a correctness property for a hex dump.
    //
    // Space Grotesk for chrome and labels, JetBrains Mono for anything numeric.
    {
        // Local picker: this path does not have the find_font() helper the
        // Vulkan loader defines later in its own scope.
        auto pick_font = [](const char* const* names, size_t count) -> std::string {
            for (size_t i = 0; i < count; ++i)
                if (std::filesystem::exists(names[i])) return names[i];
            return {};
        };
        const char* ui_candidates[] = {
            "src/assets/fonts/SpaceGrotesk-VariableFont_wght.ttf",
            "src/assets/fonts/static/SpaceGrotesk-Regular.ttf",
            "src/assets/fonts/Inter-Regular.ttf",
            "/usr/share/ricasso/launcher/fonts/Inter-Regular.ttf"
        };
        const char* mono_candidates[] = {
            "src/assets/fonts/JetBrainsMono-VariableFont_wght.ttf",
            "src/assets/fonts/static/JetBrainsMono-Regular.ttf",
            "/usr/share/ricasso/launcher/fonts/JetBrainsMono-Regular.ttf"
        };
        std::string ui_font_path   = pick_font(ui_candidates,   4);
        std::string mono_font_path = pick_font(mono_candidates, 3);
        const unsigned char* emb_ui = nullptr;    size_t emb_ui_size = 0;
        const unsigned char* emb_mono = nullptr;  size_t emb_mono_size = 0;
        for (const char* n : { "fonts/SpaceGrotesk-VariableFont_wght.ttf",
                               "fonts/Inter-Regular.ttf" })
            if (embedded_asset(n, &emb_ui, &emb_ui_size)) break;
        if (!embedded_asset("fonts/JetBrainsMono-VariableFont_wght.ttf", &emb_mono, &emb_mono_size))
            embedded_asset("fonts/JetBrainsMono-Italic-VariableFont_wght.ttf", &emb_mono, &emb_mono_size);

        const float ui_sz   = 15.0f * dpi_scale;
        const float mono_sz = 14.0f * dpi_scale;
        ImFontConfig emb_ui_cfg;   emb_ui_cfg.FontDataOwnedByAtlas = false;
        ImFontConfig emb_mono_cfg; emb_mono_cfg.FontDataOwnedByAtlas = false;
        auto merge_fa_into = [&](float size) {
            static const ImWchar icon_ranges[] = { ICON_FA_MIN, ICON_FA_MAX, 0 };
            ImFontConfig cfg;
            cfg.MergeMode = true; cfg.PixelSnapH = true;
            cfg.GlyphMinAdvanceX = size; cfg.GlyphOffset = ImVec2(0, 1);
            cfg.FontDataOwnedByAtlas = false;
            if (emb_fa && emb_fa_size > 0)
                io.Fonts->AddFontFromMemoryTTF((void*)emb_fa, (int)emb_fa_size,
                                               size * 0.85f, &cfg, icon_ranges);
            else if (!fa_path.empty())
                io.Fonts->AddFontFromFileTTF(fa_path.c_str(), size * 0.85f, &cfg, icon_ranges);
        };

        m_font_ui = (emb_ui && emb_ui_size > 0)
            ? io.Fonts->AddFontFromMemoryTTF((void*)emb_ui, (int)emb_ui_size, ui_sz, &emb_ui_cfg)
            : (!ui_font_path.empty()
                   ? io.Fonts->AddFontFromFileTTF(ui_font_path.c_str(), ui_sz)
                   : io.Fonts->AddFontDefault());
        if (m_font_ui) merge_fa_into(ui_sz);

        m_font_mono = (emb_mono && emb_mono_size > 0)
            ? io.Fonts->AddFontFromMemoryTTF((void*)emb_mono, (int)emb_mono_size, mono_sz, &emb_mono_cfg)
            : (!mono_font_path.empty()
                   ? io.Fonts->AddFontFromFileTTF(mono_font_path.c_str(), mono_sz)
                   : io.Fonts->AddFontDefault());
        if (m_font_mono) merge_fa_into(mono_sz);

        std::cout << "[SwordfareGUI] Tool fonts: ui=" << (m_font_ui ? "ok" : "default")
                  << " mono=" << (m_font_mono ? "ok" : "default") << std::endl;
    }

    apply_swordfare_theme();

    // Scale ImGui styles according to layout scale rather than full physical DPI scale
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(layout_scale);

    // Apply global font scale to make rasterized high-res fonts display at the correct logical size
    io.FontGlobalScale = layout_scale / dpi_scale;

    ImGui_ImplSDL3_InitForOpenGL(window, gl_ctx);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    m_initialized = true;
}

// ---------------------------------------------------------------------------
// init_vulkan — same setup as init() but wires the Dear ImGui Vulkan backend
// ---------------------------------------------------------------------------
#ifdef VULKAN_BACKEND
void SwordfareGUI::init_vulkan(SDL_Window* window, VulkanBackend* vk_backend) {
    if (m_initialized) return;

    m_window        = window;
    m_vk_backend    = vk_backend;
    m_vulkan_active = true;

    // Dedicated ImGui context — does not share state with the launcher
    m_imgui_ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(m_imgui_ctx));

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    // DPI / layout scale (identical logic to init)
    float dpi_scale = 1.0f;
    int ww, wh, pw, ph;
    SDL_GetWindowSize(window, &ww, &wh);
    SDL_GetWindowSizeInPixels(window, &pw, &ph);
    if (ww > 0 && pw > 0) dpi_scale = (float)pw / (float)ww;
    if (dpi_scale < 1.0f) dpi_scale = 1.0f;

    float layout_scale = (wh >= 1440) ? 1.5f : (wh >= 1080) ? 1.25f : 1.0f;

    // Font candidates — same search paths as init()
    std::string home_dir = os_external::home_dir();

    auto find_font = [&](std::vector<std::string> candidates) -> std::string {
        if (!home_dir.empty()) {
            candidates.push_back(home_dir + "/.local/share/ricasso/launcher/fonts/MegalopolisExtra-Regular.otf");
            candidates.push_back(home_dir + "/.local/share/ricasso/src/assets/fonts/MegalopolisExtra-Regular.otf");
            candidates.push_back(home_dir + "/.local/share/ricasso/launcher/fonts/Inter-Regular.ttf");
            candidates.push_back(home_dir + "/.local/share/ricasso/src/assets/fonts/Inter-Regular.ttf");
        }
        for (const auto& fp : candidates)
            if (std::filesystem::exists(fp)) return fp;
        return {};
    };

    std::string main_font_path = find_font({
        "src/assets/fonts/MegalopolisExtra-Regular.otf",
        "src/assets/fonts/Redaction10-Regular.otf",
        "src/assets/fonts/Inter-Regular.ttf",
        "/usr/share/ricasso/launcher/fonts/MegalopolisExtra-Regular.otf",
        "/usr/share/ricasso/launcher/fonts/Inter-Regular.ttf"
    });
    std::string button_font_path = find_font({
        "src/assets/fonts/MegalopolisExtra-Regular.otf",
        "src/assets/fonts/Redaction10-Bold.otf",
        "src/assets/fonts/Inter-Regular.ttf",
        "/usr/share/ricasso/launcher/fonts/MegalopolisExtra-Regular.otf",
        "/usr/share/ricasso/launcher/fonts/Inter-Regular.ttf"
    });
    std::string fa_path = find_font({
        "src/assets/fontawesome/otfs/Font Awesome 7 Free-Solid-900.otf",
        "src/assets/fonts/fa-solid-900.ttf",
        "/usr/share/ricasso/launcher/fontawesome/otfs/Font Awesome 7 Free-Solid-900.otf",
        "/usr/share/ricasso/launcher/fonts/fa-solid-900.ttf"
    });

    // ── Embedded fonts FIRST (permanent fix) ──
    const unsigned char* emb_main = nullptr;    size_t emb_main_size = 0;
    const unsigned char* emb_button = nullptr;  size_t emb_button_size = 0;
    const unsigned char* emb_fa = nullptr;      size_t emb_fa_size = 0;
    const char* emb_main_names[] = {
        "fonts/MegalopolisExtra-Regular.otf", "fonts/Redaction10-Regular.otf",
        "fonts/Inter-Regular.ttf",
    };
    for (const char* n : emb_main_names)
        if (embedded_asset(n, &emb_main, &emb_main_size)) break;
    const char* emb_button_names[] = {
        "fonts/MegalopolisExtra-Regular.otf", "fonts/Redaction10-Bold.otf",
        "fonts/Inter-Regular.ttf",
    };
    for (const char* n : emb_button_names)
        if (embedded_asset(n, &emb_button, &emb_button_size)) break;
    if (!embedded_asset("fonts/fa-solid-900.ttf", &emb_fa, &emb_fa_size))
        embedded_asset("fonts/fa-solid-900.otf", &emb_fa, &emb_fa_size);

    // Load fonts (same logic as init — embedded first, disk fallback)
    // Embedded fonts are static .rodata — the atlas must never free() them.
    ImFontConfig emb_main_cfg;
    emb_main_cfg.FontDataOwnedByAtlas = false;
    ImFontConfig emb_btn_cfg;
    emb_btn_cfg.FontDataOwnedByAtlas = false;
    auto merge_fa = [&](float size) {
        static const ImWchar icon_ranges[] = { ICON_FA_MIN, ICON_FA_MAX, 0 };
        ImFontConfig cfg;
        cfg.MergeMode = true; cfg.PixelSnapH = true;
        cfg.GlyphMinAdvanceX = size; cfg.GlyphOffset = ImVec2(0, 1);
        cfg.FontDataOwnedByAtlas = false;
        if (emb_fa && emb_fa_size > 0)
            io.Fonts->AddFontFromMemoryTTF((void*)emb_fa, (int)emb_fa_size, size * 0.85f, &cfg, icon_ranges);
        else if (!fa_path.empty())
            io.Fonts->AddFontFromFileTTF(fa_path.c_str(), size * 0.85f, &cfg, icon_ranges);
    };

    float main_sz   = 14.0f * dpi_scale;
    float button_sz = 20.0f * dpi_scale;
    m_font_main   = (emb_main && emb_main_size > 0)
                        ? io.Fonts->AddFontFromMemoryTTF((void*)emb_main, (int)emb_main_size, main_sz, &emb_main_cfg)
                        : (!main_font_path.empty()
                            ? io.Fonts->AddFontFromFileTTF(main_font_path.c_str(), main_sz)
                            : io.Fonts->AddFontDefault());
    if (m_font_main) merge_fa(main_sz);
    m_font_button = (emb_button && emb_button_size > 0)
                        ? io.Fonts->AddFontFromMemoryTTF((void*)emb_button, (int)emb_button_size, button_sz, &emb_btn_cfg)
                        : (!button_font_path.empty()
                            ? io.Fonts->AddFontFromFileTTF(button_font_path.c_str(), button_sz)
                            : io.Fonts->AddFontDefault());
    if (m_font_button) merge_fa(button_sz);

    // ── Tool typography (same rationale as the GL path above) ───────────────
    // Kept in step with the first loader: a font slot that is only populated on
    // one of two paths is how m_font_mono ended up permanently null.
    std::string ui_font_path = find_font({
        "src/assets/fonts/SpaceGrotesk-VariableFont_wght.ttf",
        "src/assets/fonts/static/SpaceGrotesk-Regular.ttf",
        "src/assets/fonts/Inter-Regular.ttf",
        "/usr/share/ricasso/launcher/fonts/Inter-Regular.ttf"
    });
    std::string mono_font_path = find_font({
        "src/assets/fonts/JetBrainsMono-VariableFont_wght.ttf",
        "src/assets/fonts/static/JetBrainsMono-Regular.ttf",
        "/usr/share/ricasso/launcher/fonts/JetBrainsMono-Regular.ttf"
    });
    const unsigned char* emb_ui = nullptr;    size_t emb_ui_size = 0;
    const unsigned char* emb_mono = nullptr;  size_t emb_mono_size = 0;
    for (const char* n : { "fonts/SpaceGrotesk-VariableFont_wght.ttf",
                           "fonts/Inter-Regular.ttf" })
        if (embedded_asset(n, &emb_ui, &emb_ui_size)) break;
    if (!embedded_asset("fonts/JetBrainsMono-VariableFont_wght.ttf", &emb_mono, &emb_mono_size))
        embedded_asset("fonts/JetBrainsMono-Italic-VariableFont_wght.ttf", &emb_mono, &emb_mono_size);
    {
        const float ui_sz   = 15.0f * dpi_scale;
        const float mono_sz = 14.0f * dpi_scale;
        ImFontConfig emb_ui_cfg;   emb_ui_cfg.FontDataOwnedByAtlas = false;
        ImFontConfig emb_mono_cfg; emb_mono_cfg.FontDataOwnedByAtlas = false;
        m_font_ui = (emb_ui && emb_ui_size > 0)
            ? io.Fonts->AddFontFromMemoryTTF((void*)emb_ui, (int)emb_ui_size, ui_sz, &emb_ui_cfg)
            : (!ui_font_path.empty()
                   ? io.Fonts->AddFontFromFileTTF(ui_font_path.c_str(), ui_sz)
                   : io.Fonts->AddFontDefault());
        if (m_font_ui) merge_fa(ui_sz);
        m_font_mono = (emb_mono && emb_mono_size > 0)
            ? io.Fonts->AddFontFromMemoryTTF((void*)emb_mono, (int)emb_mono_size, mono_sz, &emb_mono_cfg)
            : (!mono_font_path.empty()
                   ? io.Fonts->AddFontFromFileTTF(mono_font_path.c_str(), mono_sz)
                   : io.Fonts->AddFontDefault());
        if (m_font_mono) merge_fa(mono_sz);
        std::cout << "[SwordfareGUI] Tool fonts: ui=" << (m_font_ui ? "ok" : "default")
                  << " mono=" << (m_font_mono ? "ok" : "default") << std::endl;
    }

    apply_swordfare_theme();
    ImGui::GetStyle().ScaleAllSizes(layout_scale);
    io.FontGlobalScale = layout_scale / dpi_scale;

    // SDL3 Vulkan platform layer
    ImGui_ImplSDL3_InitForVulkan(window);

    // Initialize ImGui Vulkan functions with volk
    ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_0, [](const char* function_name, void* user_data) {
        return vkGetInstanceProcAddr((VkInstance)user_data, function_name);
    }, vk_backend->get_instance());

    // Create ImGui-specific descriptor pool
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER,                1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       1000 }
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets       = 1000 * 11;
    pool_info.poolSizeCount = 11;
    pool_info.pPoolSizes    = pool_sizes;
    if (vkCreateDescriptorPool(vk_backend->get_device(), &pool_info, nullptr,
                               &m_imgui_vk_descriptor_pool) != VK_SUCCESS) {
        fprintf(stderr, "[SwordfareGUI] Failed to create ImGui Vulkan descriptor pool\n");
    }

    // Wire ImGui Vulkan backend
    ImGui_ImplVulkan_InitInfo vk_init = {};
    vk_init.ApiVersion                         = VK_API_VERSION_1_0;
    vk_init.Instance                           = vk_backend->get_instance();
    vk_init.PhysicalDevice                     = vk_backend->get_physical_device();
    vk_init.Device                             = vk_backend->get_device();
    vk_init.QueueFamily                        = vk_backend->get_graphics_family();
    vk_init.Queue                              = vk_backend->get_graphics_queue();
    vk_init.DescriptorPool                     = m_imgui_vk_descriptor_pool;
    vk_init.MinImageCount                      = vk_backend->get_image_count();
    vk_init.ImageCount                         = vk_backend->get_image_count();
    vk_init.PipelineCache                      = vk_backend->get_pipeline_cache();
    vk_init.PipelineInfoMain.RenderPass        = vk_backend->get_render_pass();
    vk_init.PipelineInfoMain.MSAASamples       = VK_SAMPLE_COUNT_1_BIT;
    ImGui_ImplVulkan_Init(&vk_init);

    m_initialized = true;
}
#else
void SwordfareGUI::init_vulkan(SDL_Window* window, class VulkanBackend*) {
    // Vulkan backend not compiled — fall back to OpenGL
    fprintf(stderr, "[SwordfareGUI] init_vulkan called but VULKAN_BACKEND not compiled — no-op\n");
    (void)window;
}
#endif

void SwordfareGUI::shutdown() {
    if (!m_initialized) return;
    stop_tcp_server();
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(m_imgui_ctx));
#ifdef VULKAN_BACKEND
    if (m_vulkan_active) {
        ImGui_ImplVulkan_Shutdown();
        if (m_imgui_vk_descriptor_pool != VK_NULL_HANDLE && m_vk_backend) {
            vkDestroyDescriptorPool(m_vk_backend->get_device(), m_imgui_vk_descriptor_pool, nullptr);
            m_imgui_vk_descriptor_pool = VK_NULL_HANDLE;
        }
    } else
#endif
    {
        ImGui_ImplOpenGL3_Shutdown();
    }
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext(static_cast<ImGuiContext*>(m_imgui_ctx));

    m_imgui_ctx   = nullptr;
    m_initialized = false;
}

// ---------------------------------------------------------------------------
// Event processing
// ---------------------------------------------------------------------------

bool SwordfareGUI::process_event(const SDL_Event& event) {
    if (!m_initialized) return false;
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(m_imgui_ctx));
    ImGui_ImplSDL3_ProcessEvent(&event);
    
    ImGuiIO& io = ImGui::GetIO();

    // Nothing drawn here has keyboard controls: keys always reach the hotkeys and the game.
    if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) return false;

    // For mouse events: block if ImGui wants it OR if the click lands on any
    // SRE overlay or button area (even when the mod-menu is hidden)
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        if (io.WantCaptureMouse) return true;
        if (event.button.button == SDL_BUTTON_LEFT) {
            float ex = (float)event.button.x;
            float ey = (float)event.button.y;
            if (is_input_blocked(ex, ey)) return true;
        }
        return false;
    }
    return false;
}

// ---------------------------------------------------------------------------
// begin_frame / end_frame
// ---------------------------------------------------------------------------

void SwordfareGUI::update_console_backend() {
    if (!m_console_ready || !m_guest_memory) return;

    // ---- Guest console IPC: DELIVER first, DISPATCH second, never both ----
    //
    // The guest console is a SINGLE-SLOT, cross-thread mailbox:
    //   host: writes buf, sets status=0 then pending=1   (submit)
    //   guest: sees pending==1, runs it, writes result, sets status(1|2),
    //          clears pending=0                            (complete)
    //   host: sees status!=0, reads result, sets status=0 (consume)
    //
    // The original rewrite dispatched a NEW command at the TOP of this tick and
    // read the result at the BOTTOM of the SAME tick. Because console_submit()
    // unconditionally writes status=0, a dispatch could race ahead and WIPE a
    // completion the host had not consumed yet — so most results were silently
    // dropped and the survivors appeared one command late (the desync + the
    // ">>"-flood we observed on the wire).
    //
    // Host-only fix (no guest/Raijin change): make DELIVERY and DISPATCH
    // mutually exclusive per tick, with delivery ALWAYS winning. We only ever
    // submit the next chunk on a tick where the slot is fully idle
    // (pending==0 && status==0) AND we did not just consume a result. That
    // closes the check-then-act window: a pending completion is always read
    // and cleared on its own tick before the slot can be reused.
    bool delivered_this_tick = false;

    // ---- Unified SRE command result polling (Local GUI + TCP Console) ----
    int32_t status = *(int32_t*)(m_guest_memory + m_console_status_addr);
    if (status != 0) {
        const char* result = (const char*)(m_guest_memory + m_console_result_addr);
        std::string res_str = (result && result[0]) ? result : "";

        // Feed to GUI console history & terminal log
        if (!res_str.empty()) {
            m_console_history.push_back({res_str, (status == 2), false});
            while ((int)m_console_history.size() > CONSOLE_MAX_HISTORY)
                m_console_history.erase(m_console_history.begin());

            // Print output to main terminal for easy copying/logging
            if (status == 2) {
                std::cout << "[SRE-Lua Error] " << res_str << std::endl;
            } else {
                std::cout << "[SRE-Lua Output]\n" << res_str << std::endl;
            }
        }

        // If the command was sent by TCP, send the result back to the client
        // that OWNS it (m_tcp_in_flight_fd), not whatever client happens to be
        // connected now — a slow result must never leak to a newer client.
        // Then clear the in-flight flag and wake the reader thread (which may
        // be blocked in enqueue_wait keeping output strictly ordered).
        bool was_in_flight;
        int  owner_fd;
        uint64_t owner_gen;
        uint64_t cur_gen;
        {
            std::lock_guard<std::mutex> lk(m_tcp_mutex);
            was_in_flight = m_tcp_cmd_in_flight;
            owner_fd      = m_tcp_in_flight_fd;
            owner_gen     = m_tcp_in_flight_gen;
            cur_gen       = m_tcp_client_gen;
        }

        if (m_tcp_running && was_in_flight) {
            // Only deliver if the owning client is still the connected one:
            // BOTH the fd matches AND the connection generation matches, so a
            // result computed for a client that has since disconnected (and had
            // its fd possibly recycled by a new client) is never written to the
            // wrong client.
            if (owner_fd >= 0 && owner_fd == m_tcp_client_fd.load() &&
                owner_gen == cur_gen) {
                std::string output;
                if (status == 2) {
                    // Output error in red
                    output = "\033[1;31mError: " + res_str + "\033[0m\n\033[1;36mraijin-sdk\033[0m> ";
                } else {
                    // Normal execution output
                    output = res_str.empty() ? "\033[1;36mraijin-sdk\033[0m> " : (res_str + "\n\033[1;36mraijin-sdk\033[0m> ");
                }
                write(owner_fd, output.c_str(), output.size());
            }

            {
                std::lock_guard<std::mutex> lk(m_tcp_mutex);
                m_tcp_cmd_in_flight = false;
                m_tcp_in_flight_fd  = -1;
                m_tcp_completed_seq++;
            }
            m_tcp_done_cv.notify_all();
        }

        *(int32_t*)(m_guest_memory + m_console_status_addr) = 0;
        delivered_this_tick = true;
    }

    // ---- Dispatch the next queued TCP chunk (ONLY if we did not just consume
    //      a result this tick). Popping and submitting are done under the same
    //      lock, and we re-check the guest slot is fully idle right before
    //      console_submit() so we can never clobber an unread completion. This
    //      strict deliver-then-dispatch ordering is the host-only fix for the
    //      dropped-result / one-behind desync seen on the wire. ----
    if (m_tcp_running && !delivered_this_tick) {
        std::unique_lock<std::mutex> lk(m_tcp_mutex);
        if (!m_tcp_cmd_in_flight && !m_tcp_cmd_queue.empty()) {
            int32_t pending = *(int32_t*)(m_guest_memory + m_console_pending_addr);
            int32_t st      = *(int32_t*)(m_guest_memory + m_console_status_addr);
            if (pending == 0 && st == 0) {
                TcpCmd item = m_tcp_cmd_queue.front();
                m_tcp_cmd_queue.pop_front();
                m_tcp_cmd_in_flight = true;
                m_tcp_in_flight_fd  = item.client_fd;
                m_tcp_in_flight_gen = item.gen;
                lk.unlock();
                console_submit(item.code);
            }
        }
    }
}

void SwordfareGUI::begin_frame() {
    if (!m_initialized) return;

    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(m_imgui_ctx));
#ifdef VULKAN_BACKEND
    if (m_vulkan_active) {
        ImGui_ImplVulkan_NewFrame();
    } else
#endif
    {
        ImGui_ImplOpenGL3_NewFrame();
    }
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void SwordfareGUI::end_frame() {
    if (!m_initialized) return;
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(m_imgui_ctx));
    ImGui::Render();
#ifdef VULKAN_BACKEND
    if (m_vulkan_active && m_vk_backend) {
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_vk_backend->get_current_command_buffer());
    } else
#endif
    {
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
}

// ---------------------------------------------------------------------------
// draw_debug — the F3 debug window
// ---------------------------------------------------------------------------

void SwordfareGUI::draw_debug(const SwordfareDebugStats& st) {
    // -- Update FPS ring buffer --
    swardfare_push_fps(m_fps_history, m_fps_idx, st.fps, FPS_HISTORY);

    ImGuiIO& io = ImGui::GetIO();
    float screen_w = io.DisplaySize.x;
    float screen_h = io.DisplaySize.y;

    // Unbound fullscreen overlay (no rigid borders, no clamping box, completely transparent canvas)
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(screen_w, screen_h), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));

    ImGuiWindowFlags wflags =
        ImGuiWindowFlags_NoTitleBar            |
        ImGuiWindowFlags_NoResize              |
        ImGuiWindowFlags_NoMove                |
        ImGuiWindowFlags_NoScrollbar           |
        ImGuiWindowFlags_NoScrollWithMouse     |
        ImGuiWindowFlags_NoCollapse            |
        ImGuiWindowFlags_NoNav                 |
        ImGuiWindowFlags_NoSavedSettings       |
        ImGuiWindowFlags_NoFocusOnAppearing    |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        // THIS OVERLAY IS FULL-SCREEN AND READ-ONLY, SO IT MUST NOT WANT INPUT.
        // Without NoInputs ImGui sets io.WantCaptureMouse for the whole screen
        // while F3 is up, and main.cpp's click router feeds the mouse to the HUD
        // instead of the game - which reads as "the game is unclickable with F3
        // open". NoInputs also removes it from keyboard capture, which is what a
        // read-only debug readout wants.
        ImGuiWindowFlags_NoInputs;

    if (!ImGui::Begin("##MinecraftF3DebugOverlay", nullptr, wflags)) {
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        return;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    float font_size = ImGui::GetFontSize();
    float line_height = font_size + 3.0f;

    // Subtle top accent band so the HUD reads as a designed surface, not raw text.
    dl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(screen_w, 24.0f),
                                IM_COL32(20, 40, 60, 110), IM_COL32(50, 22, 34, 110),
                                IM_COL32(50, 22, 34, 0),   IM_COL32(20, 40, 60, 0));

    // Helper to draw a left-aligned line: rounded dark badge + cyan accent stripe.
    float left_x = 10.0f;
    float left_y = 10.0f;
    auto draw_left = [&](const char* text, ImU32 text_col = IM_COL32(224, 224, 224, 255)) {
        if (!text || !text[0]) {
            left_y += 5.0f;
            return;
        }
        ImVec2 tsz = ImGui::CalcTextSize(text);
        dl->AddRectFilled(
            ImVec2(left_x - 5.0f, left_y - 1.0f),
            ImVec2(left_x + tsz.x + 5.0f, left_y + tsz.y + 1.0f),
            IM_COL32(12, 14, 18, 170), 3.0f
        );
        dl->AddRectFilled(
            ImVec2(left_x - 5.0f, left_y - 1.0f),
            ImVec2(left_x - 3.0f, left_y + tsz.y + 1.0f),
            IM_COL32(0, 170, 220, 170), 1.0f
        );
        dl->AddText(ImVec2(left_x + 1.0f, left_y + 1.0f), IM_COL32(20, 20, 20, 220), text);
        dl->AddText(ImVec2(left_x, left_y), text_col, text);
        left_y += line_height;
    };

    // Helper to draw a Minecraft-style right-aligned line with semi-transparent badge & text shadow
    float right_margin = 10.0f;
    float right_y = 10.0f;
    auto draw_right = [&](const char* text, ImU32 text_col = IM_COL32(224, 224, 224, 255)) {
        if (!text || !text[0]) {
            right_y += 5.0f;
            return;
        }
        ImVec2 tsz = ImGui::CalcTextSize(text);
        float rx = screen_w - right_margin - tsz.x;
        dl->AddRectFilled(
            ImVec2(rx - 5.0f, right_y - 1.0f),
            ImVec2(rx + tsz.x + 5.0f, right_y + tsz.y + 1.0f),
            IM_COL32(12, 14, 18, 170), 3.0f
        );
        dl->AddRectFilled(
            ImVec2(rx + tsz.x + 3.0f, right_y - 1.0f),
            ImVec2(rx + tsz.x + 5.0f, right_y + tsz.y + 1.0f),
            IM_COL32(139, 61, 255, 170), 1.0f
        );
        dl->AddText(ImVec2(rx + 1.0f, right_y + 1.0f), IM_COL32(20, 20, 20, 220), text);
        dl->AddText(ImVec2(rx, right_y), text_col, text);
        right_y += line_height;
    };

    char buf[512];

    // ── LEFT SIDE (Minecraft F3 style) ───────────────────────────────────────
    // 1. Version header
    bool is_v13 = (strstr(st.binary_name, "1.4.13") != nullptr);
    snprintf(buf, sizeof(buf), "Swordigo %s (%s / x86_64 host)",
             (is_v13 ? "1.4.13" : "1.4.12"), "swordfare_boot");
    draw_left(buf, IM_COL32(255, 255, 255, 255));

    // 2. FPS line (coloured by performance: green >= 55, amber >= 30, red < 30)
    ImU32 fps_col = (st.fps >= 55.0f) ? IM_COL32(85, 255, 85, 255)
                  : (st.fps >= 30.0f) ? IM_COL32(255, 200, 50, 255)
                                      : IM_COL32(255, 85, 85, 255);
    snprintf(buf, sizeof(buf), "%.1f fps (%.2f ms) T: 60 vsync, speed: %s%s",
             st.fps, (st.fps > 0.0f ? 1000.0f / st.fps : 0.0f),
             st.speed_label, st.game_paused ? " [PAUSED]" : "");
    draw_left(buf, fps_col);

    // 3. Engine & Runtime details
    snprintf(buf, sizeof(buf), "Engine: ARM64 Dynarmic JIT | RenderGuard: 0x0 - 0x3000000");
    draw_left(buf, IM_COL32(210, 210, 210, 255));

    snprintf(buf, sizeof(buf), "Runtime: %s @ 0x2000000 | ABI %d | API: %s",
             (is_v13 ? "libsre13.so" : "libsre12.so"),
             (is_v13 ? 13 : 12),
             st.graphics_api);
    draw_left(buf, IM_COL32(210, 210, 210, 255));

    draw_left(""); // spacer

    // 4. LOADED MOD INFOS (user-requested)
    std::string active_mod = get_active_mod_name();
    if (!active_mod.empty()) {
        snprintf(buf, sizeof(buf), "[Active Mod] %s", active_mod.c_str());
        draw_left(buf, IM_COL32(255, 170, 0, 255)); // Gold / amber header

        std::string mods_dir = get_user_data_dir() + "mods";
        auto mods = modman::list_mods(mods_dir);
        const modman::ModMeta* meta = nullptr;
        for (const auto& m : mods) {
            if (m.id == active_mod) {
                meta = &m;
                break;
            }
        }
        if (meta) {
            snprintf(buf, sizeof(buf), "  Name: %s (v%s)",
                     meta->name.c_str(),
                     meta->version.empty() ? "1.0" : meta->version.c_str());
            draw_left(buf, IM_COL32(85, 255, 255, 255)); // Cyan

            snprintf(buf, sizeof(buf), "  Author: %s | Category: %s",
                     meta->author.empty() ? "Community" : meta->author.c_str(),
                     meta->category.empty() ? "General" : meta->category.c_str());
            draw_left(buf, IM_COL32(180, 220, 240, 255));
        }

        // Native mod libraries
        auto mod_libs = get_loaded_guest_mod_libs();
        if (!mod_libs.empty()) {
            std::string libs_str;
            for (size_t i = 0; i < mod_libs.size(); ++i) {
                if (i > 0) libs_str += ", ";
                libs_str += mod_libs[i];
            }
            snprintf(buf, sizeof(buf), "  Native Libs: %s (ARM64 .so injected)", libs_str.c_str());
            draw_left(buf, IM_COL32(85, 255, 85, 255)); // Green
        } else {
            draw_left("  Native Libs: None (Resource-only mod)", IM_COL32(160, 160, 160, 255));
        }

        // VFS Hierarchy status
        snprintf(buf, sizeof(buf), "  VFS Hierarchy: Active (mods/%s/resources -> base)", active_mod.c_str());
        draw_left(buf, IM_COL32(180, 220, 240, 255));

        // Mod Load Order list from launcher.toml (cached, refreshed at most every 2s)
        static LauncherConfig s_cached_lcfg;
        static double s_last_lcfg_check = -10.0;
        double cur_time = ImGui::GetTime();
        if (cur_time - s_last_lcfg_check > 2.0) {
            s_cached_lcfg = launcher_config_load();
            s_last_lcfg_check = cur_time;
        }
        const LauncherConfig& lcfg = s_cached_lcfg;

        if (!lcfg.mod_load_order.empty()) {
            std::string order_str = "[";
            for (size_t i = 0; i < lcfg.mod_load_order.size(); ++i) {
                if (i > 0) order_str += ", ";
                order_str += lcfg.mod_load_order[i];
            }
            order_str += "]";
            snprintf(buf, sizeof(buf), "  Load Order: %s", order_str.c_str());
            draw_left(buf, IM_COL32(180, 180, 180, 255));
        }
    } else {
        draw_left("[Active Mod] None (Vanilla base mode)", IM_COL32(255, 170, 0, 255));
        static LauncherConfig s_cached_vanilla_lcfg;
        static double s_last_vanilla_check = -10.0;
        double cur_time = ImGui::GetTime();
        if (cur_time - s_last_vanilla_check > 2.0) {
            s_cached_vanilla_lcfg = launcher_config_load();
            s_last_vanilla_check = cur_time;
        }
        const LauncherConfig& lcfg = s_cached_vanilla_lcfg;
        if (!lcfg.mod_load_order.empty()) {
            std::string order_str = "[";
            for (size_t i = 0; i < lcfg.mod_load_order.size(); ++i) {
                if (i > 0) order_str += ", ";
                order_str += lcfg.mod_load_order[i];
            }
            order_str += "]";
            snprintf(buf, sizeof(buf), "  Config Order: %s (inactive)", order_str.c_str());
            draw_left(buf, IM_COL32(160, 160, 160, 255));
        }
    }

    draw_left(""); // spacer

    // 5. XYZ & Block position (Minecraft style)
    snprintf(buf, sizeof(buf), "XYZ: %.3f / %.3f / %.3f", st.hero_x, st.hero_y, st.hero_z);
    draw_left(buf, IM_COL32(255, 255, 255, 255));

    snprintf(buf, sizeof(buf), "Block: %d  %d  %d", (int)st.hero_x, (int)st.hero_y, (int)st.hero_z);
    draw_left(buf, IM_COL32(210, 210, 210, 255));

    snprintf(buf, sizeof(buf), "Camera: (%.1f, %.1f, %.1f) zoom: %.2fx  focus: %s",
             st.cam_x, st.cam_y, st.cam_z, st.cam_zoom,
             st.cam_active ? "Overridden" : "Hero Follow");
    draw_left(buf, IM_COL32(210, 210, 210, 255));

    // 6. Draw statistics & PostFX
    snprintf(buf, sizeof(buf), "Draw Calls: %d | Vertices: %d | Tex Binds: %d | States: %d",
             st.draw_calls, st.vertices, st.tex_binds, st.state_changes);
    draw_left(buf, IM_COL32(180, 220, 240, 255));

    snprintf(buf, sizeof(buf), "PostFX: %s (%s) | Scaling: %s",
             st.postfx_on ? "ON" : "OFF", st.postfx_preset, st.scale_mode);
    draw_left(buf, st.postfx_on ? IM_COL32(255, 200, 85, 255) : IM_COL32(160, 160, 160, 255));

    // ── RIGHT SIDE (Minecraft F3 style, right-aligned) ────────────────────────
    // 1. GPU & Hardware
    static std::string s_gl_renderer;
    static std::string s_gl_version;
    if (s_gl_renderer.empty() && !m_vulkan_active) {
        const char* rend = (const char*)glGetString(GL_RENDERER);
        if (rend) s_gl_renderer = rend;
        const char* ver = (const char*)glGetString(GL_VERSION);
        if (ver) s_gl_version = ver;
    }

    if (!s_gl_renderer.empty()) {
        snprintf(buf, sizeof(buf), "%s", s_gl_renderer.c_str());
        draw_right(buf, IM_COL32(255, 255, 255, 255));
    } else {
        snprintf(buf, sizeof(buf), "Graphics: %s", st.graphics_api);
        draw_right(buf, IM_COL32(255, 255, 255, 255));
    }
    if (!s_gl_version.empty()) {
        snprintf(buf, sizeof(buf), "GL: %s", s_gl_version.c_str());
        draw_right(buf, IM_COL32(200, 200, 200, 255));
    }

    // 2. Display Resolution
    snprintf(buf, sizeof(buf), "Display: %dx%d (Draw: %dx%d)",
             st.win_w, st.win_h, st.draw_w, st.draw_h);
    draw_right(buf, IM_COL32(200, 200, 200, 255));

    draw_right(""); // spacer

    // 3. Memory & GC (Minecraft style: "Mem: XX% XXX/XXXMB")
    auto& rgc = RedstellGC::instance();
    uint32_t ghs64 = get_guest_heap_size_64();
    uint32_t ghs32 = get_guest_heap_size_32();
    uint32_t guest_heap = ghs64 + ghs32;
    float guest_heap_mb = (float)guest_heap / (1024.0f * 1024.0f);
    float host_ram_mb = (float)rgc.get_current_ram() / (1024.0f * 1024.0f);
    float peak_ram_mb = (float)rgc.get_peak_ram() / (1024.0f * 1024.0f);

    int guest_pct = (int)((guest_heap_mb / 128.0f) * 100.0f);
    if (guest_pct > 100) guest_pct = 100;
    snprintf(buf, sizeof(buf), "Guest Heap: %d%%  %.1f/128MB", guest_pct, guest_heap_mb);
    draw_right(buf, IM_COL32(85, 255, 85, 255)); // Green

    snprintf(buf, sizeof(buf), "Host RAM: %.1f MB (Peak: %.1f MB)", host_ram_mb, peak_ram_mb);
    draw_right(buf, IM_COL32(200, 200, 200, 255));

    snprintf(buf, sizeof(buf), "RGC: %.1f%% frag | Alloc: %lu/s | Free: %lu/s",
             rgc.get_fragmentation_estimate() * 100.0f,
             (unsigned long)rgc.get_alloc_rate(),
             (unsigned long)rgc.get_free_rate());
    draw_right(buf, IM_COL32(180, 220, 240, 255));

    snprintf(buf, sizeof(buf), "Resources: %lu (Tex: %u, POD: %u)",
             (unsigned long)rgc.get_resource_count(),
             rgc.get_texture_count(), rgc.get_pod_count());
    draw_right(buf, IM_COL32(180, 220, 240, 255));

    // ── SRE13 texture identity (REMOVABLE TEST SURFACE) ─────────────────────
    //   Answers "does the host know which texture is bound" from measurement.
    //   The diagnostic value is in the RELATIONSHIP between the two lines:
    //     * identified == 0 while binds climbs  -> the Caver::Texture offsets in
    //       sre13/hooks/texture_layout.h do not match this build. A broken
    //       layout otherwise looks identical to a working one that found
    //       nothing, which is why this is on F3 rather than in a log.
    //     * unique > 0 but trusted == 0        -> the channel works but the
    //       mapper's trust gate is refusing every name.
    //   Delete this block, the extern above, and sre13_tex_telemetry_snapshot
    //   once texture identity no longer needs proving.
    {
        uint32_t tt[6] = {0, 0, 0, 0, 0, 0};
        sre13_tex_telemetry_snapshot(tt);
        const uint32_t t_binds = tt[0], t_ident = tt[1], t_uniq = tt[2];
        const uint32_t t_trust = tt[3], t_rej = tt[4], t_level = tt[5];

        // Red when the channel is receiving binds but identifying none: that is
        // the broken-offset case and it must not look like success.
        const bool broken = (t_binds > 0u && t_ident == 0u);
        const ImU32 col = broken ? IM_COL32(255, 90, 90, 255)
                                 : (t_ident > 0u ? IM_COL32(150, 255, 170, 255)
                                                 : IM_COL32(150, 150, 150, 255));
        snprintf(buf, sizeof(buf), "TexID: %u of %u binds named", t_ident, t_binds);
        draw_right(buf, col);

        snprintf(buf, sizeof(buf), "TexID: %u distinct  L%u  gate +%u/-%u",
                 t_uniq, t_level, t_trust, t_rej);
        draw_right(buf, IM_COL32(180, 220, 240, 255));
    }

    // ── Authored-emitter bloom key (REMOVABLE TEST SURFACE) ─────────────
    //   Which emitters the bloom extract is being keyed on, in the resolved
    //   biome. "no biome resolved" is the failure that looks like success in the
    //   picture: an empty palette and a wrong threshold both produce no bloom.
    //   Delete this block, the extern above, and swordigo_bloom_emitter_report
    //   in fbo_scaler.cpp when the key no longer needs proving.
    {
        extern PostFXState g_postfx;   // defined in the main translation unit
        char ebuf[128];
        swordigo_bloom_emitter_report(ebuf, sizeof(ebuf));
        const bool keyed = g_postfx.bloom_emitter_key;
        snprintf(buf, sizeof(buf), "Bloom key: %s", keyed ? "authored emitters" : "luma ONLY");
        draw_right(buf, keyed ? IM_COL32(255, 200, 120, 255) : IM_COL32(255, 120, 120, 255));
        snprintf(buf, sizeof(buf), "  %s", ebuf);
        draw_right(buf, IM_COL32(200, 180, 140, 255));
    }

    draw_right(""); // spacer

    // 4. Subsystems
    snprintf(buf, sizeof(buf), "OpenAL Channels: %u | Open Files: %u",
             rgc.get_openal_count(), rgc.get_open_file_count());
    draw_right(buf, IM_COL32(180, 180, 180, 255));

    snprintf(buf, sizeof(buf), "Mouse: %d, %d | Frame: %d",
             st.mouse_x, st.mouse_y, st.frame_count);
    draw_right(buf, IM_COL32(180, 180, 180, 255));

    if (st.typing_mode) {
        draw_right("[TYPING MODE ACTIVE]", IM_COL32(255, 85, 85, 255));
    }

    // ── BOTTOM-LEFT: Minecraft-style Frame Time Sparkline ────────────────────
    float graph_w = 240.0f;
    float graph_h = 42.0f;
    float graph_x = 10.0f;
    float graph_y = screen_h - graph_h - 24.0f;

    if (graph_y > left_y + 10.0f) {
        // Dark background plate
        dl->AddRectFilled(
            ImVec2(graph_x - 3.0f, graph_y - 14.0f),
            ImVec2(graph_x + graph_w + 3.0f, graph_y + graph_h + 3.0f),
            IM_COL32(0, 0, 0, 150)
        );
        dl->AddText(ImVec2(graph_x, graph_y - 13.0f), IM_COL32(200, 200, 200, 255), "Frame Time (Target: 60 fps / 16.6ms)");

        // 60fps guide line
        float line_60_y = graph_y + graph_h * (1.0f - (60.0f / 80.0f));
        dl->AddLine(ImVec2(graph_x, line_60_y), ImVec2(graph_x + graph_w, line_60_y), IM_COL32(85, 255, 85, 120), 1.0f);

        // Frame bars
        float bar_w = graph_w / (float)FPS_HISTORY;
        for (int i = 0; i < FPS_HISTORY; ++i) {
            int idx = (m_fps_idx + i) % FPS_HISTORY;
            float f = m_fps_history[idx];
            if (f < 0.0f) f = 0.0f;
            if (f > 80.0f) f = 80.0f;
            float bar_h = (f / 80.0f) * graph_h;
            ImU32 bcol = (f >= 55.0f) ? IM_COL32(85, 255, 85, 200)
                       : (f >= 30.0f) ? IM_COL32(255, 200, 50, 200)
                                      : IM_COL32(255, 85, 85, 200);
            dl->AddRectFilled(
                ImVec2(graph_x + i * bar_w, graph_y + graph_h - bar_h),
                ImVec2(graph_x + (i + 1) * bar_w - 1.0f, graph_y + graph_h),
                bcol
            );
        }
    }

    // Bottom-center quick keybind reminder
    {
        const char* hint = "F1:Menu  F2:Controls  F3:Debug  F4:Cheats  F5:Camera  F6:Graphics  F11:Scenes  F12:Fullscreen";
        ImVec2 hsz = ImGui::CalcTextSize(hint);
        float hx = (screen_w - hsz.x) * 0.5f;
        float hy = screen_h - hsz.y - 8.0f;
        dl->AddRectFilled(
            ImVec2(hx - 4.0f, hy - 2.0f),
            ImVec2(hx + hsz.x + 4.0f, hy + hsz.y + 2.0f),
            IM_COL32(0, 0, 0, 130)
        );
        dl->AddText(ImVec2(hx, hy), IM_COL32(180, 180, 180, 220), hint);
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

// ---------------------------------------------------------------------------
// Custom Button Array structures (matches SreBtnSlot in sre_mini_api.c)
// ---------------------------------------------------------------------------
#define SRE_BTN_MAX       128
#define SRE_BTN_ID_LEN    32
#define SRE_BTN_LABEL_LEN 64

struct SreBtnSlot {
    char     id[SRE_BTN_ID_LEN];       /* Lua string ID */
    char     label[SRE_BTN_LABEL_LEN]; /* Display text */
    float    x, y;                      /* Normalized position (0-1) */
    float    w, h;                      /* Normalized dimensions (base-relative) */
    float    alpha;                     /* Overall alpha 0-255 */
    float    scale_x, scale_y;         /* Scaling factors */
    int      text_color;               /* Packed ARGB */
    float    text_scale;               /* Text size multiplier */
    int      bg_alpha;                 /* Background alpha (0-255) */
    int      hidden;                   /* Per-button hidden flag */
    int      clickable;                /* Whether it accepts clicks */
    int      movable;                  /* Can be dragged */
    int      snapback;                 /* Returns to original pos on release */
    float    home_x, home_y;           /* Original position (for snapback) */
    int      padding_l, padding_t, padding_r, padding_b;
    int      alignment;                /* Text alignment / gravity */
    char     overlay_id[SRE_BTN_ID_LEN]; /* Belongs to overlay */
    int      confined;                  /* Confined to overlay bounds */
    /* ---- STATE (written by host, read by SRE) ---- */
    volatile int pressed;              /* Host writes: 1=down, 0=up */
    volatile int released;             /* Host writes: 1 on release */
    volatile int dragging;             /* Host writes: 1=dragging, 0=not */
    volatile float cur_x, cur_y;       /* Current position (after drag) */
    int      active;                   /* 1 = slot in use, 0 = free */
    int      dirty;                    /* 1 = needs visual update by host */
};

#define SRE_OVERLAY_MAX 8
struct SreOverlaySlot {
    char     id[SRE_BTN_ID_LEN];
    float    x, y;
    float    w, h;
    int      bg_color;
    int      bg_alpha;
    float    corner_radius;
    int      hidden;
    int      movable;
    int      pinchable;
    float    scale_factor;
    int      pinching;
    /* Separators inside overlay */
    float    separators[8];
    int      separator_count;
    int      active;
    int      dirty;
};

void SwordfareGUI::draw_buttons(void* guest_buttons_ptr, void* guest_overlays_ptr, bool globally_hidden) {
    m_last_buttons_ptr = guest_buttons_ptr;
    m_last_overlays_ptr = guest_overlays_ptr;
    m_buttons_globally_hidden = globally_hidden;
    g_sre_overlay_blocking = false;
    
    // Diagnostic log
    static int log_ticks = 0;
    if (guest_buttons_ptr && log_ticks++ % 60 == 0) {
        FILE* f = fopen("sre_gui_debug.log", "w");
        if (f) {
            SreOverlaySlot* ovrs = static_cast<SreOverlaySlot*>(guest_overlays_ptr);
            SreBtnSlot* btns = static_cast<SreBtnSlot*>(guest_buttons_ptr);
            fprintf(f, "=== Overlays ===\n");
            if (ovrs) {
                for (int i = 0; i < SRE_OVERLAY_MAX; i++) {
                    if (ovrs[i].active) {
                        fprintf(f, "Slot %d: id='%s' active=%d hidden=%d x=%.3f y=%.3f w=%.3f h=%.3f bg_color=0x%08X bg_alpha=%d corner_radius=%.3f scale=%.3f\n",
                                i, ovrs[i].id, ovrs[i].active, ovrs[i].hidden,
                                ovrs[i].x, ovrs[i].y, ovrs[i].w, ovrs[i].h,
                                (unsigned int)ovrs[i].bg_color, ovrs[i].bg_alpha, ovrs[i].corner_radius, ovrs[i].scale_factor);
                    } else {
                        fprintf(f, "Slot %d: id='%s' active=0\n", i, ovrs[i].id);
                    }
                }
            } else {
                fprintf(f, "overlays is NULL\n");
            }
            fprintf(f, "\n=== Buttons ===\n");
            for (int i = 0; i < SRE_BTN_MAX; i++) {
                if (btns[i].active) {
                    fprintf(f, "Slot %d: id='%s' label='%s' active=%d hidden=%d overlay_id='%s' x=%.3f y=%.3f w=%.3f h=%.3f bg_alpha=%d alpha=%.3f clickable=%d\n",
                            i, btns[i].id, btns[i].label, btns[i].active, btns[i].hidden,
                            btns[i].overlay_id, btns[i].cur_x, btns[i].cur_y, btns[i].w, btns[i].h,
                            btns[i].bg_alpha, btns[i].alpha, btns[i].clickable);
                }
            }
            fclose(f);
        }
    }

    if (!m_initialized || !guest_buttons_ptr || globally_hidden) return;
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(m_imgui_ctx));

    // Retrieve active viewport bounds for correct positioning
    ImGuiIO& io = ImGui::GetIO();
    float win_w = io.DisplaySize.x;
    float win_h = io.DisplaySize.y;

    extern int GAME_W, GAME_H;
    float game_asp = (float)GAME_W / (float)GAME_H;
    float win_asp  = win_w / win_h;
    float vp_x = 0, vp_y = 0, vp_w = win_w, vp_h = win_h;

    if (win_asp > game_asp) {
        vp_w = win_h * game_asp;
        vp_x = (win_w - vp_w) / 2;
    } else {
        vp_h = win_w / game_asp;
        vp_y = (win_h - vp_h) / 2;
    }

    SreOverlaySlot* overlays = static_cast<SreOverlaySlot*>(guest_overlays_ptr);
    SreBtnSlot* buttons = static_cast<SreBtnSlot*>(guest_buttons_ptr);

    static int last_active_count = -1;
    int current_active_count = 0;
    for (int i = 0; i < SRE_BTN_MAX; i++) {
        if (buttons[i].active) current_active_count++;
    }
    if (current_active_count != last_active_count) {
        last_active_count = current_active_count;
        std::cout << "[GUI-Debug] Active buttons count changed to " << current_active_count << std::endl;
    }

    // Determine if any full-screen / blocking overlay is active (for input gating)
    bool any_overlay_blocking = false;
    if (overlays) {
        for (int i = 0; i < SRE_OVERLAY_MAX; i++) {
            SreOverlaySlot& ovr = overlays[i];
            if (!ovr.active || ovr.hidden) continue;
            if (ovr.w >= 0.6f && ovr.h >= 0.6f) {
                any_overlay_blocking = true;
                break;
            }
        }
    }
    extern bool g_sre_overlay_blocking;
    g_sre_overlay_blocking = any_overlay_blocking;

    // Set up transparent fullscreen window for drawing and hit-testing
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | 
                                    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground |
                                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
                                    ImGuiWindowFlags_NoDecoration;

    ImVec2 mpos = io.MousePos;
    if (!is_input_blocked(mpos.x, mpos.y)) {
        window_flags |= ImGuiWindowFlags_NoInputs;
    }

    ImGui::Begin("##SreOverlayButtonsWindow", nullptr, window_flags);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // 1. Draw Overlays
    if (overlays) {
        for (int i = 0; i < SRE_OVERLAY_MAX; i++) {
            SreOverlaySlot& ovr = overlays[i];
            if (!ovr.active || ovr.hidden) continue;

            float ow = vp_w * ovr.w * ovr.scale_factor;
            float oh = vp_h * ovr.h * ovr.scale_factor;
            float ox = vp_x + vp_w * ovr.x - ow / 2.0f;
            float oy = vp_y + vp_h * ovr.y - oh / 2.0f;

            // Handle dragging & pinching on the overlay background
            if (ovr.movable || ovr.pinchable) {
                ImGui::SetCursorScreenPos(ImVec2(ox, oy));
                ImGui::PushID(ovr.id);
                ImGui::InvisibleButton("##ovr_bg", ImVec2(ow, oh));
                
                bool ovr_hovered = ImGui::IsItemHovered();
                bool ovr_active = ImGui::IsItemActive();

                if (ovr_active && ovr.movable && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    ImVec2 delta = io.MouseDelta;
                    if (vp_w > 0) ovr.x += delta.x / vp_w;
                    if (vp_h > 0) ovr.y += delta.y / vp_h;
                    ovr.dirty = 1;
                }

                if (ovr_hovered && ovr.pinchable) {
                    float wheel = io.MouseWheel;
                    if (wheel != 0.0f) {
                        ovr.scale_factor += wheel * 0.1f;
                        if (ovr.scale_factor < 0.2f) ovr.scale_factor = 0.2f;
                        if (ovr.scale_factor > 3.0f) ovr.scale_factor = 3.0f;
                        ovr.dirty = 1;
                    }
                }
                ImGui::PopID();
            }

            // Parse background color from packed ARGB (0xAARRGGBB)
            unsigned int c_val = (unsigned int)ovr.bg_color;
            uint8_t ba = (c_val >> 24) & 0xFF;
            uint8_t br = (c_val >> 16) & 0xFF;
            uint8_t bg_c = (c_val >> 8) & 0xFF;
            uint8_t bb = c_val & 0xFF;

            if (c_val == 0) {
                br = 22; bg_c = 27; bb = 34;
                ba = (uint8_t)ovr.bg_alpha;
            }

            ImU32 fill_col = IM_COL32(br, bg_c, bb, ba);
            float rounding = (ovr.corner_radius > 0.0f) ? ovr.corner_radius : 6.0f;
            if (ovr.w >= 0.98f && ovr.h >= 0.98f) rounding = 0.0f;

            draw_list->AddRectFilled(ImVec2(ox, oy), ImVec2(ox + ow, oy + oh), fill_col, rounding);

            if (ovr.w < 0.98f || ovr.h < 0.98f) {
                draw_list->AddRect(ImVec2(ox, oy), ImVec2(ox + ow, oy + oh),
                                   IM_COL32(233, 69, 96, 100), rounding, 0, 1.0f);
            }

            // Separators
            for (int s = 0; s < ovr.separator_count; s++) {
                float sy = ovr.separators[s];
                float line_y = oy + oh * sy;
                draw_list->AddLine(ImVec2(ox, line_y), ImVec2(ox + ow, line_y),
                                   IM_COL32(255, 255, 255, 50), 1.5f);
            }
        }
    }

    // 2. Draw Buttons
    ImFont* btn_font = static_cast<ImFont*>(m_font_button);

    for (int i = 0; i < SRE_BTN_MAX; i++) {
        SreBtnSlot& btn = buttons[i];
        if (!btn.active || btn.hidden) continue;

        // Resolve parent overlay
        SreOverlaySlot* parent_ovr = nullptr;
        if (btn.overlay_id[0] != '\0' && overlays) {
            for (int o = 0; o < SRE_OVERLAY_MAX; o++) {
                if (overlays[o].active && strcmp(overlays[o].id, btn.overlay_id) == 0) {
                    parent_ovr = &overlays[o];
                    break;
                }
            }
            if (!parent_ovr) continue;
        }

        // Hide button if parent overlay is hidden/inactive
        if (parent_ovr && (parent_ovr->hidden || !parent_ovr->active)) continue;

        // Compute sizes, accounting for overlay scale
        float o_scale = parent_ovr ? parent_ovr->scale_factor : 1.0f;
        // Kiwi sizes controls from the shorter viewport side while positions
        // remain normalized to the full viewport.
        float square_base = std::min(vp_w, vp_h);
        float pw = square_base * btn.w * btn.scale_x * o_scale;
        float ph = square_base * btn.h * btn.scale_y * o_scale;
        float bx, by;

        if (parent_ovr) {
            float ow = vp_w * parent_ovr->w * parent_ovr->scale_factor;
            float oh = vp_h * parent_ovr->h * parent_ovr->scale_factor;
            float ox = vp_x + vp_w * parent_ovr->x - ow / 2.0f;
            float oy = vp_y + vp_h * parent_ovr->y - oh / 2.0f;
            bx = ox + ow * btn.cur_x - pw / 2.0f;
            by = oy + oh * btn.cur_y - ph / 2.0f;
        } else {
            bx = vp_x + vp_w * btn.cur_x - pw / 2.0f;
            by = vp_y + vp_h * btn.cur_y - ph / 2.0f;
        }

        float bx2 = bx + pw;
        float by2 = by + ph;
        if (bx2 < 0 || by2 < 0 || bx > win_w || by > win_h) continue;

        // Interactive Invisible Button
        ImGui::SetCursorScreenPos(ImVec2(bx, by));
        ImGui::PushID(btn.id);
        
        bool clicked = false;
        if (btn.clickable) {
            clicked = ImGui::InvisibleButton("##btn", ImVec2(pw, ph));
        } else {
            // Non-interactive spacing/dummy widget
            ImGui::Dummy(ImVec2(pw, ph));
        }

        bool hovered = btn.clickable && ImGui::IsItemHovered();
        bool pressing = btn.clickable && ImGui::IsItemActive();

        // Draw premium background style
        if (btn.bg_alpha > 0) {
            float alpha = (btn.alpha / 255.0f) * (btn.bg_alpha / 255.0f);
            
            ImU32 bg_col;
            if (pressing) {
                bg_col = IM_COL32(0, 100, 200, (uint8_t)(alpha * 255.0f * 0.9f));
            } else if (hovered) {
                bg_col = IM_COL32(40, 45, 60, (uint8_t)(alpha * 255.0f * 0.85f));
            } else {
                bg_col = IM_COL32(20, 20, 25, (uint8_t)(alpha * 255.0f * 0.75f));
            }
            
            float rounding = ph * 0.25f;
            if (rounding < 4.0f) rounding = 4.0f;
            if (rounding > 12.0f) rounding = 12.0f;

            draw_list->AddRectFilled(ImVec2(bx, by), ImVec2(bx2, by2), bg_col, rounding);

            ImU32 border_col;
            if (pressing) {
                border_col = IM_COL32(0, 180, 255, 255);
            } else if (hovered) {
                border_col = IM_COL32(0, 140, 255, 220);
            } else {
                border_col = IM_COL32(255, 255, 255, (uint8_t)(alpha * 120.0f));
            }
            draw_list->AddRect(ImVec2(bx, by), ImVec2(bx2, by2), border_col, rounding, 0, 1.5f);
        }

        // Draw label
        const char* lbl = btn.label;
        if (lbl[0] != '\0' && btn_font) {
            float fscale = btn.text_scale > 0.0f ? btn.text_scale : 1.0f;
            if (fscale > 3.0f) fscale = 3.0f;
            float fs = btn_font->LegacySize * fscale * o_scale;
            ImVec2 tsize = btn_font->CalcTextSizeA(fs, FLT_MAX, 0.0f, lbl);

            float pl = btn.padding_l * o_scale;
            float pt = btn.padding_t * o_scale;
            float pr = btn.padding_r * o_scale;
            float pb = btn.padding_b * o_scale;

            int gravity = btn.alignment;
            float tx = bx + (pw - tsize.x) * 0.5f;
            float ty = by + (ph - tsize.y) * 0.5f;

            if (gravity != 0) {
                if ((gravity & 3) == 3 || gravity == 3) {
                    tx = bx + pl;
                } else if ((gravity & 5) == 5 || gravity == 5) {
                    tx = bx2 - tsize.x - pr;
                }
                
                if ((gravity & 48) == 48 || gravity == 48) {
                    ty = by + pt;
                } else if ((gravity & 80) == 80 || gravity == 80) {
                    ty = by2 - tsize.y - pb;
                }
            }

            unsigned int tc = (unsigned int)btn.text_color;
            uint8_t ta = (tc >> 24) & 0xFF;
            uint8_t tr = (tc >> 16) & 0xFF;
            uint8_t tg = (tc >> 8)  & 0xFF;
            uint8_t tb = tc & 0xFF;
            if (ta == 0) ta = (uint8_t)btn.alpha;
            ImU32 text_col = IM_COL32(tr, tg, tb, ta);

            draw_list->AddText(btn_font, fs, ImVec2(tx, ty), text_col, lbl);
        }

        // Input state machine
        if (btn.clickable) {
            bool completed_drag = btn.dragging != 0;
            if (pressing) {
                btn.pressed = 1;
                if (btn.movable && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    btn.dragging = 1;
                    ImVec2 delta = io.MouseDelta;
                    if (parent_ovr) {
                        float ow = vp_w * parent_ovr->w * parent_ovr->scale_factor;
                        float oh = vp_h * parent_ovr->h * parent_ovr->scale_factor;
                        if (ow > 0) btn.cur_x += delta.x / ow;
                        if (oh > 0) btn.cur_y += delta.y / oh;
                    } else {
                        if (vp_w > 0) btn.cur_x += delta.x / vp_w;
                        if (vp_h > 0) btn.cur_y += delta.y / vp_h;
                    }

                    if (btn.confined) {
                        if (parent_ovr) {
                            float min_x = (pw / 2.0f) / (vp_w * parent_ovr->w * parent_ovr->scale_factor);
                            float max_x = 1.0f - min_x;
                            float min_y = (ph / 2.0f) / (vp_h * parent_ovr->h * parent_ovr->scale_factor);
                            float max_y = 1.0f - min_y;
                            if (btn.cur_x < min_x) btn.cur_x = min_x;
                            if (btn.cur_x > max_x) btn.cur_x = max_x;
                            if (btn.cur_y < min_y) btn.cur_y = min_y;
                            if (btn.cur_y > max_y) btn.cur_y = max_y;
                        } else {
                            float min_x = (pw / 2.0f) / vp_w;
                            float max_x = 1.0f - min_x;
                            float min_y = (ph / 2.0f) / vp_h;
                            float max_y = 1.0f - min_y;
                            if (btn.cur_x < min_x) btn.cur_x = min_x;
                            if (btn.cur_x > max_x) btn.cur_x = max_x;
                            if (btn.cur_y < min_y) btn.cur_y = min_y;
                            if (btn.cur_y > max_y) btn.cur_y = max_y;
                        }
                    }
                    btn.dirty = 1;
                }
            } else {
                btn.pressed = 0;
                if (btn.dragging) {
                    btn.dragging = 0;
                    if (btn.snapback) {
                        btn.cur_x = btn.home_x;
                        btn.cur_y = btn.home_y;
                        btn.dirty = 1;
                    }
                }
            }

            // A drag release is not a click. This preserves Kiwi's polling
            // API without the Android listener bug that consumed all taps.
            if (clicked && !completed_drag) {
                btn.released = 1;
                std::cout << "[SRE GUI] ImGui Click: '" << btn.label << "' (slot " << i << ")\n";
            }
        }

        ImGui::PopID();
    }

    ImGui::End();
    ImGui::PopStyleVar(3);
}

bool SwordfareGUI::is_input_blocked(float mx, float my) {
    if (!m_initialized) return false;
    
    ImGuiIO& io = ImGui::GetIO();
    float win_w = io.DisplaySize.x;
    float win_h = io.DisplaySize.y;

    if (!m_last_buttons_ptr || m_buttons_globally_hidden) return false;

    extern int GAME_W, GAME_H;
    float game_asp = (float)GAME_W / (float)GAME_H;
    float win_asp  = win_w / win_h;
    float vp_x = 0, vp_y = 0, vp_w = win_w, vp_h = win_h;
    
    if (win_asp > game_asp) {
        vp_w = win_h * game_asp;
        vp_x = (win_w - vp_w) / 2;
    } else {
        vp_h = win_w / game_asp;
        vp_y = (win_h - vp_h) / 2;
    }
    
    SreOverlaySlot* overlays = static_cast<SreOverlaySlot*>(m_last_overlays_ptr);
    SreBtnSlot* buttons = static_cast<SreBtnSlot*>(m_last_buttons_ptr);

    // 1. Block clicks inside any active overlay background region
    if (overlays) {
        for (int i = 0; i < SRE_OVERLAY_MAX; i++) {
            SreOverlaySlot& ovr = overlays[i];
            if (!ovr.active || ovr.hidden) continue;
            float ow = vp_w * ovr.w;
            float oh = vp_h * ovr.h;
            float ox = vp_x + vp_w * ovr.x - ow / 2.0f;
            float oy = vp_y + vp_h * ovr.y - oh / 2.0f;
            if (mx >= ox && mx <= ox + ow && my >= oy && my <= oy + oh) {
                return true; // Click is inside an overlay panel — always block
            }
        }
    }

    // 2. Check if coords hit any active clickable button
    if (buttons) {
        for (int i = 0; i < SRE_BTN_MAX; i++) {
            SreBtnSlot& btn = buttons[i];
            if (!btn.active || btn.hidden || !btn.clickable) continue;

            // Resolve parent overlay
            SreOverlaySlot* parent_ovr = nullptr;
            if (btn.overlay_id[0] != '\0' && overlays) {
                for (int o = 0; o < SRE_OVERLAY_MAX; o++) {
                    if (overlays[o].active && strcmp(overlays[o].id, btn.overlay_id) == 0) {
                        parent_ovr = &overlays[o];
                        break;
                    }
                }
                if (parent_ovr && (parent_ovr->hidden || !parent_ovr->active)) continue;
            }

            float square_base = std::min(vp_w, vp_h);
            float pw = square_base * btn.w * btn.scale_x;
            float ph = square_base * btn.h * btn.scale_y;
            float bx, by;

            if (parent_ovr) {
                float ow = vp_w * parent_ovr->w;
                float oh = vp_h * parent_ovr->h;
                float ox = vp_x + vp_w * parent_ovr->x - ow / 2.0f;
                float oy = vp_y + vp_h * parent_ovr->y - oh / 2.0f;
                bx = ox + ow * btn.cur_x - pw / 2.0f;
                by = oy + oh * btn.cur_y - ph / 2.0f;
            } else {
                bx = vp_x + vp_w * btn.cur_x - pw / 2.0f;
                by = vp_y + vp_h * btn.cur_y - ph / 2.0f;
            }

            if (mx >= bx && mx <= bx + pw && my >= by && my <= by + ph) {
                return true;
            }
        }
    }
    
    return false;
}

// =============================================================================
// Backends of the Scene Select and Lua Console tool windows
// =============================================================================

// Scene Select backend: the scene shifter state lives in guest memory once the runtime is loaded.
SwordfareGUI::SceneShifterState SwordfareGUI::scene_shifter_state() {
    int*   p_list_count  = (m_scene_shifter_ready && m_guest_memory) ? (int*)(m_guest_memory + m_ss_list_count_va) : &g_sre_scene_list_count;
    char (*p_list)[128]  = (m_scene_shifter_ready && m_guest_memory) ? (char(*)[128])(m_guest_memory + m_ss_list_va) : g_sre_scene_list;
    volatile int* p_pending = (m_scene_shifter_ready && m_guest_memory) ? (volatile int*)(m_guest_memory + m_ss_pending_va) : &g_sre_scene_shift_pending;
    char*  p_error       = (m_scene_shifter_ready && m_guest_memory) ? (char*)(m_guest_memory + m_ss_error_va) : g_sre_scene_shift_last_error;
    int*   p_active      = (m_scene_shifter_ready && m_guest_memory) ? (int*)(m_guest_memory + m_ss_active_va) : (int*)&g_sre_scene_shift_active;
    char*  p_cur_scene   = (m_scene_shifter_ready && m_guest_memory && m_ss_current_scene_va) ? (char*)(m_guest_memory + m_ss_current_scene_va) : g_sre_current_scene_name;

    SceneShifterState s;
    s.count   = *p_list_count;
    s.names   = p_list;
    s.current = p_cur_scene;
    s.busy    = (*p_pending != 0 || *p_active != 0);
    s.error   = (p_error[0] && strcmp(p_error, "OK") != 0) ? p_error : "";
    return s;
}

void SwordfareGUI::scene_shifter_rescan() {
    int*   p_list_count  = (m_scene_shifter_ready && m_guest_memory) ? (int*)(m_guest_memory + m_ss_list_count_va) : &g_sre_scene_list_count;
    char (*p_list)[128]  = (m_scene_shifter_ready && m_guest_memory) ? (char(*)[128])(m_guest_memory + m_ss_list_va) : g_sre_scene_list;

    if (m_scene_shifter_ready && !m_ss_assets_dir.empty()) {
        namespace fs = std::filesystem;
        *p_list_count = 0;
        auto try_scan = [&](const fs::path& dir) {
            if (!fs::exists(dir) || !fs::is_directory(dir)) return;
            std::error_code ec;
            for (auto& ent : fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec)) {
                if (*p_list_count >= 256) break;
                if (!ent.is_regular_file(ec)) continue;
                auto name = ent.path().filename().string();
                if (name.size() <= 6 || name.substr(name.size() - 6) != ".scene") continue;
                std::string stem = name.substr(0, name.size() - 6);
                bool dup = false;
                for (int i = 0; i < *p_list_count; i++) {
                    if (std::string(p_list[i]) == stem) { dup = true; break; }
                }
                if (dup) continue;
                strncpy(p_list[*p_list_count], stem.c_str(), 127);
                p_list[*p_list_count][127] = '\0';
                (*p_list_count)++;
            }
        };
        try_scan(fs::path(m_ss_assets_dir) / "resources");
        qsort(p_list, *p_list_count, 128, [](const void* a, const void* b) -> int {
            return strcmp((const char*)a, (const char*)b);
        });
    } else {
        sre_scene_shifter_scan_scenes();
    }
}

bool SwordfareGUI::scene_shifter_go(const char* scene, const char* spawn, bool forced) {
    if (!scene || !scene[0]) return false;

    char*  p_target      = (m_scene_shifter_ready && m_guest_memory) ? (char*)(m_guest_memory + m_ss_target_va) : g_sre_scene_shift_target;
    char*  p_spawn       = (m_scene_shifter_ready && m_guest_memory) ? (char*)(m_guest_memory + m_ss_spawn_va) : g_sre_scene_shift_spawn;
    volatile int* p_pending = (m_scene_shifter_ready && m_guest_memory) ? (volatile int*)(m_guest_memory + m_ss_pending_va) : &g_sre_scene_shift_pending;
    int*   p_active      = (m_scene_shifter_ready && m_guest_memory) ? (int*)(m_guest_memory + m_ss_active_va) : (int*)&g_sre_scene_shift_active;

    if (*p_pending != 0 || *p_active != 0) return false;

    strncpy(p_target, scene, 127);
    p_target[127] = '\0';
    strncpy(p_spawn, spawn ? spawn : "", 63);
    p_spawn[63] = '\0';
    // Mode: 1=normal, 2=forced
    *p_pending = forced ? 2 : 1;
    return true;
}

void SwordfareGUI::init_lua_console(
    uint8_t* guest_memory,
    uint64_t buf_addr,   uint64_t result_addr,
    uint64_t pending_addr, uint64_t status_addr,
    uint64_t print_addr)
{
    m_guest_memory         = guest_memory;
    m_console_buf_addr     = buf_addr;
    m_console_result_addr  = result_addr;
    m_console_pending_addr = pending_addr;
    m_console_status_addr  = status_addr;
    m_console_print_addr   = print_addr;
    m_console_ready        = (buf_addr != 0);

    if (m_console_ready) {
        m_console_history.push_back({"Raijin  •  World's first Swordigo Lua console  •  Swordfare subsystem", false, false});
        m_console_history.push_back({"Type Lua and press Enter. Up/Down for history.", false, false});
    }
}

// ---------------------------------------------------------------------------
// SwordfareGUI::init_scene_shifter
// ---------------------------------------------------------------------------
// Wires up all scene shifter globals in guest memory.  After this call the
// GUI reads/writes those globals directly via (m_guest_memory + VA) instead
// of the always-zero host-side weak fallback copies.
//
// Also runs an initial host-side filesystem scan (std::filesystem) of
//   assets_dir/resources/  and, if present,  data_dir/mods/<mod>/resources/
// to populate g_sre_scene_list in guest memory so the panel shows scenes
// immediately without needing the guest scan function.
// ---------------------------------------------------------------------------
#include <filesystem>
void SwordfareGUI::init_scene_shifter(
    uint8_t* guest_memory,
    uint64_t scene_list_count_va,
    uint64_t scene_list_va,
    uint64_t shift_pending_va,
    uint64_t shift_target_va,
    uint64_t shift_spawn_va,
    uint64_t shift_error_va,
    uint64_t shift_active_va,
    uint64_t current_scene_va,
    const std::string& assets_dir)
{
    namespace fs = std::filesystem;

    // Store guest memory base (may already be set by init_lua_console)
    if (!m_guest_memory) m_guest_memory = guest_memory;

    m_ss_list_count_va    = scene_list_count_va;
    m_ss_list_va          = scene_list_va;
    m_ss_pending_va       = shift_pending_va;
    m_ss_target_va        = shift_target_va;
    m_ss_spawn_va         = shift_spawn_va;
    m_ss_error_va         = shift_error_va;
    m_ss_active_va        = shift_active_va;
    m_ss_current_scene_va = current_scene_va;
    m_ss_assets_dir       = assets_dir;  // e.g. ~/.local/share/ricasso/assets

    m_scene_shifter_ready = (guest_memory &&
                             scene_list_count_va &&
                             scene_list_va &&
                             shift_pending_va &&
                             shift_target_va);

    if (!m_scene_shifter_ready) {
        fprintf(stderr, "[SwordfareGUI] init_scene_shifter: missing VAs — scene shifter disabled\n");
        return;
    }

    // Run host-side scan immediately so the list is populated on first open.
    // We write directly into guest memory (g_sre_scene_list / g_sre_scene_list_count).
    int*  g_count = (int*)(guest_memory + scene_list_count_va);
    char (*g_list)[128] = (char(*)[128])(guest_memory + scene_list_va);
    *g_count = 0;

    auto try_scan = [&](const fs::path& dir) {
        if (!fs::exists(dir) || !fs::is_directory(dir)) return;
        std::error_code ec;
        for (auto& ent : fs::recursive_directory_iterator(dir,
                fs::directory_options::skip_permission_denied, ec)) {
            if (*g_count >= 256) break;
            if (!ent.is_regular_file(ec)) continue;
            auto name = ent.path().filename().string();
            if (name.size() <= 6) continue;
            if (name.substr(name.size() - 6) != ".scene") continue;
            // strip extension
            std::string stem = name.substr(0, name.size() - 6);
            // deduplicate
            bool dup = false;
            for (int i = 0; i < *g_count; i++) {
                if (std::string(g_list[i]) == stem) { dup = true; break; }
            }
            if (dup) continue;
            strncpy(g_list[*g_count], stem.c_str(), 127);
            g_list[*g_count][127] = '\0';
            (*g_count)++;
        }
    };

    // 1. Vanilla assets/resources/
    try_scan(fs::path(assets_dir) / "resources");

    // 2. Sort alphabetically for readability
    qsort(g_list, *g_count, 128, [](const void* a, const void* b) -> int {
        return strcmp((const char*)a, (const char*)b);
    });

    fprintf(stderr, "[SwordfareGUI] Scene Shifter ready — %d scenes found in %s/resources\n",
            *g_count, assets_dir.c_str());
}


void SwordfareGUI::console_submit(const std::string& cmd) {
    if (cmd.empty() || !m_guest_memory || !m_console_buf_addr) return;

    // Intercept 'openport' command
    if (cmd == "openport" || cmd.rfind("openport ", 0) == 0) {
        m_console_history.push_back({"> " + cmd, false, true});
        int port = 12345;
        if (cmd.size() > 9) {
            try {
                port = std::stoi(cmd.substr(9));
            } catch (...) {
                m_console_history.push_back({"[TCP-Console] Error: Invalid port number.", true, false});
                return;
            }
        }
        start_tcp_server(port);
        return;
    }

    std::cout << "[SRE-Lua] > " << cmd << std::endl;
    m_console_history.push_back({"> " + cmd, false, true});
    while ((int)m_console_history.size() > CONSOLE_MAX_HISTORY)
        m_console_history.erase(m_console_history.begin());

    char* buf = (char*)(m_guest_memory + m_console_buf_addr);
    size_t len = cmd.size();
    if (len > 4094) len = 4094;
    memcpy(buf, cmd.c_str(), len);
    buf[len] = 0;

    *(int32_t*)(m_guest_memory + m_console_status_addr)  = 0;
    *(int32_t*)(m_guest_memory + m_console_pending_addr) = 1;
}

void SwordfareGUI::start_tcp_server(int port) {
    stop_tcp_server();

    m_tcp_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_tcp_server_fd < 0) {
        m_console_history.push_back({"[TCP-Console] Error: Failed to create socket.", true, false});
        return;
    }

    int opt = 1;
    setsockopt(m_tcp_server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(m_tcp_server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        m_console_history.push_back({"[TCP-Console] Error: Failed to bind to port " + std::to_string(port), true, false});
        close(m_tcp_server_fd);
        m_tcp_server_fd = -1;
        return;
    }

    if (listen(m_tcp_server_fd, 3) < 0) {
        m_console_history.push_back({"[TCP-Console] Error: Failed to listen on socket.", true, false});
        close(m_tcp_server_fd);
        m_tcp_server_fd = -1;
        return;
    }

    m_tcp_running = true;
    m_tcp_thread = std::thread(&SwordfareGUI::tcp_server_loop, this);
    m_console_history.push_back({"[TCP-Console] Server started on port " + std::to_string(port) + ". Run 'nc localhost " + std::to_string(port) + "' to connect.", false, false});
}

void SwordfareGUI::stop_tcp_server() {
    m_tcp_running = false;
    // Wake the reader thread if it is parked in the back-pressure CV wait so it
    // observes m_tcp_running == false and unwinds instead of hanging the join.
    m_tcp_done_cv.notify_all();
    {
        std::lock_guard<std::mutex> lk(m_tcp_mutex);
        m_tcp_cmd_queue.clear();
        m_tcp_cmd_in_flight = false;
        m_tcp_in_flight_fd  = -1;
    }
    if (m_tcp_server_fd >= 0) {
        ::shutdown(m_tcp_server_fd, SHUT_RDWR);
        close(m_tcp_server_fd);
        m_tcp_server_fd = -1;
    }
    
    int client_fd = m_tcp_client_fd.load();
    if (client_fd >= 0) {
        ::shutdown(client_fd, SHUT_RDWR);
        close(client_fd);
        m_tcp_client_fd = -1;
    }

    if (m_tcp_thread.joinable()) {
        m_tcp_thread.join();
    }
}

static bool is_lua_chunk_complete(const std::string& code) {
    int braces = 0;
    int parens = 0;
    int brackets = 0; // [ ]
    int blocks = 0;
    bool pending_elseif = false; // true between an `elseif` and its `then`

    size_t i = 0;
    size_t n = code.length();
    while (i < n) {
        char ch = code[i];
        
        // 1. Skip comments
        if (ch == '-' && i + 1 < n && code[i + 1] == '-') {
            i += 2;
            // Check for multi-line comment: --[[ ... ]] or --[=[ ... ]=]
            if (i < n && code[i] == '[') {
                size_t start = i;
                size_t equals_count = 0;
                i++;
                while (i < n && code[i] == '=') {
                    equals_count++;
                    i++;
                }
                if (i < n && code[i] == '[') {
                    // It is a multi-line comment. Find matching closer: ]=...]
                    i++;
                    std::string closer = "]" + std::string(equals_count, '=') + "]";
                    size_t closer_pos = code.find(closer, i);
                    if (closer_pos != std::string::npos) {
                        i = closer_pos + closer.length();
                    } else {
                        i = n; // Incomplete multi-line comment
                    }
                    continue;
                }
                // Not a multi-line comment, fall back to single-line comment
                i = start;
            }
            // Single-line comment: skip to newline
            while (i < n && code[i] != '\n') {
                i++;
            }
            continue;
        }
        
        // 2. Skip string literals
        if (ch == '\'' || ch == '"') {
            char quote = ch;
            i++;
            while (i < n) {
                if (code[i] == '\\' && i + 1 < n) {
                    i += 2; // skip escaped char
                    continue;
                }
                if (code[i] == quote) {
                    i++;
                    break;
                }
                i++;
            }
            continue;
        }
        
        // 3. Skip Lua long brackets (strings): [[ ... ]] or [=[ ... ]=]
        if (ch == '[') {
            size_t start = i;
            size_t equals_count = 0;
            i++;
            while (i < n && code[i] == '=') {
                equals_count++;
                i++;
            }
            if (i < n && code[i] == '[') {
                i++;
                std::string closer = "]" + std::string(equals_count, '=') + "]";
                size_t closer_pos = code.find(closer, i);
                if (closer_pos != std::string::npos) {
                    i = closer_pos + closer.length();
                } else {
                    i = n; // Incomplete long string
                }
                continue;
            }
            i = start; // Just a regular bracket [
        }
        
        // 4. Track braces, parens, brackets
        if (ch == '{') braces++;
        else if (ch == '}') braces--;
        else if (ch == '(') parens++;
        else if (ch == ')') parens--;
        else if (ch == '[') brackets++;
        else if (ch == ']') brackets--;
        
        // 5. Track Lua block keywords.
        //
        //   Openers:  function / do / then / repeat  -> blocks++
        //   Closers:  end / until                    -> blocks--
        //   elseif / else are NEITHER — they live inside an already-open
        //   if-block and must not change the depth (the old code did
        //   `elseif -> blocks--`, which made every if/elseif/end report as
        //   unbalanced and falsely "complete", the core multiline bug).
        //
        //   Note: a bare `then`/`do` that is part of `elseif ... then` still
        //   correctly pairs with the block's single `end`, because the `if`
        //   only opened one level via its first `then`. To keep depth right we
        //   only count the FIRST `then` of an if-chain: track whether we are
        //   between `elseif`/`else` and its `then` and skip that `then`.
        if (isalpha((unsigned char)ch) || ch == '_') {
            std::string word;
            while (i < n && (isalnum((unsigned char)code[i]) || code[i] == '_')) {
                word += code[i];
                i++;
            }
            if (word == "function" || word == "do" || word == "repeat") {
                blocks++;
            } else if (word == "then") {
                // `then` closes an `if`/`elseif` condition. Only the if's
                // first `then` opens a block; an `elseif ... then` reuses the
                // same block level.
                if (pending_elseif) {
                    pending_elseif = false;   // elseif's then: no depth change
                } else {
                    blocks++;                 // if/while ... then: opens block
                }
            } else if (word == "end" || word == "until") {
                blocks--;
            } else if (word == "elseif") {
                pending_elseif = true;        // its following `then` is neutral
            }
            // `else` and everything else: no depth change.
            continue;
        }
        
        i++;
    }
    return (braces <= 0 && parens <= 0 && brackets <= 0 && blocks <= 0);
}

// Wrap a bare expression so the console prints its value, LuaJIT/lua5.1
// interactive style. If `code` is a single expression (e.g. `1+2`, `ffi.base()`,
// `player.hp`) rather than a statement, evaluating it as a chunk yields nothing;
// prefixing `return ` makes the result visible. We only do this when:
//   * the chunk is a single line (no embedded newline), AND
//   * it does not already start with a statement keyword or `return`, AND
//   * it does not contain a top-level `=` assignment (`==`/`~=`/`<=`/`>=` are ok).
// On failure to compile as an expression, the caller falls back to the raw
// chunk, so this is always safe.
static std::string maybe_wrap_expression(const std::string& code) {
    // Trim.
    size_t a = code.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return code;
    size_t b = code.find_last_not_of(" \t\r\n");
    std::string s = code.substr(a, b - a + 1);

    if (s.find('\n') != std::string::npos) return code;   // multi-line: leave as-is

    static const char* kw[] = {
        "return", "local", "if", "for", "while", "repeat", "do", "function",
        "break", "goto", "end", "else", "elseif", "then", nullptr
    };
    for (int i = 0; kw[i]; i++) {
        size_t klen = strlen(kw[i]);
        if (s.size() >= klen && s.compare(0, klen, kw[i]) == 0 &&
            (s.size() == klen || !(isalnum((unsigned char)s[klen]) || s[klen] == '_')))
            return code;   // already a statement
    }

    // Detect a top-level assignment `=` (not ==, ~=, <=, >=). Respect strings.
    bool in_str = false; char q = 0; int depth = 0;
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (in_str) {
            if (c == '\\') { i++; continue; }
            if (c == q) in_str = false;
            continue;
        }
        if (c == '\'' || c == '"') { in_str = true; q = c; continue; }
        if (c == '(' || c == '{' || c == '[') depth++;
        else if (c == ')' || c == '}' || c == ']') depth--;
        else if (c == '=' && depth == 0) {
            char prev = (i > 0) ? s[i-1] : 0;
            char next = (i+1 < s.size()) ? s[i+1] : 0;
            if (prev != '=' && prev != '~' && prev != '<' && prev != '>' &&
                next != '=')
                return code;   // assignment statement, don't wrap
        }
    }

    return "return " + s;
}

void SwordfareGUI::tcp_server_loop() {
    while (m_tcp_running) {
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(m_tcp_server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0) {
            if (!m_tcp_running) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // Handle client connection (support one client at a time for simplicity)
        int old_client = m_tcp_client_fd.exchange(client_fd);
        if (old_client >= 0) {
            const char* msg = "Another client connected. Disconnecting...\n";
            write(old_client, msg, strlen(msg));
            close(old_client);
        }

        // New connection generation. Purge any backlog from a previous client
        // and clear the in-flight slot so a dead client's queued/in-flight
        // results are never delivered to THIS client and the dispatcher cannot
        // stay wedged waiting on an owner that has gone away.
        uint64_t my_gen;
        {
            std::lock_guard<std::mutex> lk(m_tcp_mutex);
            my_gen = ++m_tcp_client_gen;
            m_tcp_cmd_queue.clear();
            m_tcp_cmd_in_flight = false;
            m_tcp_in_flight_fd  = -1;
            m_tcp_in_flight_gen = 0;
        }
        m_tcp_done_cv.notify_all();

        // Welcome greeting
        const char* greeting = 
            "\033[1;36m=====================================================\033[0m\n"
            "\033[1;32m      RAIJIN Lua SDK Console (Swordfare TCP Server)  \033[0m\n"
            "\033[1;36m=====================================================\033[0m\n"
            " \033[33m*\033[0m Enter Lua commands or blocks to evaluate dynamically.\n"
            " \033[33m*\033[0m Supports full multiline input (braces, quotes, functions).\n"
            " \033[33m*\033[0m Type \033[1;31m.\033[0m on a new line to clear current input buffer.\n"
            " \033[33m*\033[0m Type \033[1;32mclear\033[0m to clear screen.\n"
            " \033[33m*\033[0m Type \033[1;31mexit\033[0m or \033[1;31mquit\033[0m to close connection.\n\n"
            "\033[1;36mraijin-sdk\033[0m> ";
        write(client_fd, greeting, strlen(greeting));

        // Read loop for client commands
        char read_buf[4096];
        std::string line_accumulator;
        std::string multiline_cmd;
        while (m_tcp_running && m_tcp_client_fd.load() == client_fd) {
            ssize_t bytes_read = read(client_fd, read_buf, sizeof(read_buf) - 1);
            if (bytes_read <= 0) {
                // Client disconnected or read error
                break;
            }
            read_buf[bytes_read] = '\0';
            // Append by explicit length so embedded NULs in a paste don't
            // truncate the accumulator (operator+= on a char* stops at NUL).
            line_accumulator.append(read_buf, (size_t)bytes_read);

            // Process any complete lines
            size_t newline_pos;
            while ((newline_pos = line_accumulator.find('\n')) != std::string::npos) {
                std::string line = line_accumulator.substr(0, newline_pos);
                line_accumulator.erase(0, newline_pos + 1);

                // Strip carriage return if present
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }

                // Strip leading/trailing whitespaces
                size_t first = line.find_first_not_of(" \t");
                if (first != std::string::npos) {
                    line = line.substr(first);
                    size_t last = line.find_last_not_of(" \t");
                    line = line.substr(0, last + 1);
                } else {
                    line.clear();
                }

                // Check for input buffer reset command
                if (line == ".") {
                    multiline_cmd.clear();
                    const char* reset_msg = "Input buffer cleared.\n\033[1;36mraijin-sdk\033[0m> ";
                    write(client_fd, reset_msg, strlen(reset_msg));
                    continue;
                }

                if (line == "exit" || line == "quit") {
                    const char* bye = "Goodbye!\n";
                    write(client_fd, bye, strlen(bye));
                    close(client_fd);
                    m_tcp_client_fd.compare_exchange_strong(client_fd, -1);
                    break;
                }

                if (line == "clear") {
                    // Send ANSI clear screen command
                    const char* clear_ansi = "\033[2J\033[H\033[1;36mraijin-sdk\033[0m> ";
                    write(client_fd, clear_ansi, strlen(clear_ansi));
                    multiline_cmd.clear();
                    continue;
                }

                // Accumulate block line
                if (multiline_cmd.empty()) {
                    multiline_cmd = line;
                } else {
                    multiline_cmd += "\n" + line;
                }

                // Execute block if it is complete
                bool execute = false;
                if (multiline_cmd.empty()) {
                    execute = true;
                } else {
                    if (is_lua_chunk_complete(multiline_cmd)) {
                        execute = true;
                    }
                }

                if (execute) {
                    if (!multiline_cmd.empty()) {
                        // Auto-wrap a bare expression (e.g. `1+2`, `ffi.base()`)
                        // so the console prints its value like an interactive
                        // REPL. Multi-line/statement chunks pass through.
                        std::string chunk = maybe_wrap_expression(multiline_cmd);

                        // Enqueue the chunk (FIFO). The main thread drains one
                        // at a time and returns each result + prompt in order.
                        // We do NOT busy-wait on a single slot anymore — that
                        // was the multiline-paste bug (races/dropped lines).
                        //
                        // Back-pressure: if the client floods us (huge paste),
                        // wait — bounded — until the queue drains below a cap so
                        // we never grow unboundedly, but without ever dropping a
                        // line. The wait is interruptible on disconnect/shutdown.
                        {
                            std::unique_lock<std::mutex> lk(m_tcp_mutex);
                            const size_t kMaxQueued = 256;
                            m_tcp_done_cv.wait_for(
                                lk, std::chrono::milliseconds(250),
                                [&]{
                                    return !m_tcp_running ||
                                           m_tcp_client_fd.load() != client_fd ||
                                           m_tcp_cmd_queue.size() < kMaxQueued;
                                });
                            if (!m_tcp_running || m_tcp_client_fd.load() != client_fd) {
                                break;
                            }
                            m_tcp_cmd_queue.push_back({chunk, client_fd, my_gen});
                        }

                        multiline_cmd.clear();
                        // The prompt for THIS command is emitted by the main
                        // thread once the guest returns its result, so output
                        // and prompt stay correctly ordered even under paste.
                    } else {
                        // Empty line in a clean state: just re-emit the prompt.
                        const char* prompt = "\033[1;36mraijin-sdk\033[0m> ";
                        write(client_fd, prompt, strlen(prompt));
                    }
                } else {
                    // Incomplete block: keep accumulating lines. Only emit a
                    // continuation prompt when we have genuinely CONSUMED all
                    // buffered input and are now waiting on the human to type
                    // the next line. When a whole multi-line block was pasted
                    // in a single write (line_accumulator still holds more
                    // complete lines), emitting a ">>" per interior line just
                    // floods the client with "  >>   >>   >>" and desyncs the
                    // display — so we suppress it until the buffer drains.
                    if (line_accumulator.find('\n') == std::string::npos) {
                        const char* cont_prompt = "\033[1;33m          \033[0m>> ";
                        write(client_fd, cont_prompt, strlen(cont_prompt));
                    }
                }
            }
        }

        // Cleanup this client connection if we broke out of the read loop.
        // Also purge any pending/in-flight work belonging to THIS connection so
        // the main-thread dispatcher never stays wedged (m_tcp_cmd_in_flight
        // stuck true) waiting on a client that has gone away, and a late guest
        // result for this connection is dropped rather than leaked to the next
        // client. Bumping the generation invalidates any in-flight result.
        {
            std::lock_guard<std::mutex> lk(m_tcp_mutex);
            ++m_tcp_client_gen;
            m_tcp_cmd_queue.clear();
            m_tcp_cmd_in_flight = false;
            m_tcp_in_flight_fd  = -1;
            m_tcp_in_flight_gen = 0;
        }
        m_tcp_done_cv.notify_all();

        if (m_tcp_client_fd.load() == client_fd) {
            close(client_fd);
            m_tcp_client_fd.store(-1);
        }
    }
}

// F2 controls editor window (bindings table, tap points)
