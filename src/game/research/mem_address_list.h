// =============================================================================
// mem_address_list.h — the Cheat Engine "address list", identity-routed.
//
// Differences from a plain CE/GG address list, and the reason this can be more
// useful than either:
//
//   * A row is identified by its DETERMINISTIC identity (var_id + base name from
//     mem_identity.h), not by its current runtime address.  The address is a
//     cached, re-resolvable property — `rebind()` updates it when the guest
//     moves the object, and nothing else about the row changes.
//   * Renaming a row is a *label* on top of the identity: the original
//     var_id/base_name are preserved, so a researcher's name for an
//     undiscovered offset does not destroy what the row actually is.
//   * Writes are transactional: bounds-checked against the guest buffer, and
//     when the row knows the extent of the field it points into, a write whose
//     width would spill into the neighbouring field is refused instead of
//     corrupting the struct next door.
//   * Freeze is applied from the emulator's own update tick (apply_freeze()),
//     not a wall-clock thread, so there is no read/write race against the guest.
//
// Categorisation (§8 of the brief) is expressed as a filter so the UI can ask
// for "unresolved fields inside SceneObject instances" rather than squinting at
// a flat list: identity tier, provenance, kind, owning group and value type.
// =============================================================================
#pragma once

#include "game/research/mem_identity.h"
#include "game/research/mem_scanner.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace swordfare::research {

struct AddressEntry {
    uint64_t    id         = 0;
    uint64_t    runtime_va = 0;
    ValueType   type       = ValueType::Dword;
    // How this row's value is READ and WRITTEN.  Separate from `type` on
    // purpose: "this is a Dword" and "I want to see it in hex" are different
    // questions, and collapsing them is what made the old edit box seed itself
    // with a rendering ("32 (0x20)") that then failed to parse back.
    ValueDomain domain     = ValueDomain::Decimal;

    // ── Deterministic identity (see mem_identity.h) ────────────────────────
    Identity    identity;                 // var_id + deterministic base name
    InstanceTag tag;                      // which live copy
    std::string display_name;             // identity base + tag
    std::string user_label;               // researcher's rename ("" = none)
    MemKind     kind       = MemKind::Unknown;
    Provenance  provenance = Provenance::Unknown;

    // ── Ownership / safety ────────────────────────────────────────────────
    std::string group;                    // owning struct / scene object
    uint64_t    field_bound_end = 0;      // exclusive end of the owning field, 0 = unknown
    std::string user_notes;

    // ── Freeze state ──────────────────────────────────────────────────────
    bool                 frozen = false;
    std::vector<uint8_t> frozen_bytes;    // exact bytes restored every tick
    uint64_t             freeze_writes = 0;
    uint64_t             freeze_conflicts = 0;  // guest overwrote a frozen value

    // Rendering helpers
    std::string label() const { return user_label.empty() ? display_name : user_label; }
    bool        is_known() const { return provenance_is_proven(provenance); }
};

// Category filter (§8): identity tier, provenance, kind, owning group, type.
// A researcher can ask for "unresolved fields inside SceneObject instances"
// instead of squinting at a flat list.
struct AddressFilter {
    bool        unresolved_only = false;   // tier B/C: not recovered/confirmed
    bool        frozen_only     = false;
    bool        active_only     = true;
    Provenance  min_provenance  = Provenance::Unknown;  // Unknown = any
    MemKind     kind            = MemKind::Unknown;     // Unknown = any
    ValueType   type            = ValueType::All;       // All = any
    std::string group;                                  // empty = any
    bool        has_group       = false;
};

class AddressList {
public:
    // Add a row.  Identity is derived from the StaticRef (deterministic); the
    // tag says which live copy this row refers to.
    uint64_t add(const StaticRef& ref,
                 uint64_t runtime_va,
                 ValueType type,
                 const std::string& group = "",
                 uint64_t field_bound_end = 0,
                 const InstanceTag& tag = {});

    bool remove(uint64_t id);
    void clear();

    // ── Identity / movement ────────────────────────────────────────────────
    // The object moved (GC, scene reload, reallocation).  Only the runtime
    // address changes; var_id / base name / label / notes are untouched.
    bool rebind(uint64_t id, uint64_t new_runtime_va);

    // Researcher rename.  Identity is preserved (see header note).
    bool set_label(uint64_t id, const std::string& label);

    bool set_notes(uint64_t id, const std::string& notes);
    bool set_provenance(uint64_t id, Provenance p);   // forward-only

    // ── Values ─────────────────────────────────────────────────────────────
    // Rendered for reading ("32 (0x20)", quoted strings, spaced byte patterns).
    std::string read_value(uint64_t id, const uint8_t* mem, uint64_t mem_size) const;

    // The same value as *editable* text: no decoration, parses back to itself
    // through set_value().  An edit box must be seeded with this, never with
    // read_value() — that is a rendering, not an input.
    std::string read_editable(uint64_t id, const uint8_t* mem, uint64_t mem_size,
                              ValueDomain domain = ValueDomain::Decimal) const;

    // Per-row representation.  True when the row exists.
    bool set_domain(uint64_t id, ValueDomain domain);

    // Would this text write cleanly?  Runs the identical checks set_value()
    // does, without touching memory, so the UI can disable the commit button and
    // say why while the operator is still typing.
    bool validate_value(uint64_t id, const std::string& text, ValueDomain domain,
                        std::string* err) const;

    // Transactional write.  Refused (with `err` set) when out of bounds, when the
    // text does not fit the row's type, or when the width would spill past
    // field_bound_end.
    bool set_value(uint64_t id, const std::string& text,
                   uint8_t* mem, uint64_t mem_size, std::string* err,
                   ValueDomain domain = ValueDomain::Auto);

    // Capture the current value as the frozen payload; with `text` non-empty the
    // frozen payload is that value instead.
    bool set_frozen(uint64_t id, bool on,
                    const uint8_t* mem, uint64_t mem_size,
                    const std::string& text = "", std::string* err = nullptr);

    // Called from the emulator update tick.  Returns how many rows were written.
    size_t apply_freeze(uint8_t* mem, uint64_t mem_size);

    // ── Query / categorisation ─────────────────────────────────────────────
    std::vector<AddressEntry> snapshot(const AddressFilter& f = AddressFilter{}) const;
    std::vector<std::string>  groups() const;
    const AddressEntry*       find(uint64_t id) const;
    size_t                    size() const;

    std::string summary() const;

private:
    AddressEntry* find_locked(uint64_t id);
    const AddressEntry* find_locked(uint64_t id) const;

    // Shared by validate_value() and set_value() so the button's enablement and
    // the write's outcome can never disagree.  On success `*width` receives the
    // byte count the write will occupy.
    bool check_write_locked(const AddressEntry& e, const std::string& text,
                            ValueDomain domain, size_t* width,
                            std::string* err) const;

    mutable std::mutex m_mu;
    std::vector<AddressEntry> m_entries;
    uint64_t m_next_id = 1;
    uint64_t m_writes = 0;
    uint64_t m_refused = 0;
};

} // namespace swordfare::research
