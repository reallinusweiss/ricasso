// =============================================================================
// instance_registry.h — Session-scoped runtime instance tracker
//
// Maps static struct identities (from RecoveryCatalog) to the live guest VAs
// observed during this session.  The registry is NEVER persisted: it is
// cleared/marked stale on level load or game restart.
//
// Threading:
//   • All public methods take std::lock_guard on m_mu.
//   • tick() is called from the render thread (once per ImGui/game frame).
//   • observe() may be called from the root walker (render thread) or hooks.
// =============================================================================
#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <mutex>
#include <optional>

namespace swordfare::research {

// ---------------------------------------------------------------------------
// RuntimeInstance — one observed live instance of a known struct type
// ---------------------------------------------------------------------------
struct RuntimeInstance {
    uint32_t    id          = 0;   // session-unique monotonic ID
    std::string struct_name;       // matches CatalogStruct::name
    int         struct_db_id= 0;   // CatalogStruct::id
    uint64_t    guest_va    = 0;   // base address of this instance
    uint64_t    observed_at = 0;   // frame counter when first seen
    uint64_t    last_seen   = 0;   // frame counter of last observation
    std::string label;             // display label (e.g. "GSC", "Hero", "Enemy_04")
    bool        is_root     = false; // true for g_gsc, g_hero_obj etc.
    bool        stale       = false; // true after epoch invalidation
};

// ---------------------------------------------------------------------------
// InstanceRegistry — singleton
// ---------------------------------------------------------------------------
class InstanceRegistry {
public:
    static InstanceRegistry& instance();

    // Register or refresh a known instance.  Returns the RuntimeInstance::id.
    // If the (struct_name, guest_va) pair was already registered for that
    // struct, refreshes last_seen (and upgrades label / is_root if supplied).
    uint32_t observe(const std::string& struct_name, int struct_db_id,
                     uint64_t guest_va, const std::string& label = "",
                     bool is_root = false);

    // Mark all instances stale (call on level transition / scene teardown).
    void invalidate_all();

    // Remove stale instances that haven't been seen for N frames.
    // Root instances (is_root == true) are never pruned.
    void prune(uint64_t current_frame, uint64_t stale_threshold = 120);

    // Tick frame counter (call once per game frame).
    void tick() { m_frame++; }
    uint64_t frame() const { return m_frame; }

    // ── Lookup ────────────────────────────────────────────────────────────
    const RuntimeInstance* find_by_va(uint64_t va) const;
    const RuntimeInstance* find_by_id(uint32_t id) const;
    std::vector<const RuntimeInstance*> instances_of(const std::string& struct_name) const;
    std::vector<RuntimeInstance> all() const;

    // ── Stats ─────────────────────────────────────────────────────────────
    size_t count() const;
    size_t stale_count() const;

private:
    InstanceRegistry();
    InstanceRegistry(const InstanceRegistry&) = delete;
    InstanceRegistry& operator=(const InstanceRegistry&) = delete;

    mutable std::mutex           m_mu;
    std::vector<RuntimeInstance> m_instances;
    uint32_t                     m_next_id = 1;
    uint64_t                     m_frame   = 0;
};

} // namespace swordfare::research
