// =============================================================================
// live_object_map.h — the STATIC → RUNTIME → OBJECT → STRUCT pipeline.
//
// This is the part that a cross-process editor (CE, GG) structurally cannot do.
// They hold a raw runtime address and lose the object the moment the guest
// moves it.  Swordfare owns the guest address space, so it can re-run the
// pointer chain every frame and update only the RUNTIME side of this pipeline:
//
//     STATIC      libswordigo.so + RVA 0x1A2B3C     (never changes)
//         ↓  load bias, tracked live
//     RUNTIME     0x71A50090                        (may change every frame)
//         ↓  resolved type
//     OBJECT      GameSceneController               (never changes)
//         ↓  field layout
//     STRUCT      +0x90 currentHealth / +0xA0 VAR_0750   (never changes)
//
// When the object moves, only `runtime_va` is rewritten.  Object identity, field
// identity and static identity are untouched — that is the entire point, and
// `moved()` is what tells the UI a re-resolution happened rather than the
// researcher having silently started looking at a different object.
//
// Identity still never comes from an address: every node's identity is derived
// from its StaticRef (module + RVA + offset + type), never from `runtime_va`.
// =============================================================================
#pragma once

#include "game/research/mem_identity.h"

#include <cstdint>
#include <string>
#include <vector>

namespace swordfare::research {

// ---------------------------------------------------------------------------
// ModuleMap — the RVA ↔ runtime VA translation for the loaded image.
//
// The guest maps libswordigo at a base that varies per boot/run (ELF p_vaddr is
// 0 for this .so, so runtime VA == base + RVA).  Everything the map reports is
// normalised through here, so nothing downstream ever depends on the raw VA.
// ---------------------------------------------------------------------------
struct ModuleExtent {
    std::string name;          // "libswordigo.so"
    uint64_t    base_va  = 0;  // runtime address of the image base
    uint64_t    rva_begin = 0; // lowest RVA covered (usually 0)
    uint64_t    rva_end   = 0; // highest RVA covered (text+data size)
    std::string build_id;      // "sre13-1.4.13-arm64"

    bool contains_va(uint64_t va) const {
        return va >= base_va && (va - base_va) < (rva_end - rva_begin) + 1;
    }
};

class ModuleMap {
public:
    void clear();
    void set(const ModuleExtent& m);          // replace/create by name
    void clear_modules() { m_modules.clear(); }

    size_t size() const { return m_modules.size(); }
    const std::vector<ModuleExtent>& modules() const { return m_modules; }

    const ModuleExtent* find(const std::string& name) const;

    // Normalise a live pointer into (module, RVA).  false when the address does
    // not belong to any known image (stack, heap, JIT arena).
    bool rva_of(uint64_t va, std::string* module, uint64_t* rva) const;

    // Re-materialise: static RVA -> current runtime address.
    bool va_of(const std::string& module, uint64_t rva, uint64_t* va) const;

    // Human readable "libswordigo.so+0x1A2B3C", or "0x1234 (unmapped)".
    std::string describe(uint64_t va) const;

    std::string summary() const;

private:
    std::vector<ModuleExtent> m_modules;
};

// ---------------------------------------------------------------------------
// Pointer paths — CE-style multi-level pointers, resolved lazily.
//
// Semantics match Cheat Engine's, because that is what a researcher will
// expect: the root static slot is read first, THEN offsets[0] is applied to
// that pointer, then offsets[1] to the next one, and so on.  The final offset
// is added to the last pointer without dereferencing it (that is the field
// itself).  With one offset the path is "pointer-to-field"; with two it is the
// familiar [ [root]+a ]+b.  The root is a static site (module + RVA), so the
// whole path is deterministic and re-resolvable on any boot.
// ---------------------------------------------------------------------------
struct PointerPath {
    std::string          module;      // module holding the root pointer
    uint64_t             root_rva = 0;// static location of the first pointer
    std::vector<uint64_t> offsets;    // dereference offsets; last one is the field

    bool valid() const { return !offsets.empty() && root_rva != 0; }
};

struct PointerResolution {
    bool        ok        = false;
    uint64_t    address   = 0;    // final runtime address
    std::string module;           // module the final address lives in (if any)
    uint64_t    final_rva = 0;    // final RVA when mapped
    std::vector<uint64_t> hops;   // each intermediate pointer, for the UI
    std::string error;            // why it failed, when ok == false
    std::string describe() const;
};

// ---------------------------------------------------------------------------
// A watched field inside a resolved object.
// ---------------------------------------------------------------------------
struct ObjectField {
    StaticRef   ref;                 // deterministic identity of the field
    Identity    identity;
    uint64_t    offset       = 0;    // offset inside the object
    uint64_t    width        = 4;
    std::string type_name;           // "float", "int32", "Vector2f"
    std::string recovered_name;      // "" when undiscovered
    Provenance  provenance   = Provenance::Unknown;
    std::string last_value;          // cached render value

