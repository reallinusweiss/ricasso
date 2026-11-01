// =============================================================================
// recovery_session_manager.h — Session-scoped epoch & scene-transition tracker
//
// Responsibilities:
//   1. Manages an alloc_epoch counter that increments on every scene
//      transition (level load / teardown). All InstanceRegistry entries
//      with a stale epoch are invalidated automatically.
//   2. Detects level changes by monitoring `GSC + 0x180` (updatesDisabled)
//      and the `Scene::pauseCount` field, cross-referenced with the
//      SceneObject root pointer changing between frames.
//   3. Provides a thin cooperative-pause API so the render thread can
//      request a safe-read window while g_gsc is being decoded (avoids
//      reading a half-initialized object after GotoLevel).
//   4. Emits "epoch changed" callbacks that flush InstanceRegistry,
//      MemWatchpointManager hit log, and RootWalker::last_result.
//
// Threading model:
//   • All public API is called from the RENDER/ImGui thread only.
//   • The emulator thread's MemoryWrite hooks are the only writers to
//     guest memory — they never call into this class.
//   • scene_tick() is called once per ImGui frame (render thread).
//     It is safe to read g_guest_memory from the render thread because
//     Dynarmic does NOT write to host-side state during render frames
//     (frames are discrete ticks interleaved with render calls).
// =============================================================================
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>

namespace swordfare::research {

// ---------------------------------------------------------------------------
// Epoch change callback — fired when a scene transition is detected.
// called from the render thread.
// ---------------------------------------------------------------------------
using EpochCallback = std::function<void(uint64_t new_epoch)>;

// ---------------------------------------------------------------------------
// SceneSnapshot — lightweight copy of scene-level identifiers taken each
// frame for change detection (all values read from guest memory).
// ---------------------------------------------------------------------------
struct SceneSnapshot {
    uint32_t gsc_va         = 0;
    uint64_t scene_va       = 0;
    uint64_t hero_va        = 0;
    int32_t  pause_count    = 0;
    bool     updates_disabled = false; // GSC+0x180 updatesDisabled flag
    uint64_t frame          = 0;
};

// ---------------------------------------------------------------------------
// RecoverySessionManager — singleton
// ---------------------------------------------------------------------------
class RecoverySessionManager {
public:
    static RecoverySessionManager& instance();

    // ── Configuration ──────────────────────────────────────────────────────
    // Call once after guest memory is mapped.
    void init(const uint8_t* guest_memory, uint64_t mem_size);

    // ── Per-frame (render thread) ──────────────────────────────────────────
    // Call once per ImGui frame with current live root VAs.
    // Detects scene transitions and fires epoch callbacks if needed.
    void scene_tick(uint32_t gsc_va, uint64_t scene_va, uint64_t hero_va);

    // ── Epoch ──────────────────────────────────────────────────────────────
    uint64_t current_epoch() const {
        return m_epoch.load(std::memory_order_relaxed);
    }

    // ── Callbacks ──────────────────────────────────────────────────────────
    // Register a function to be called when epoch increments.
    // Returns a handle that can be passed to unregister_callback().
    uint32_t register_callback(EpochCallback cb);
    void     unregister_callback(uint32_t handle);

    // ── State queries ──────────────────────────────────────────────────────
    bool is_in_transition() const { return m_in_transition; }
    const SceneSnapshot& last_snapshot() const { return m_last; }
    const SceneSnapshot& prev_snapshot() const { return m_prev; }

    // Force an epoch bump (e.g. manual "Reset session" button in UI).
    void force_epoch_bump(const std::string& reason = "manual");

    // Human-readable session log (last 32 entries) for the DB Status panel.
    struct LogEntry { uint64_t epoch; uint64_t frame; std::string reason; };
    std::vector<LogEntry> session_log() const;

    uint64_t frame_counter() const { return m_frame; }

private:
    RecoverySessionManager() = default;

    bool safe_read_u32(uint64_t va, uint32_t& out) const;
    bool safe_read_u64(uint64_t va, uint64_t& out) const;
    bool safe_read_i32(uint64_t va, int32_t&  out) const;

    void bump_epoch(const std::string& reason);
    void fire_callbacks(uint64_t new_epoch);

    const uint8_t* m_mem      = nullptr;
    uint64_t       m_mem_size = 0;
    bool           m_ready    = false;

    std::atomic<uint64_t> m_epoch{1};
    uint64_t m_frame = 0;

    SceneSnapshot m_last{};
    SceneSnapshot m_prev{};
    bool m_in_transition = false;

    // Hysteresis: require N consecutive frames of "different scene" before
    // declaring a transition, to avoid false positives during normal
    // hero teleports and camera cuts.
    static constexpr int kTransitionHysteresis = 3;
    int m_transition_frames = 0;

    mutable std::mutex m_mu;

    struct CallbackEntry { uint32_t handle; EpochCallback cb; };
    std::vector<CallbackEntry> m_callbacks;
    uint32_t m_next_handle = 1;

    std::vector<LogEntry> m_log;
    static constexpr size_t kMaxLogEntries = 64;
};

} // namespace swordfare::research
