// =============================================================================
// Swordfare GUI — ImGui layer drawn on top of the running game
//
// Draws what stays inside the game picture: the F3 debug overlay and the
// buttons and overlays that Lua mods register. Windows with controls are tool
// windows (tool_windows.h); this class keeps the backends they use (Lua console
// bridge, TCP console server, scene shifter).
//
// Usage:
//   SwordfareGUI gui;
//   gui.init(sdl_window, gl_context);        // call once after GL context ready
//   // per-frame (after game draw, before swap):
//   gui.begin_frame();
//   if (gui.is_visible()) gui.draw_debug(stats);
//   gui.end_frame();
//   gui.shutdown();                          // call on exit
// =============================================================================

#pragma once

#include <SDL3/SDL.h>
#define GL_GLEXT_PROTOTYPES
#include "platform/gl_inc.h"

#ifdef VULKAN_BACKEND
#include "volk.h"
#else
// Stub so VkDescriptorPool / VK_NULL_HANDLE exist without the full Vulkan SDK
typedef struct VkDescriptorPool_T* VkDescriptorPool;
#define VK_NULL_HANDLE nullptr
#endif

#include "platform/gui.h"
#include <vector>
#include <string>
#include <cstdint>
#include <thread>
#include <atomic>
#include <mutex>
#include <deque>
#include <condition_variable>
#include <memory>

// ---------------------------------------------------------------------------
// Debug stats snapshot (passed to draw_debug each frame)
// ---------------------------------------------------------------------------
struct SwordfareDebugStats {
    float   fps           = 0.0f;
    float   dt_seconds    = 0.0f;
    int     frame_count   = 0;
    int     draw_calls    = 0;
    int     tex_binds     = 0;
    int     vertices      = 0;
    int     matrix_ops    = 0;
    int     state_changes = 0;
    int     tex_uploads   = 0;
    int     win_w         = 0, win_h = 0;
    int     draw_w        = 0, draw_h = 0;
    int     mouse_x       = 0, mouse_y = 0;
    float   cam_x         = 0.0f, cam_y = 0.0f, cam_z = 0.0f;
    float   cam_zoom      = 1.0f;
    float   hero_x        = 0.0f, hero_y = 0.0f, hero_z = 0.0f;
    bool    cam_active    = false;
    bool    typing_mode   = false;
    bool    game_paused   = false;
    bool    postfx_on     = false;
    const char* postfx_preset  = "Off";
    const char* scale_mode     = "Sharp-Bilinear";
    const char* binary_name    = "";
    const char* speed_label    = "1×";
    const char* graphics_api   = "OpenGL";
};

// ---------------------------------------------------------------------------
// SwordfareGUI — main class
// ---------------------------------------------------------------------------
class SwordfareGUI {
public:
    SwordfareGUI()  = default;
    ~SwordfareGUI() = default;

    // Lifecycle
    void init(SDL_Window* window, SDL_GLContext gl_ctx);
    void init_vulkan(SDL_Window* window, class VulkanBackend* vk_backend);
    void shutdown();
    bool is_initialized() const { return m_initialized; }

    // Event pass-through — call before your own SDL event handling
    // Returns true if ImGui consumed the event (caller should not process it).
    bool process_event(const SDL_Event& event);

    // Frame management — wrap the game's GL draw calls
    void begin_frame();
    void end_frame();     // renders ImGui draw data into the GL context

    // The ImGui context this GUI owns (with the shared SDL3/GL3 backend).
    // Used by the boot loading screen so it draws on the SAME context/backend
    // rather than creating a second one. Null until init() runs.
    void* imgui_context() const { return m_imgui_ctx; }

    // Toggle visibility (called on F3)
    void toggle_visible() { m_visible = !m_visible; }
    bool is_visible()     const { return m_visible; }

    // Draw the debug panel — call between begin_frame()/end_frame() when visible
    void draw_debug(const SwordfareDebugStats& stats);

