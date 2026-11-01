// =============================================================================
// research_workspace.h — the on-disk .swordfare/ layout.
//
//     .swordfare/
//         builds/
//             swordigo_1.4.13_arm64.json     # per-build static profile
//         research/
//             symbols.json       # identity -> names (rename-safe, keyed by var_id)
//             mappings.json      # identity -> static location (module/rva/offset)
//             observations.json  # identity -> what was noticed, and why
//             structs.json       # recovered layouts we are using
//         sessions/
//             2026-09-20_1422.session         # one live session
//
// The split is the point, and it mirrors the identity model exactly:
//
//   * `research/` is keyed by var_id (identity), so it survives a reboot, a
//     crash, a scene change and an ASLR change.  A researcher's rename of
//     VAR_750 -> currentEnergy is recorded *alongside* the original identity,
//     never in place of it.
//   * `sessions/` holds the transient half — runtime addresses and instance
//     tags.  It is written to continuously (checkpoint) so an unexpected close
//     leaves behind a readable record of what was being investigated.
//
// Crash-recovery behaviour, concretely: `checkpoint()` marks the session as
// running and rewrites it in place.  `end_session("closed")` is the only thing
// that marks it clean.  So `latest_session_summary()` finding a session that is
// still marked running means the last process died — and it can report what the
// researcher was looking at, which is exactly the VAR_750-not-VAR_193 promise.
// =============================================================================
#pragma once

#include "game/research/mem_address_list.h"
#include "game/research/mem_identity.h"
#include "game/research/live_object_map.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace swordfare::research {

// ---------------------------------------------------------------------------
// Static location of an identity — the mapping half of the research DB.
// ---------------------------------------------------------------------------
struct Mapping {
    uint64_t    var_id = 0;
    std::string module;
    uint64_t    rva        = 0;   // static location (module-relative!)
    uint64_t    offset     = 0;   // offset inside the container
    uint64_t    width      = 0;
    std::string type_name;
    MemKind     kind       = MemKind::Unknown;
    std::string container;        // struct / function / global name
    std::string note;
};

struct Observation {
    uint64_t    var_id     = 0;
    uint64_t    seen_at    = 0;        // unix seconds
    std::string state;                 // "OBSERVED" / "SUSPECTED" / ...
    double      confidence = 0.0;      // 0..1
    std::string what;                  // "value changes when chest opened"
    std::string evidence;              // raw detail (instruction, PC RVA, ...)
};

struct BuildProfileModule {
    std::string name;
    uint64_t    base_va   = 0;
    uint64_t    rva_begin = 0;
    uint64_t    rva_end   = 0;
    std::string sha;
};

struct BuildProfile {
    std::string build_id;             // "swordigo_1.4.13_arm64"
    std::string game_version;
    std::string abi;                  // "arm64" / "armv7"
    uint64_t    text_size = 0;
    std::string notes;
    std::vector<BuildProfileModule> modules;
};

// ---------------------------------------------------------------------------
class ResearchWorkspace {
public:
    // Create (or adopt) the tree under `root`.  Empty root -> ".swordfare".
    bool open(const std::string& root, std::string* err = nullptr);

    const std::string& root() const { return m_root; }
    bool is_open() const { return m_open; }
    std::string path_of(const std::string& relative) const;
    std::string build_profile_path(const std::string& build_id) const;

    // ── research/ (identity-keyed, survives everything) ───────────────────
    IdentityStore& symbols() { return m_symbols; }
    const IdentityStore& symbols() const { return m_symbols; }

    void set_mapping(const Mapping& m);
    std::optional<Mapping> mapping(uint64_t var_id) const;
    size_t mapping_count() const { return m_mappings.size(); }

    void add_observation(const Observation& o);
    std::vector<Observation> observations_for(uint64_t var_id) const;
    std::vector<Observation> all_observations() const { return m_observations; }

    bool save_research(std::string* err = nullptr) const;
    bool load_research(std::string* err = nullptr);

    // "Why do I think this is what I think it is?" — the provenance explanation
    // the UI shows when a row is clicked.  Never claims more than the data says.
    std::string explain(uint64_t var_id) const;

    // ── builds/ ───────────────────────────────────────────────────────────
    bool save_build_profile(const BuildProfile& p, std::string* err = nullptr) const;
    std::optional<BuildProfile> load_build_profile(const std::string& build_id,
                                                   std::string* err = nullptr) const;

    // ── sessions/ ─────────────────────────────────────────────────────────
    bool begin_session(uint64_t session_seed, std::string* err = nullptr);
    // Autosave.  Cheap enough to call on a timer; writes the whole session so
    // an abrupt kill still leaves the researcher's working state on disk.
    bool checkpoint(const std::vector<AddressEntry>& rows,
                    const std::vector<LiveObject>& objects,
                    std::string* err = nullptr);
    bool end_session(const std::string& reason, std::string* err = nullptr);

    bool   has_open_session() const { return m_session_open; }
    const  std::string& session_path() const { return m_session_path; }

    // Newest session file on disk (whether or not this process owns it).
    std::string latest_session_file() const;
    // True when the newest session was left marked "running" -> the previous
    // process did not shut down cleanly.
    bool crash_recovered() const;
    // What was in progress, for the "welcome back" panel.
    std::string latest_session_summary() const;

    std::string summary() const;

private:
    bool write_text_file(const std::string& path, const std::string& text,
                         std::string* err) const;
    bool read_text_file(const std::string& path, std::string* out) const;
    std::vector<std::string> list_dir(const std::string& dir) const;

    std::string m_root;
    std::string m_session_path;
    bool        m_open         = false;
    bool        m_session_open = false;
    uint64_t    m_session_seed = 0;
    uint64_t    m_checkpoints  = 0;

    IdentityStore            m_symbols;
    std::vector<Mapping>     m_mappings;
    std::vector<Observation> m_observations;
};

// ---------------------------------------------------------------------------
// Minimal JSON helpers — real JSON in and out so the files are usable from any
// tool, with no third-party dependency.  Only the subset this module emits is
// supported; anything else is reported as a parse failure rather than guessed.
// ---------------------------------------------------------------------------
namespace json {
std::string  escape(const std::string& s);
std::string  quote(const std::string& s);
std::string  number(double v);
// Exact 64-bit integer literal.  NEVER write an identity through `number()`:
// a var_id is a 64-bit hash, and anything above 2^53 does not survive a double
// round-trip.  An identity that changes when it is saved is not an identity.
std::string  u64(uint64_t v);

// Flat object of string/number/bool fields (one level), enough for a profile.
struct Obj {
    std::string text;                                  // raw object body
    bool        get_string(const std::string& key, std::string* out) const;
    bool        get_number(const std::string& key, uint64_t* out) const;
    // Array of flat objects under `key`.
    std::vector<Obj> get_array(const std::string& key) const;
};

// Parse a top-level object.  Returns false on malformed input.
bool parse_object(const std::string& src, Obj* out);
} // namespace json

} // namespace swordfare::research
