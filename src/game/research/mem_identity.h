// =============================================================================
// mem_identity.h — Deterministic memory identity for the live inspector
//
// TARGET: a GameGuardian-class live memory workspace where *nothing* is ever
// identified by its runtime address.
//
// The runtime address is temporary and moves on every boot (and on every scene
// change, thread switch and allocation).  Everything a researcher accumulates —
// names, notes, proven offsets, screenshots of a value's behaviour — must
// survive that.  So Swordfare splits identity in two:
//
//   BASE IDENTITY (deterministic, boot-independent)
//     build_id + module + kind + container + offset + type
//        -> var_id    (FNV-1a 64 of the canonical key; never changes)
//        -> base_name ("currentHealth" | "VAR_750" | "sub_00156120_VAR_750")
//
//   INSTANCE TAG (runtime, may change per boot — by design)
//     the Nth live copy of that same static site, e.g. "currentHealth_2726"
//
// Why this split is forced on us: Swordigo is data-driven, so ONE static field
// (say Vector2f.x inside the shared seal/scene-object layout) is instantiated
// thousands of times per frame across unrelated objects.  Naming them all the
// same is useless; naming them by address is useless across boots.  So the base
// name states *what static thing this is* and the tag states *which live copy*.
//
//   * Recovered offsets keep their recovered name as the base name.
//   * Undiscovered offsets get a VAR_<n> base name whose <n> is derived from the
//     deterministic hash — the SAME offset always yields the SAME number, no
//     matter how many times the game is booted or where the player is standing.
//   * Where the engine hands us a stable anchor for an instance (a SceneObject's
//     own name, an entity id, a spawn identifier) we derive the tag from that
//     anchor instead of from randomness, so it is stable too.  Where there is no
//     anchor, the tag is per-session random and is explicitly labelled as such.
//
// Provenance is first-class and never a guess dressed up as a fact:
//   Unknown < Observed < Inferred < Recovered < Confirmed
// =============================================================================
#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace swordfare::research {

// ---------------------------------------------------------------------------
// What kind of memory slot are we looking at?  Different kinds carry different
// identity systems (a global is identified by its RVA, a stack local by
// function+stack offset, a struct field by struct+offset, a heap object by its
// provenance chain).
// ---------------------------------------------------------------------------
enum class MemKind : uint8_t {
    Unknown = 0,
    Global,        // module static/global: identified by module + RVA
    StackLocal,    // identified by function RVA + stack offset
    StructField,   // identified by struct identity + field offset
    HeapInstance,  // identified by its provenance chain (type + anchor + parent)
    VtablePtr,     // identified by module + RVA of the vtable
    String,        // char*/std::string payload
    ArrayElement,  // index into an array/vector/set container
};

const char* mem_kind_name(MemKind k);

// ---------------------------------------------------------------------------
// Provenance — how Swordfare knows what it thinks it knows.
// ---------------------------------------------------------------------------
// Ordering is meaningful and is used for the forward-only promote() rule, so
// the values are never reordered: Unknown < Observed < Inferred < Correlated
// < Recovered < Confirmed.
enum class Provenance : uint8_t {
    Unknown = 0,   // nothing: raw bytes we have not tied to anything
    Observed,      // seen at runtime only (a write/read was caught)
    Inferred,      // derived from layout/relationships, not proven
    Correlated,    // tied to a specific static instruction (PC + operand), but
                   // still only evidence — never treated as proven
    Recovered,     // recovered from the static binary by RE work
    Confirmed,     // a researcher explicitly confirmed it
};

const char* provenance_name(Provenance p);
const char* provenance_badge(Provenance p);   // short UI marker: "?", "~", "OK", "*"
bool        provenance_is_proven(Provenance p);

// ---------------------------------------------------------------------------
// StaticRef — the deterministic description of a static memory site.
// This is the ONLY thing identity is computed from.
// ---------------------------------------------------------------------------
struct StaticRef {
    std::string build_id;      // e.g. "sre13-1.4.13-arm64" (empty = unknown build)
    std::string module;        // e.g. "libswordigo.so"
    MemKind     kind          = MemKind::Unknown;
    std::string struct_name;   // owning struct ("GameSceneController"), if any
    uint64_t    container_rva = 0;   // module RVA of the container: struct base,
                                     // function, or the global itself.  0 = n/a
    uint64_t    offset        = 0;   // offset inside that container
    std::string type_name;     // "float", "Vector2f", "int32" (may be empty)
    std::string recovered_name;// known name if recovered ("" when undiscovered)
    Provenance  provenance    = Provenance::Unknown;

    // Human-readable static key, also the hash input.  Deterministic.
    std::string canonical() const;
};

// ---------------------------------------------------------------------------
// Identity — the deterministic, boot-independent answer.
// ---------------------------------------------------------------------------
struct Identity {
    uint64_t    var_id = 0;      // FNV-1a 64 of canonical(); stable forever
    std::string base_name;       // "currentHealth" | "VAR_750" | "sub_..._VAR_750"
    std::string canonical;       // the exact key the hash came from
    Provenance  provenance = Provenance::Unknown;
    bool        deterministic = true;  // false only for the instance tag
};

// ---------------------------------------------------------------------------
// InstanceTag — which live copy of a static site this is.
// ---------------------------------------------------------------------------
enum class TagSource : uint8_t {
    None = 0,       // no live copy yet
    Provenance,     // derived from a stable engine-provided anchor (deterministic)
    Session,        // per-session random (labelled as such; may differ per boot)
};