    // Draw the custom LUA-registered buttons and overlays using ImGui vector elements
    void draw_buttons(void* guest_buttons_ptr, void* guest_overlays_ptr, bool globally_hidden = false);

    // ---- Lua console backend ----
    // init_lua_console() is called once when the SRE console addresses are resolved,
    // update_console_backend() every frame. The window is in tool_windows_extra.cpp.
    struct ConsoleEntry { std::string text; bool is_error; bool is_input; };

    void init_lua_console(
        uint8_t* guest_memory,
        uint64_t buf_addr,
        uint64_t result_addr,
        uint64_t pending_addr,
        uint64_t status_addr,
        uint64_t print_addr);

    // Scene Shifter — guest memory address wiring.
    // Call once after libsre.so is loaded (same time as init_lua_console).
    // Pass g_guest_memory as base and each symbol's VA from get_symbol_vaddr().
    // After this, the GUI reads/writes scene shifter state in guest memory
    // instead of the (always-empty) host-side weak fallback copies.
    void init_scene_shifter(
        uint8_t* guest_memory,
        uint64_t scene_list_count_va,   // int  g_sre_scene_list_count
        uint64_t scene_list_va,          // char g_sre_scene_list[256][128]
        uint64_t shift_pending_va,       // volatile int  g_sre_scene_shift_pending
        uint64_t shift_target_va,        // char g_sre_scene_shift_target[128]
        uint64_t shift_spawn_va,         // char g_sre_scene_shift_spawn[64]
        uint64_t shift_error_va,         // char g_sre_scene_shift_last_error[256]
        uint64_t shift_active_va,        // int  g_sre_scene_shift_active
        uint64_t current_scene_va,       // char g_sre_current_scene_name[128]
        const std::string& assets_dir);  // path for host-side scan fallback

    // Scene list and scene change for the Scene Select tool window.
    struct SceneShifterState {
        int count = 0;                        // entries in names
        const char (*names)[128] = nullptr;
        const char* current = "";             // current scene, may be empty
        bool busy = false;                    // a scene change is pending or running
        const char* error = "";               // last error, empty when none
    };
    SceneShifterState scene_shifter_state();
    void scene_shifter_rescan();
    bool scene_shifter_go(const char* scene, const char* spawn, bool forced);  // false while busy

    // Lua console backend for the Lua Console tool window.
    const std::vector<ConsoleEntry>& console_history() const { return m_console_history; }
    void console_run(const std::string& code) { console_submit(code); }
    void console_clear() { m_console_history.clear(); }

    bool is_lua_console_ready() const { return m_console_ready; }
    void update_console_backend();

    // Public entry point used by the SWORDFARE_TCP_PORT debug env var
    void auto_start_tcp_server(int port) { start_tcp_server(port); }

    // Returns true if coordinates fall inside an active Lua mod overlay or button
    bool is_input_blocked(float mx, float my);

private:
    // Internal helpers
    void apply_swordfare_theme();
    void console_submit(const std::string& cmd); // write to guest + set pending

    SDL_Window*   m_window   = nullptr;
    SDL_GLContext m_gl_ctx   = nullptr;

public:
    // Hand the tool fonts to a surface that draws outside this class's own
    // layout.  void* rather than ImFont* so the header
    // stays free of an ImGui dependency.
    void* tool_font_ui() const   { return m_font_ui; }
    void* tool_font_mono() const { return m_font_mono; }
private:

    class VulkanBackend* m_vk_backend = nullptr;
    bool                 m_vulkan_active = false;
    VkDescriptorPool     m_imgui_vk_descriptor_pool = VK_NULL_HANDLE;
    void*         m_imgui_ctx = nullptr;   // ImGuiContext*
    void*         m_font_main = nullptr;   // ImFont*
    void*         m_font_button = nullptr; // ImFont* (display face — dialogs only)
    // ImFont*.  Tool surfaces: `ui` is a proportional UI face (Space Grotesk),
    // `mono` is fixed-width (JetBrains Mono) for addresses / hex / values.  Both
    // were previously unassigned, which silently left every PushFont site dead.
    void*         m_font_ui = nullptr;
    void*         m_font_mono = nullptr;

