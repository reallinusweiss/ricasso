// =============================================================================
// recovery_session_manager.cpp
// =============================================================================
#include "game/research/recovery_session_manager.h"
#include "game/research/instance_registry.h"
#include "game/research/mem_watchpoint.h"
#include <cstring>
#include <algorithm>
#include <iostream>

#define SWORDFARE_HAVE_INSTANCE_REGISTRY 1

namespace swordfare::research {

RecoverySessionManager& RecoverySessionManager::instance() {
    static RecoverySessionManager s;
    return s;
}

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------
void RecoverySessionManager::init(const uint8_t* guest_memory, uint64_t mem_size) {
    std::lock_guard<std::mutex> lg(m_mu);
    m_mem      = guest_memory;
    m_mem_size = mem_size;
    m_ready    = true;
}

// ---------------------------------------------------------------------------
// Safe read helpers
// ---------------------------------------------------------------------------
bool RecoverySessionManager::safe_read_u32(uint64_t va, uint32_t& out) const {
    if (!m_mem || va < 0x1000 || va + 4 > m_mem_size) return false;
    std::memcpy(&out, m_mem + va, 4);
    return true;
}
bool RecoverySessionManager::safe_read_u64(uint64_t va, uint64_t& out) const {
    if (!m_mem || va < 0x1000 || va + 8 > m_mem_size) return false;
    std::memcpy(&out, m_mem + va, 8);
    return true;
}
bool RecoverySessionManager::safe_read_i32(uint64_t va, int32_t& out) const {
    uint32_t raw = 0;
    if (!safe_read_u32(va, raw)) return false;
    out = static_cast<int32_t>(raw);
    return true;
}

// ---------------------------------------------------------------------------
// scene_tick — called once per ImGui frame (render thread)
// ---------------------------------------------------------------------------
void RecoverySessionManager::scene_tick(uint32_t gsc_va,
                                         uint64_t scene_va,
                                         uint64_t hero_va) {
    if (!m_ready) return;

    m_frame++;

    // ── Build current snapshot ─────────────────────────────────────────────
    SceneSnapshot cur;
    cur.gsc_va  = gsc_va;
    cur.scene_va = scene_va;
    cur.hero_va  = hero_va;
    cur.frame    = m_frame;

    // Read updatesDisabled at GSC+0x180
    if (gsc_va != 0) {
        uint32_t upd = 0;
        safe_read_u32((uint64_t)gsc_va + 0x180, upd);
        cur.updates_disabled = (upd != 0);
    }

    // Read pauseCount at Scene+0x20
    if (scene_va != 0 && scene_va >= 0x10000 && scene_va < m_mem_size) {
        safe_read_i32(scene_va + 0x20, cur.pause_count);
    }

    m_prev = m_last;
    m_last = cur;

    // ── Change detection ───────────────────────────────────────────────────
    // A scene transition is signalled by:
    //   (a) the scene_va pointer changes (different Scene object), OR
    //   (b) updatesDisabled became true (GotoLevel teardown in progress)
    // We require kTransitionHysteresis consecutive frames before committing.

    bool scene_changed = (m_prev.scene_va != 0) &&
                         (cur.scene_va != m_prev.scene_va);
    bool teardown      = cur.updates_disabled && !m_prev.updates_disabled;

    if (scene_changed || teardown) {
        m_transition_frames++;
    } else {
        // If updatesDisabled cleared and we were in transition → commit
        if (m_in_transition && !cur.updates_disabled && m_prev.updates_disabled) {
            // Transition finished — bump epoch
            m_in_transition = false;
            m_transition_frames = 0;
            bump_epoch(teardown ? "GotoLevel teardown" : "scene VA changed");
            return;
        }
        m_transition_frames = 0;
    }

    if (!m_in_transition && m_transition_frames >= kTransitionHysteresis) {
        m_in_transition = true;
        std::cout << "[RSM] Scene transition detected (epoch "
                  << m_epoch.load() << " → " << m_epoch.load() + 1 << ")\n";
        // If updatesDisabled is the trigger, wait for it to clear before bump.
        // If it's just a scene_va change (teleport / instant load), bump now.
        if (!cur.updates_disabled) {
            m_in_transition = false;
            m_transition_frames = 0;
            bump_epoch("scene VA changed (instant)");
        }
    }
}

// ---------------------------------------------------------------------------
// bump_epoch — increment epoch and notify all subsystems
// ---------------------------------------------------------------------------
void RecoverySessionManager::bump_epoch(const std::string& reason) {
    uint64_t new_epoch = m_epoch.fetch_add(1, std::memory_order_acq_rel) + 1;

    // Log it
    {
        std::lock_guard<std::mutex> lg(m_mu);
        m_log.push_back({new_epoch, m_frame, reason});
        if (m_log.size() > kMaxLogEntries)
            m_log.erase(m_log.begin());
    }

    std::cout << "[RSM] Epoch " << new_epoch << " — " << reason << "\n";

    // Invalidate all tracked instances (requires Agent 2's instance_registry.h)
#if SWORDFARE_HAVE_INSTANCE_REGISTRY
    InstanceRegistry::instance().invalidate_all();
#endif

    // Clear the watchpoint hit log (stale addresses after scene change)
    MemWatchpointManager::instance().clear_hits();

    // Fire registered callbacks (render thread only → no extra locking needed)
    fire_callbacks(new_epoch);
}

void RecoverySessionManager::fire_callbacks(uint64_t new_epoch) {
    std::vector<CallbackEntry> snap;
    {
        std::lock_guard<std::mutex> lg(m_mu);
        snap = m_callbacks;
    }
    for (auto& e : snap) {
        if (e.cb) e.cb(new_epoch);
    }
}

// ---------------------------------------------------------------------------
// force_epoch_bump
// ---------------------------------------------------------------------------
void RecoverySessionManager::force_epoch_bump(const std::string& reason) {
    bump_epoch(reason);
}

// ---------------------------------------------------------------------------
// Callback registration
// ---------------------------------------------------------------------------
uint32_t RecoverySessionManager::register_callback(EpochCallback cb) {
    std::lock_guard<std::mutex> lg(m_mu);
    uint32_t h = m_next_handle++;
    m_callbacks.push_back({h, std::move(cb)});
    return h;
}

void RecoverySessionManager::unregister_callback(uint32_t handle) {
    std::lock_guard<std::mutex> lg(m_mu);
    m_callbacks.erase(
        std::remove_if(m_callbacks.begin(), m_callbacks.end(),
            [handle](const CallbackEntry& e){ return e.handle == handle; }),
        m_callbacks.end());
}

// ---------------------------------------------------------------------------
// session_log
// ---------------------------------------------------------------------------
std::vector<RecoverySessionManager::LogEntry>
RecoverySessionManager::session_log() const {
    std::lock_guard<std::mutex> lg(m_mu);
    return m_log;
}

} // namespace swordfare::research
