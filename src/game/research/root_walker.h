// =============================================================================
// root_walker.h — Bounded traversal of the live object graph from g_gsc
//
// Part of the Swordfare Live Memory Research System.
// Walks the verified pointer graph from g_gsc outward to enumerate live objects.
// Replaces brute-force heap scanning with a safe, bounded, cycle-detected traversal.
// =============================================================================
#pragma once

#include "game/research/recovery_catalog.h"
#include "game/research/vtable_classifier.h"
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_set>
#include <atomic>
#include <mutex>
#include <cstring>

namespace swordfare::research {

struct WalkedObject {
    uint64_t    va           = 0; // guest VA of this object
    std::string struct_name;      // from VtableClassifier or inferred
    std::string display_name;     // for Object Tree ("Hero", "Enemy 3", etc.)
    int         depth        = 0; // 0=roots, 1=scene objects, 2=components
    std::vector<uint64_t> component_vas; // populated for SceneObjects
};

struct WalkResult {
    std::vector<WalkedObject> objects;
    int         total_walked   = 0;
    int         classified     = 0;
    int         unclassified   = 0;
    bool        truncated      = false; // hit a cap
    uint64_t    gsc_va         = 0;
    uint64_t    scene_va       = 0;
    int32_t     pause_count    = 0;
    std::string walk_error;             // empty if OK
};

class RootWalker {
public:
    static RootWalker& instance();

    // One synchronous walk from g_gsc.
    // guest_memory: g_guest_memory. mem_size: 0xE0000000.
    // max_objects: hard cap on SceneObjects (default 512).
    WalkResult walk(uint64_t gsc_va,
                    const uint8_t* guest_memory,
                    uint64_t mem_size,
                    int max_objects = 512);

    // Request a walk (sets pending flag + stores parameters for render thread).
    void request_walk(uint64_t gsc_va,
                      const uint8_t* guest_memory,
                      uint64_t mem_size);

    // Convenience method: executes pending walk if flagged and returns last_result().
    const WalkResult& poll_pending_walk();

    const WalkResult& last_result() const {
        std::lock_guard<std::mutex> lock(m_mu);
        return m_last;
    }
    bool walk_pending() const { return m_pending.load(std::memory_order_acquire); }

private:
    RootWalker() = default;
    ~RootWalker() = default;
    RootWalker(const RootWalker&) = delete;
    RootWalker& operator=(const RootWalker&) = delete;

    // RB-tree traversal (in-order).
    // rb_header_va: address of the 24-byte RB-tree header.
    // node_offset_to_ptr: offset within node to the SceneObject*.
    void walk_rbtree(uint64_t rb_header_va,
                     const uint8_t* mem, uint64_t mem_size,
                     int max_nodes, WalkResult& out);

    // Decode SSO std::string at str_va → host std::string.
    std::string decode_sso(uint64_t str_va, const uint8_t* mem, uint64_t mem_size);

    // Safe read helpers
    bool read_u32(uint64_t va, const uint8_t* m, uint64_t sz, uint32_t& out);
    bool read_u64(uint64_t va, const uint8_t* m, uint64_t sz, uint64_t& out);

    WalkResult m_last;
    std::atomic<bool> m_pending{false};
    uint64_t       m_pending_gsc_va = 0;
    const uint8_t* m_pending_mem    = nullptr;
    uint64_t       m_pending_mem_sz = 0;
    mutable std::mutex m_mu;
};

} // namespace swordfare::research