    bool          m_initialized = false;
    bool          m_visible     = false;
    bool          m_buttons_globally_hidden = false;

    void*         m_last_buttons_ptr = nullptr;
    void*         m_last_overlays_ptr = nullptr;

    // Smooth FPS graph data
    static constexpr int FPS_HISTORY = 90;
    float m_fps_history[FPS_HISTORY] = {};
    int   m_fps_idx = 0;

    // ---- Lua Console state ----
    bool          m_console_ready  = false;

    uint8_t*      m_guest_memory   = nullptr;
    uint64_t      m_console_buf_addr     = 0;
    uint64_t      m_console_result_addr  = 0;
    uint64_t      m_console_pending_addr = 0;
    uint64_t      m_console_status_addr  = 0;
    uint64_t      m_console_print_addr   = 0;

    // ---- Scene Shifter guest memory pointers ----
    // All non-zero after init_scene_shifter() succeeds.
    // Each is a VA in guest address space; add m_guest_memory to get host ptr.
    bool     m_scene_shifter_ready    = false;
    uint64_t m_ss_list_count_va       = 0;  // int
    uint64_t m_ss_list_va             = 0;  // char[256][128]
    uint64_t m_ss_pending_va          = 0;  // volatile int
    uint64_t m_ss_target_va           = 0;  // char[128]
    uint64_t m_ss_spawn_va            = 0;  // char[64]
    uint64_t m_ss_error_va            = 0;  // char[256]
    uint64_t m_ss_active_va           = 0;  // int
    uint64_t m_ss_current_scene_va    = 0;  // char[128]
    std::string m_ss_assets_dir;            // host path: ~/.../assets/resources

    static constexpr int CONSOLE_MAX_HISTORY = 4096;

    std::vector<ConsoleEntry>  m_console_history;

    // ---- TCP Console Server (openport command) ----
    void start_tcp_server(int port);
    void stop_tcp_server();
    void tcp_server_loop();

    std::thread                m_tcp_thread;
    std::atomic<bool>          m_tcp_running{false};
    int                        m_tcp_server_fd = -1;
    std::atomic<int>           m_tcp_client_fd{-1};

    // Command pipeline (reader thread -> main thread -> guest -> back).
    //
    // The reader thread enqueues each complete Lua chunk into m_tcp_cmd_queue.
    // The main thread (update_console_backend) drains ONE chunk at a time,
    // dispatches it to the guest, and — when the guest's result comes back —
    // writes the result to the socket and signals m_tcp_done_cv so the reader
    // (which may be waiting to keep output ordered) can continue. This replaces
    // the old single-slot m_tcp_pending_cmd design that raced on paste and
    // dropped lines. Each queued item carries the client fd it belongs to so a
    // late result is never written to a newer client.
    //
    // Connection GENERATION: a client fd can be recycled by the OS, so keying
    // ownership on the fd alone leaks a dead connection's queued/in-flight
    // results onto the next client. Every accepted connection bumps
    // m_tcp_client_gen; each queued command captures the generation it was
    // submitted under, and a result is only written to the socket when its
    // generation still matches the live connection. On connect/disconnect we
    // purge the queue and clear the in-flight slot so the dispatcher can never
    // wedge on a command whose owner has gone away.
    struct TcpCmd { std::string code; int client_fd; uint64_t gen; };
    std::deque<TcpCmd>         m_tcp_cmd_queue;      // FIFO of pending chunks
    std::mutex                 m_tcp_mutex;          // guards queue + in-flight
    std::condition_variable    m_tcp_done_cv;        // signalled on result done
    bool                       m_tcp_cmd_in_flight = false;
    int                        m_tcp_in_flight_fd  = -1; // fd that owns result
    uint64_t                   m_tcp_in_flight_gen = 0;  // gen that owns result
    uint64_t                   m_tcp_client_gen    = 0;  // current connection gen
    uint64_t                   m_tcp_completed_seq = 0;  // monotonic completion
};
