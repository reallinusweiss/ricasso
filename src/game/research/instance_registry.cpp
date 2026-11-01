// =============================================================================
// instance_registry.cpp — Session-scoped runtime instance tracker
// =============================================================================

#include "game/research/instance_registry.h"

#include <algorithm>

namespace swordfare::research {

// ---------------------------------------------------------------------------
// Construction / singleton
// ---------------------------------------------------------------------------
InstanceRegistry::InstanceRegistry() {
    // Preallocate so the observe() hot path never grows the vector mid-frame.
    m_instances.reserve(256);
}

InstanceRegistry& InstanceRegistry::instance() {
    static InstanceRegistry s;
    return s;
}

// ---------------------------------------------------------------------------
// observe — register or refresh
// ---------------------------------------------------------------------------
uint32_t InstanceRegistry::observe(const std::string& struct_name,
                                   int struct_db_id,
                                   uint64_t guest_va,
                                   const std::string& label,
                                   bool is_root) {
    std::lock_guard<std::mutex> lk(m_mu);

    // Already tracked for this struct at this VA?  Refresh it.
    for (auto& inst : m_instances) {
        if (inst.guest_va == guest_va && inst.struct_name == struct_name) {
            inst.last_seen    = m_frame;
            inst.stale        = false;
            if (struct_db_id != 0) inst.struct_db_id = struct_db_id;
            if (!label.empty())    inst.label        = label;
            if (is_root)           inst.is_root      = true;
            return inst.id;
        }
    }

    // New instance.  Growth is rare thanks to the reserve() above, and
    // observe() runs from the render/hook thread, never the emulator hot path.
    RuntimeInstance inst;
    inst.id           = m_next_id++;
    inst.struct_name  = struct_name;
    inst.struct_db_id = struct_db_id;
    inst.guest_va     = guest_va;
    inst.observed_at  = m_frame;
    inst.last_seen    = m_frame;
    inst.label        = label;
    inst.is_root      = is_root;
    inst.stale        = false;
    m_instances.push_back(std::move(inst));
    return m_instances.back().id;
}

// ---------------------------------------------------------------------------
// Epoch invalidation
// ---------------------------------------------------------------------------
void InstanceRegistry::invalidate_all() {
    std::lock_guard<std::mutex> lk(m_mu);
    for (auto& inst : m_instances)
        inst.stale = true;
}

// ---------------------------------------------------------------------------
// prune — drop stale, non-root instances unseen for >= threshold frames
// ---------------------------------------------------------------------------
void InstanceRegistry::prune(uint64_t current_frame, uint64_t stale_threshold) {
    std::lock_guard<std::mutex> lk(m_mu);

    m_instances.erase(
        std::remove_if(m_instances.begin(), m_instances.end(),
                       [&](const RuntimeInstance& inst) {
                           if (!inst.stale || inst.is_root) return false;
                           const uint64_t unseen =
                               (current_frame >= inst.last_seen)
                                   ? (current_frame - inst.last_seen)
                                   : 0;
                           return unseen >= stale_threshold;
                       }),
        m_instances.end());
}

// ---------------------------------------------------------------------------
// Lookup
// ---------------------------------------------------------------------------
const RuntimeInstance* InstanceRegistry::find_by_va(uint64_t va) const {
    std::lock_guard<std::mutex> lk(m_mu);
    for (const auto& inst : m_instances) {
        if (inst.guest_va == va)
            return &inst;
    }
    return nullptr;
}

const RuntimeInstance* InstanceRegistry::find_by_id(uint32_t id) const {
    std::lock_guard<std::mutex> lk(m_mu);
    for (const auto& inst : m_instances) {
        if (inst.id == id)
            return &inst;
    }
    return nullptr;
}

std::vector<const RuntimeInstance*>
InstanceRegistry::instances_of(const std::string& struct_name) const {
    std::lock_guard<std::mutex> lk(m_mu);
    std::vector<const RuntimeInstance*> out;
    for (const auto& inst : m_instances) {
        if (inst.struct_name == struct_name)
            out.push_back(&inst);
    }
    return out;
}

std::vector<RuntimeInstance> InstanceRegistry::all() const {
    std::lock_guard<std::mutex> lk(m_mu);
    return m_instances;   // heap-safe copy for the UI
}

// ---------------------------------------------------------------------------
// Stats
// ---------------------------------------------------------------------------
size_t InstanceRegistry::count() const {
    std::lock_guard<std::mutex> lk(m_mu);
    return m_instances.size();
}

size_t InstanceRegistry::stale_count() const {
    std::lock_guard<std::mutex> lk(m_mu);
    size_t n = 0;
    for (const auto& inst : m_instances)
        if (inst.stale) ++n;
    return n;
}

} // namespace swordfare::research