    std::string display_name() const {
        // The recovered name is only a *name* once the field is actually proven;
        // otherwise the deterministic placeholder is the honest label.
        if (provenance_is_proven(provenance) && !recovered_name.empty())
            return recovered_name;
        return identity.base_name;
    }
};

// ---------------------------------------------------------------------------
// A live object: one root pointer plus the fields we know about it.
// ---------------------------------------------------------------------------
struct LiveObject {
    uint64_t    id = 0;
    std::string name;                 // "GameSceneController" (root label)
    StaticRef   type_ref;             // identity of the OBJECT (the type)
    Identity    identity;
    std::string type_name;

    std::string static_module;        // where the pointer itself lives
    uint64_t    static_rva = 0;

    uint64_t    runtime_va   = 0;     // current guest address (only mutable part)
    uint64_t    previous_va  = 0;
    uint64_t    byte_size    = 0;
    uint64_t    move_count   = 0;     // how many times it has been re-resolved
    uint64_t    last_seen_frame = 0;
    bool        alive        = false; // pointer non-null and mapped last refresh

    std::vector<ObjectField> fields;

    bool moved() const { return previous_va != 0 && previous_va != runtime_va; }
    std::string display_name() const {
        return identity.base_name.empty() ? name : identity.base_name;
    }
};

// ---------------------------------------------------------------------------
// ObjectMapConfig — one root pointer the engine hands us (SRE's gsc_get(), a
// global slot, a vtable-derived pointer...).
// ---------------------------------------------------------------------------
struct ObjectRoot {
    std::string label;            // "GameSceneController"
    std::string module;           // module the pointer slot lives in
    uint64_t    pointer_rva = 0;  // static RVA of the pointer slot
    uint64_t    byte_size   = 0;  // size of the pointed-to object, 0 = unknown
    MemKind     kind        = MemKind::HeapInstance;
    std::string struct_name;      // owning struct name for identity
    std::string build_id;

    // Optional: when the engine gives us the pointer directly (a host-side
    // accessor like SRE's gsc_get()), there is no static pointer slot to read.
    bool        direct_value = false;
    uint64_t    direct_va    = 0;
};

class LiveObjectMap {
public:
    void set_modules(const ModuleMap& m) { m_modules = m; }
    const ModuleMap& modules() const { return m_modules; }

    // Register a root.  Returns its stable id.
    uint64_t add_root(const ObjectRoot& root);

    // Attach a field layout to a root (offsets from the recovered catalog).
    void set_fields(uint64_t root_id, std::vector<ObjectField> fields);

    // Feed an engine-provided pointer for a `direct_value` root (SRE gsc_get).
    bool set_direct_pointer(uint64_t root_id, uint64_t va);

    // Re-resolve every root against guest memory.  Returns how many objects
    // moved (i.e. had their runtime address rewritten while staying the same
    // object).  This is the function the emulator tick calls.
    size_t refresh(uint8_t* mem, uint64_t mem_size, uint64_t frame);

    // Read every known field of every live object into `last_value`.
    size_t sample_fields(const uint8_t* mem, uint64_t mem_size);

    bool remove_root(uint64_t id);
    size_t size() const;
    void clear() { m_objects.clear(); m_next_id = 1; }

    // Snapshot for the UI (copy; cheap for the handful of roots we track).
    std::vector<LiveObject> snapshot() const;
    const LiveObject*       find(uint64_t id) const;
    const LiveObject*       find_by_va(uint64_t va) const;

    // Index used for identity/instance naming of everything we resolve here.
    LiveMemoryIndex& index() { return m_index; }
    const LiveMemoryIndex& index() const { return m_index; }

    uint64_t total_moves() const { return m_total_moves; }
    std::string summary() const;

private:
    ModuleMap                  m_modules;
    std::vector<LiveObject>    m_objects;
    LiveMemoryIndex            m_index;
    uint64_t                   m_next_id    = 1;
    uint64_t                   m_total_moves = 0;
};

// ---------------------------------------------------------------------------
// Free helpers — pointer-path resolution, usable on their own (the address list
// uses them for multi-level "pointer" entries).
// ---------------------------------------------------------------------------
PointerResolution resolve_pointer_path(const PointerPath& path,
                                       const ModuleMap& modules,
                                       const uint8_t* mem, uint64_t mem_size);

// Read a qword, bounds-checked.  Exposed because both the resolver and the
// address list need it and neither should hand-roll the bounds rule.
bool mem_read_pointer(const uint8_t* mem, uint64_t mem_size, uint64_t addr, uint64_t* out);

// The load bias is the single fact that makes RVA-space identity possible.
// Returns false when the module is unknown.
bool load_bias_of(const ModuleMap& modules, const std::string& module, uint64_t* bias);

} // namespace swordfare::research
