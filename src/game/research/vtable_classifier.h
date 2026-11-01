// =============================================================================
// vtable_classifier.h — Object concrete type classification via vptr
//
// Part of the Swordfare Live Memory Research System.
// Given a guest virtual address (VA) pointing to an object, this classifies
// its concrete C++ type by reading its 64-bit vptr (at offset 0).
//
// ── Address spaces (the important part) ─────────────────────────────────────
// libswordigo.so is mapped at a load base (main.cpp uses 0x1000000), so every
// pointer stored in guest memory — including an object's vptr — is a RELOCATED
// VA.  The recovery catalog, by contrast, publishes module-relative RVAs
// (structs.vtable_rva / vtables.vtable_rva / target_func_rva), the same space
// IDA and the OpenSwordigo corpora use.  A lookup that compares a live vptr
// against a catalog RVA can never match, so classify() normalizes the observed
// vptr into RVA space first (see set_module_window()).
//
// ── Two independent strategies ──────────────────────────────────────────────
//  1. RTTI (primary): the Itanium ABI keeps the object's `type_info*` at
//     vptr-8 and the mangled type name inside it.  That yields the concrete
//     class name with NO dependency on any vtable address table — it works for
//     any object whose class has RTTI, including ones the catalog has never
//     seen.  This is what makes classification succeed even when the catalog's
//     vtable RVAs are stale.
//  2. Known-vtable table (fallback): catalog vtables + the compiled-in seeds,
//     matched in RVA space as described above.
// =============================================================================
#pragma once

#include "game/research/recovery_catalog.h"
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace swordfare::research {

struct ClassificationResult {
    bool        matched    = false;
    std::string struct_name;     // e.g. "SceneObject"
    uint64_t    vptr_va    = 0;   // the raw vptr read from *obj (a runtime VA)
    uint64_t    vptr_rva   = 0;   // the same vptr normalized to module-relative form
    int         catalog_id = -1;  // CatalogStruct::id or -1
    std::string confidence;       // "RTTI" (type_info) / "KNOWN" (vtable table) / "UNKNOWN"
};

class VtableClassifier {
public:
    static VtableClassifier& instance();

    // Load vtable map from RecoveryCatalog (call once after catalog init).
    // Also populates the hardcoded confirmed table as fallback.
    void init();
    bool is_ready() const;

    // The mapped range of libswordigo in guest memory.  Catalog vtable keys are
    // RVAs relative to `base`.  Defaults match main.cpp's load_addr (0x1000000)
    // and the libswordigo window the ARM64 backend already recognises.
    void     set_module_window(uint64_t base, uint64_t end);
    uint64_t module_base() const;
    uint64_t module_end()  const;

    // Register a known vtable keyed by its module-relative RVA.
    void register_vtable(uint64_t vptr_rva, const std::string& struct_name,
                         int catalog_id);

    // Classify the object at guest_va.
    // guest_va: the object's base address.
    // guest_memory: g_guest_memory. mem_size: 0xE0000000 (a smaller buffer is
    // accepted — every read is bounded by it).
    ClassificationResult classify(uint64_t obj_va,
                                  const uint8_t* guest_memory,
                                  uint64_t mem_size) const;

    // Bulk: classify all VAs in a list. Returns parallel result vector.
    std::vector<ClassificationResult> classify_many(
        const std::vector<uint64_t>& vas,
        const uint8_t* guest_memory, uint64_t mem_size) const;

    // Register a runtime observation: "at VA X we saw vptr Y which we think is Type Z"
    // Writes to user_research.db via RecoveryCatalog::record_runtime_observation.
    void record_observation(uint64_t obj_va, uint64_t vptr_va,
                            const std::string& guessed_name);

    // Dump the known vptr table for ImGui display.
    // NOTE: `vptr_va` holds the module-relative RVA key, not a runtime VA.
    struct VtableEntry {
        uint64_t    vptr_va;
        std::string struct_name;
        int         catalog_id;
    };
    std::vector<VtableEntry> known_vtables() const;

    // ── Diagnostics ───────────────────────────────────────────────────────
    // One-line report of how many known vtable keys actually resolve to a real
    // vtable in guest memory, plus a bounded sample of vptr RVAs that failed to
    // classify.  Filled in lazily by the first classify() call that is given
    // guest memory, so it is the fastest way to answer "are the catalog's
    // vtable addresses right for the binary we are running?".
    std::string probe_summary() const;

    // How many classify() calls matched by RTTI / by table / not at all.
    void stats(uint64_t* rtti, uint64_t* table, uint64_t* unknown) const;

private:
    VtableClassifier();
    ~VtableClassifier() = default;
    VtableClassifier(const VtableClassifier&) = delete;
    VtableClassifier& operator=(const VtableClassifier&) = delete;

    // Normalize a runtime VA to module-relative RVA space (and tolerate input
    // that is already an RVA).
    uint64_t rva_of(uint64_t va) const;

    // Read the mangled class name out of the object's RTTI, reduced to its last
    // identifier token ("N5Caver19GameSceneControllerE" -> "GameSceneController").
    // Returns an empty string when the object has no usable RTTI.
    std::string rtti_name_at(const uint8_t* mem, uint64_t mem_size,
                             uint64_t obj_va) const;

    // One-time sweep: for every known key, read the slot it points at and check
    // whether it looks like a real vtable (first slot in the module window).
    void run_probe_locked(const uint8_t* mem, uint64_t mem_size) const;

    bool m_ready = false;

    // Module-relative RVA -> struct_name, catalog_id.
    // Populated from BOTH hardcoded confirmed table AND catalog vtables.
    std::unordered_map<uint64_t, VtableEntry> m_map;

    uint64_t m_module_base = 0x1000000;
    uint64_t m_module_end  = 0x3000000;

    // ── Diagnostics state (mutable: filled from const classify()) ──────────
    mutable bool                  m_probe_done    = false;
    mutable int                   m_probe_valid   = 0;
    mutable int                   m_probe_checked = 0;
    mutable std::vector<uint64_t> m_unmatched_rvas;   // bounded sample
    mutable uint64_t              m_hits_rtti   = 0;
    mutable uint64_t              m_hits_table  = 0;
    mutable uint64_t              m_misses      = 0;

    mutable std::mutex m_mu;
};

} // namespace swordfare::research