struct InstanceTag {
    uint32_t  value  = 0;
    TagSource source = TagSource::None;

    // "2726" style suffix, or "" when there is no tag.
    std::string suffix_str() const;
    const char* source_name() const;
};

// ---------------------------------------------------------------------------
// IdentityResolver — pure function from StaticRef to Identity.
// ---------------------------------------------------------------------------
class IdentityResolver {
public:
    // Deterministic base identity.  Same input -> same var_id and base_name on
    // every boot, on every machine, forever.
    static Identity resolve(const StaticRef& ref);

    // Compose the displayed instance name: base + "_" + tag ("VAR_750_2726").
    static std::string instance_name(const std::string& base_name, const InstanceTag& tag);

    // Deterministic tag from a stable engine anchor (a SceneObject name, entity
    // id, spawn id...).  Same anchor -> same tag, across boots.
    static uint32_t stable_tag_value(const std::string& provenance_key);

    // The 4-digit short handle used inside VAR_/sub_ names.
    static uint32_t short_handle(uint64_t var_id);
};

// ---------------------------------------------------------------------------
// session_scoped_site_name — tier C naming.
//
// A stack/heap/JIT slot has no static anchor, so there is nothing for a
// permanent name to be derived from.  Rather than invent one, the name is
// prefixed (so it can never be read as a static site) and derived from the
// runtime address (so it is at least stable within a session).  Everything that
// IS provable is what the two tiers above are for.
// ---------------------------------------------------------------------------
std::string session_scoped_site_name(uint64_t runtime_va);

// ---------------------------------------------------------------------------
// LiveMemoryIndex — runtime VA <-> identity binding, multi-instance aware.
// ---------------------------------------------------------------------------
struct LiveMemoryEntry {
    uint64_t    runtime_va    = 0;
    Identity    identity;                    // base (deterministic) identity
    InstanceTag tag;                         // which live copy
    std::string display_name;                // "VAR_750_2726"
    std::string provenance_key;              // anchor used for the tag, if any
    uint64_t    first_seen_frame = 0;
    uint64_t    last_seen_frame  = 0;
    bool        stale = false;
};

class LiveMemoryIndex {
public:
    // Bind (or refresh) the live copy at runtime_va.  Returns the display name.
    // `provenance_key` is an optional stable anchor for this instance.
    std::string bind(uint64_t runtime_va, const StaticRef& ref,
                     uint64_t frame, const std::string& provenance_key = "");

    // Mark an address as still alive this frame.
    void touch(uint64_t runtime_va, uint64_t frame);

    // Age out entries not seen for `max_age_frames`; returns how many became stale.
    size_t age_out(uint64_t frame, uint64_t max_age_frames);

    // Drop stale entries.  Returns how many were removed.
    size_t prune_stale();

    const LiveMemoryEntry* find_by_va(uint64_t runtime_va) const;

    // Every live copy of the same static site — this is the "one static field,
    // thousands of runtime instances" case, made explicit instead of collapsed.
    std::vector<LiveMemoryEntry> all_instances_of(uint64_t var_id) const;
    size_t instance_count_of(uint64_t var_id) const;

    std::vector<LiveMemoryEntry> snapshot() const;
    size_t size() const;

    // Per-session random tags live here.  New session -> new tags (expected);
    // base identities never change.
    void   reset_session(uint64_t session_seed);
    uint64_t session_seed() const;

    // One-line health summary for the UI.
    std::string summary() const;

private:
    InstanceTag tag_for_locked(uint64_t runtime_va, const std::string& provenance_key);

    mutable std::mutex m_mu;
    std::unordered_map<uint64_t, LiveMemoryEntry> m_by_va;
    std::unordered_map<uint64_t, uint32_t>        m_session_tag_by_va;
    uint64_t m_session_seed = 0;
    uint32_t m_next_tag     = 0;
    uint64_t m_binds        = 0;
    uint64_t m_rebinds      = 0;
};

// ---------------------------------------------------------------------------
// IdentityStore — persistence so research survives a crash or a restart.
//
// Deliberately a plain text format (one record per line, tab separated) so it
// stays readable, diffable and recoverable with grep when something goes wrong.
// Records are keyed by var_id, i.e. by *identity*, never by address.
// ---------------------------------------------------------------------------
struct IdentityRecord {
    uint64_t    var_id = 0;
    std::string base_name;
    std::string canonical;
    std::string user_name;          // researcher's rename ("" = none)
    Provenance  provenance = Provenance::Unknown;
    uint64_t    first_seen_boot = 0;
};

class IdentityStore {
public:
    bool load(const std::string& path);
    bool save(const std::string& path) const;

    // Remember a resolved identity (idempotent; user_name is preserved).
    void remember(const Identity& id, uint64_t boot_index);

    // Researcher renames an *identity*, so the rename follows it across boots,
    // offsets and gameplay states.
    bool rename(uint64_t var_id, const std::string& user_name);

    // Bump provenance forward (never backwards): an observation can promote an
    // Unknown to Observed, but can never demote a Recovered offset.
    bool promote(uint64_t var_id, Provenance p);

    std::optional<IdentityRecord> find(uint64_t var_id) const;
    std::vector<IdentityRecord>   all() const;
    size_t size() const;

    // Effective display base name: the researcher's name when present.
    std::string effective_base_name(uint64_t var_id) const;

private:
    mutable std::mutex m_mu;
    std::unordered_map<uint64_t, IdentityRecord> m_records;
};

} // namespace swordfare::research
