// =============================================================================
// memory_research_tab.h — ImGui Memory Research dockable tab
//
// Integrates into SwordfareGUI::draw_mod_overlay() as an additional tab.
// Renders:
//   • Catalog Browser  — struct list, field table, vtable viewer, proto fields
//   • Live Inspector   — decode live root structs (GSC, hero, health/mana comps)
//   • Watchpoints      — add/remove/view active watchpoints + hit log
//   • DB Status        — embedded DB size, table stats, user research DB path
// =============================================================================
#pragma once

#include "game/research/recovery_catalog.h"
#include "game/research/mem_identity.h"
#include "game/research/elf_symbols.h"
#include "game/research/elf_sections.h"
#include "game/research/mem_watchpoint.h"
#include "game/research/struct_decoder.h"
#include "game/research/vtable_classifier.h"
#include "game/research/root_walker.h"
#include "game/research/recovery_session_manager.h"
#include "game/research/mem_scanner.h"
#include "game/research/mem_address_list.h"
#include "game/research/unclaimed_explorer.h"
#include "game/research/live_object_map.h"
#include "game/research/mem_access_trace.h"
#include "game/research/research_workspace.h"
#if __has_include("game/research/hex_inspector.h")
#  include "game/research/hex_inspector.h"
#  define SWORDFARE_HAVE_HEX_INSPECTOR 1
#else
#  define SWORDFARE_HAVE_HEX_INSPECTOR 0
#endif
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <memory>
#include <optional>

namespace swordfare::research {

// ---------------------------------------------------------------------------
// Live root pointer state — filled from the extern globals in emulator.cpp
// ---------------------------------------------------------------------------
struct LiveRoots {
    uint32_t gsc_va      = 0;  // g_game_scene_controller (ARM32 VA)
    uint32_t hero_va     = 0;  // g_hero_obj
    uint32_t health_va   = 0;  // g_hero_health_comp
    uint32_t mana_va     = 0;  // g_hero_mana_comp
    uint32_t char_ctrl_va= 0;  // g_hero_char_ctrl_comp
};

// ---------------------------------------------------------------------------
// MemoryResearchTab
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// SiteIdentity — what a raw live address IS, resolved through the three
// identity tiers (see mem_identity.h).
//
// The UI shows this instead of a bare address, because the address is the one
// part of a memory location that is guaranteed *not* to survive a reboot.  The
// tier decides which of three honest answers we can give:
//
//   known         a recovered field / live object  -> its recovered name
//   identifiable  a module RVA with no name yet    -> the deterministic name
//                 (g_VAR_#### / VAR_####), which is the SAME on every boot
//   runtime only  stack/heap/JIT: no static anchor -> a session-scoped name,
//                 labelled as such rather than dressed up as permanent
// ---------------------------------------------------------------------------
struct SiteIdentity {
    bool        mapped  = false;  // tier A/B: normalised to module + RVA
    bool        known   = false;  // tier A: a recovered/confirmed field covers it
    std::string name;             // the display name (never a bare address)
    std::string site;             // "libswordigo.so+0x31058" | "unmapped (stack/heap/JIT)"
    std::string site_module;      // module name when mapped
    uint64_t    site_rva = 0;     // module-relative offset, 0 when unmapped
    std::string owner;            // owning struct / object, when known
    std::string tier;             // "known" | "identifiable" | "runtime only"
    std::string note;             // one line: why this name, shown as a tooltip
    Provenance  prov  = Provenance::Unknown;
    uint64_t    var_id = 0;

    // ── Symbol evidence (display only — never part of the identity) ────────
    // libswordigo.so ships ~17.7k dynamic symbols, so "where is this address?"
    // is answerable rather than something to guess:
    //   symbol  : "Caver::GameSceneController::Update+0x18"  (last symbol below)
    //   between : "Caver::A+0x4  ..  Caver::B"              (the gap it sits in)
    //   sym_kind: "inside" | "between" | "after" | "before" | ""
    // These annotate a site; they must never enter the identity key, or the
    // researcher's short stable handle would become long and build-dependent.
    std::string symbol;
    std::string between;
    std::string sym_kind;
    std::string sym_qualified;    // same symbol, fully namespace-qualified
    bool        sym_inside = false;

    // The section, when the symbol table has nothing to say (it has a hole below
    // its lowest symbol, and that hole is where a module-scoped scan floods).
    std::string section;          // ".dynsym+0x30D60" | "read-only data+0x1038"
    std::string section_kind;     // "symbol table" | "code" | ...
    // True when the address is inside the image's own bookkeeping (symbol table,
    // string table, relocations, hash).  Never game state.
    bool        in_metadata = false;
};

class MemoryResearchTab {
public:
    // Fonts for this console's own surfaces.  `void*` is an ImFont*, so this
    // header stays free of an ImGui dependency.  Both are optional: when null
    // the console falls back to whatever ImGui currently has, which on this
    // build is the game's display lettering face.
    //
    // Why it matters here specifically: m_font_mono was declared but never
    // assigned anywhere in the codebase, so every `if (m_font_mono) PushFont`
    // was dead code and addresses, hex offsets and values all rendered in a
    // comic font — where fixed-width alignment is not decoration, it is what
    // makes a column of hex readable at all.
    struct ConsoleFonts {
        void* body = nullptr;   // proportional UI face (chrome, labels, prose)
        void* mono = nullptr;   // fixed-width (addresses, hex, values, dumps)
    };
    void set_fonts(const ConsoleFonts& f) { m_fonts = f; }

    // Call once after RecoveryCatalog::init() and after guest memory is mapped.
    // guest_memory  : pointer to g_guest_memory (the 4 GiB host mmap'd region)
    // guest_mem_size: GUEST_MEM_SIZE (0xE0000000)
    void init(const uint8_t* guest_memory, uint64_t guest_mem_size);

    // Call once per ImGui frame (between Begin/End).  This function renders
    // the full tab bar contents — caller must already be inside an ImGui window.
    void draw();

    // Update the live root pointers (call from main loop after each game frame).
    // These are pointer VALUES the engine hands us (SRE's gsc_get()), so the
    // object map treats them as direct-value roots: the pointer is re-read every
    // frame and only the runtime half of the identity pipeline moves.
    void update_roots(const LiveRoots& roots) {
        m_roots = roots;
        sync_root_pointers();
    }

    bool is_ready() const { return m_ready; }

    // Called once per emulated frame, from the emulator thread — NOT from the
    // render loop.  This is where freezes are applied and the object map is
    // re-resolved, so both happen on the guest's own tick with no wall-clock
    // race against the game.
    void tick(uint64_t frame, uint8_t* guest_memory, uint64_t guest_mem_size);

    // Open the persistent workspace (.swordfare).  Called from init() with the
    // configured save directory; a failure is non-fatal and is surfaced in the
    // UI rather than silently ignored.
    void open_workspace(const std::string& root);

    // Tell the engine where a loaded image sits, so a runtime address can be
    // normalised back to module+RVA.  Until this is called every hit is
    // reported as "unmapped" — which is true, not a placeholder.
    void register_module(const ModuleExtent& e);

    // Hand over the loaded image's dynamic symbol table (the pointers the ELF
    // loader already holds: so_module_arm64::dynsym / dynstr / num_dynsym), so a
    // static site can be described by the symbol it falls in or between.
    bool register_module_symbols(const std::string& module,
                                 const void* symtab, size_t count,
                                 const char* strtab, size_t strtab_size,
                                 bool is_64);

    // Hand over the loaded image's section headers (+ the loader's copy of the
    // section-name table), so a site can be described by its section when the
    // symbol table has nothing for it, and so the image's own metadata blocks can
    // be recognised and skipped.
    bool register_module_sections(const std::string& module,
                                  const void* shdrs, size_t count,
                                  const char* names, size_t names_size,
                                  bool is_64);

    // How much symbol evidence is loaded, for the UI status line.
    std::string symbol_status() const { return m_symbols.summary(); }
    size_t      symbol_count(const std::string& module) const {
        return m_symbols.count(module);
    }
    std::string section_status() const { return m_sections.summary(); }

    // Number of roots the engine is tracking (for the header status line).
    size_t tracked_roots() const { return m_object_map.size(); }

    // ── In-game mini bar ───────────────────────────────────────────────────
    // A compact always-available strip over the game: the rows a researcher
    // actually watches, with freeze / nudge / set right there, so the game can
    // keep running while a value is being poked.  Returns true when the user
    // asked for the full console back (the "full screen" button).
    bool draw_hud();

    bool hud_enabled() const { return m_hud_enabled; }
    void set_hud_enabled(bool on) { m_hud_enabled = on; }

    // Which console section "full screen" should land on.  The HUD knows what
    // the researcher was doing, so it can open the right panel instead of
    // dumping them back at whichever section happened to be active.
    void request_section(int section) { m_active_section = section; }
    int  active_section() const { return m_active_section; }

private:
    // Tab sub-panels
    void draw_catalog_browser();
    void draw_live_inspector();
    void draw_watchpoints();
    void draw_db_status();
    void draw_object_tree();
    void draw_scanner();
    void draw_address_list();
    void draw_evidence();
    void draw_leads();

    // Rebuild the "already identified" set the explorer must skip, then run one
    // bounded exploration pass.  Called from tick(), on the emulated frame.
    void explore_tick(uint8_t* guest_memory, uint64_t guest_mem_size, uint64_t frame);

    // Add `delta` to an address-list row through the validated write path (used
    // by the in-game mini bar's -1/+1 buttons).  `ro` is the same buffer for
    // reading; kept separate so the caller's constness is honest.
    void nudge_row(uint64_t id, int delta,
                   uint8_t* mem, const uint8_t* ro, uint64_t mem_size);

    // Drop scan hits that sit in the image's own bookkeeping.  Returns how many.
    size_t drop_metadata_hits();

    // Helpers
    void sync_root_pointers();

    // Populate the address list from the recovered fields of the live roots, so
    // the panel shows real, correctly-identified values on boot instead of an
    // empty table.  Idempotent per (root, struct) pair.
    void seed_address_list_from_catalog();

    // Render a consistent "this list is empty, and here is why / what to do"
    // block.  An empty table with no explanation is indistinguishable from a
    // broken one.
    static void empty_state(const char* headline, const char* detail);

    // The scan range implied by the registered modules, if any.
    bool module_scan_range(uint64_t* begin, uint64_t* end) const;

    // Why is the address list empty?  Reports the actual blocking condition
    // (catalog not loaded / no root observed / no layout for a root) instead of
    // a generic hint, so an empty panel is diagnosable rather than mysterious.
    std::string address_list_diagnosis() const;

    // Resolve one live address to the best identity we can honestly claim (see
    // SiteIdentity).  `known_rows` may carry a pre-fetched address-list snapshot
    // so a table of thousands of hits does not re-snapshot per row.
    SiteIdentity resolve_site(uint64_t va, ValueType type,
                              const std::vector<AddressEntry>* known_rows = nullptr) const;

    // The build id of a registered module ("" when it has none).  Part of the
    // identity hash, so the same RVA in two builds is deliberately NOT the same
    // identity.
    std::string build_id_of(const std::string& module) const;
    void draw_field_table(const std::vector<DecodedField>& decoded,
                          const std::vector<CatalogRelationship>& rels);
    void draw_struct_selector();

    ConsoleFonts m_fonts;
    // Push the mono face for a numeric/hex region; returns whether it pushed, so
    // the pairing with pop_mono() is impossible to get wrong.
    bool push_mono();
    void pop_mono(bool pushed);

    // ── The value editor ──────────────────────────────────────────────────
    // Shared by the address list and the in-game bar, because the bug it exists
    // to prevent (seeding the box with a *rendering* like "32 (0x20)" and then
    // failing to parse it back, so every edit silently reverted) was present
    // independently in both.
    //
    // Guarantees:
    //   * the box is seeded from read_editable() — bare text that parses back;
    //   * the decimal/hex counterpart is computed live from what was typed;
    //   * the dec/hex toggle re-renders the SAME value in the new base, so a
    //     number never changes meaning just because the base did;
    //   * set is disabled while the text is unparseable, and the reason is shown,
    //     using the same validator the write itself uses.
    // Writes `outcome`/`*error`; `counterpart` receives the live counterpart.
    void draw_value_editor(uint64_t row_id, const AddressEntry& e,
                           const uint8_t* readable, uint8_t* writable,
                           uint64_t mem_size,
                           char* buf, size_t buf_size,
                           ValueDomain* domain, ValueDomain* row_domain,
                           bool* dirty, std::string* outcome, std::string* error,
                           std::string* counterpart,
                           float field_width, bool compact);

    // Pull the struct list out of RecoveryCatalog.  Safe to call repeatedly:
    // it no-ops once the list is loaded, and an empty result is never cached
    // (the catalog may simply not be ready yet).
    void refresh_catalog();

    bool                        m_ready           = false;
    int                         m_active_section  = 0;  // nav-rail selection
    std::unique_ptr<StructDecoder> m_decoder;

    // ── Catalog Browser state ──
    std::vector<CatalogStruct>  m_all_structs;
    bool                        m_catalog_loaded  = false;
    int                         m_catalog_retry_in = 0;  // frames until retry
    int                         m_selected_struct  = -1;
    std::vector<CatalogField>   m_selected_fields;
    std::vector<CatalogRelationship> m_selected_rels;
    std::optional<CatalogVtable>    m_selected_vtable;
    std::vector<CatalogProtoField>  m_selected_proto;
    char                        m_struct_filter[64] = {};

    // ── Live Inspector state ──
    LiveRoots                   m_roots;
    // Per-root: which struct was last decoded + the decoded fields
    struct LiveDecoded {
        std::string struct_name;
        uint64_t    last_va    = 0;
        std::vector<DecodedField> fields;
    };
    std::array<LiveDecoded, 5>  m_live_decoded;   // GSC, hero, health, mana, charctrl

    // ── Watchpoint state ──
    static constexpr int kHitBufSize = 256;
    std::array<WatchHit, kHitBufSize> m_hit_buf;
    std::vector<WatchHit>       m_hit_log;   // rolling history, max 2048
    uint32_t                    m_wp_add_va_u32 = 0;
    uint32_t                    m_wp_add_size   = 4;
    char                        m_wp_add_label[48] = {};
    bool                        m_wp_add_write  = true;
    bool                        m_wp_add_read   = false;

    // ── DB Status state ──
    std::vector<RecoveryCatalog::TableStat> m_table_stats;
    bool m_stats_loaded = false;

    // ── Object Tree state ──
    int  m_walk_throttle_frames = 0;   // count frames between walks
    bool m_auto_walk = false;           // toggle in UI
    static constexpr int kWalkInterval = 30; // walk every 30 frames when auto enabled

    struct ObjectTreeState {
        WalkResult last_walk;
        bool       dirty = true;
    };
    ObjectTreeState m_object_tree;

    // ── Hex Inspector state ──
    // Driven by clicking an object in the Object Tree.
    uint64_t                         m_hex_target_va     = 0;
    std::string                      m_hex_target_type;  // e.g. "SceneObject"
    std::vector<CatalogField>        m_hex_target_fields; // fields for overlay
    std::optional<CatalogStruct>     m_hex_target_struct;
#if SWORDFARE_HAVE_HEX_INSPECTOR
    HexInspector                     m_hex_inspector;
#endif
    void draw_hex_view();

    // ── Scanner state ─────────────────────────────────────────────────────
    MemScanner              m_scanner;
    char                    m_scan_type_filter[16] = "4 Bytes";
    char                    m_scan_value[64]        = {};
    char                    m_scan_value2[64]       = {};
    int                     m_scan_value_type      = 2;   // index into kValueTypes
    int                     m_scan_kind            = 0;   // index into kScanKinds
    int                     m_scan_alignment       = 0;   // 0 = natural
    bool                    m_scan_hex             = false;
    bool                    m_scan_restrict_module = true;
    // Skip the image's own bookkeeping (dynsym/dynstr/rela/hash).  On by default:
    // a module-scoped scan otherwise floods with hits on the symbol table itself.
    bool                    m_scan_skip_metadata  = true;
    size_t                  m_scan_skipped_meta   = 0;  // hits filtered by that rule
    uint64_t                m_scan_range_begin     = 0;
    uint64_t                m_scan_range_end       = 0;
    uint64_t                m_scan_max_results     = 4096;
    std::string             m_scan_status;
    double                  m_scan_elapsed_ms      = 0.0;
    uint32_t                m_scan_generation      = 0;   // bumps when the candidate set changes
    uint64_t                m_scan_current_shot    = 0;   // saved shot the live set came from
    char                    m_scan_pass_label[48]  = {};
    std::vector<uint32_t>   m_scan_selected;              // candidate indices picked for the list
    char                    m_addr_add_va[32]        = {};
    char                    m_addr_add_label[64]     = {};
    int                     m_addr_filter_tier       = 0;
    int                     m_addr_filter_group      = 0;
    bool                    m_addr_filter_frozen     = false;
    std::vector<std::string> m_addr_groups;
    char                    m_addr_edit[64]         = {};
    uint64_t                m_addr_editing          = 0;
    std::string             m_addr_status;
    // How an in-progress edit is being READ.  Separate from the row's own domain
    // so toggling dec/hex while typing re-reads the text instead of silently
    // reinterpreting it, and so an abandoned edit leaves the row alone.
    ValueDomain             m_addr_edit_domain     = ValueDomain::Decimal;
    ValueDomain             m_addr_edit_row_domain = ValueDomain::Decimal;
    bool                    m_addr_edit_dirty      = false;   // has the field been touched?
    std::string             m_addr_edit_error;
    std::string             m_addr_edit_counterpart;           // live "= 0x20"
    // Set by draw_value_editor() when a session ended; each caller clears its
    // own "which row" handle from it, so the shared helper never has to know
    // which surface it was called from.
    bool                    m_value_edit_over       = false;
    // "why do we think this is X?" — a real panel, not a status line.
    uint64_t                m_addr_why_id          = 0;
    std::string             m_addr_why_name;
    std::string             m_addr_why_text;
    bool                    m_addr_why_open         = false;
    std::vector<int>        m_seeded_structs;      // catalog struct ids already seeded
    uint64_t                m_seeded_root_va[5]    = {0, 0, 0, 0, 0};
    std::string             m_seeded_groups[5];    // group name used for each root's rows
    bool                    m_object_fields_attached = false;

    // ── Unclaimed explorer (leads) ─────────────────────────────────────────
    UnclaimedExplorer       m_explorer;
    bool                    m_explore_enabled      = false;
    bool                    m_explore_started      = false;   // config applied yet?
    uint64_t                m_explore_last_frame   = 0;
    uint32_t                m_explore_every_frames = 8;
    uint64_t                m_explore_scene_epoch  = 0;
    uint64_t                m_explore_last_moves   = 0;
    bool                    m_explore_rebuild      = false;
    std::string             m_explore_status;
    std::vector<Lead>       m_explore_leads;
    bool                    m_explore_dirty        = true;
    int                     m_explore_type         = 2;    // index into kValueTypes
    uint64_t                m_explore_region_begin = 0;
    uint64_t                m_explore_region_end   = 0;
    int                     m_explore_leads_shown  = 0;

    // ── Symbol / section evidence ──────────────────────────────────────────
    SymbolTable             m_symbols;
    SectionMap              m_sections;
    // 0 = short name (Caver::Matrix4::Translate), 1 = namespace-resolved.
    int                     m_symbol_style      = 0;
    std::string             m_symbol_wantname;   // last looked-up symbol, for the UI

    // Symbol evidence is a pure function of (module, RVA), so it is memoised: a
    // 2000-row table would otherwise do two binary searches and build four
    // strings per row, every frame.  Deliberately NOT a memo of the whole
    // SiteIdentity — the tier-A half depends on live objects and address-list
    // rows, which move, and a stale name is exactly the failure this whole layer
    // exists to prevent.
    struct SymCacheEntry {
        std::string module;
        uint64_t    rva    = 0;
        uint64_t    stamp  = 0;
        std::string symbol, between, kind, qualified;
        bool        inside = false;
        bool        valid  = false;
    };
    static constexpr size_t kSymCacheSlots = 256;
    mutable std::array<SymCacheEntry, kSymCacheSlots> m_sym_cache{};
    // Bumped whenever modules or symbols are (re)registered: invalidates memos.
    uint64_t                m_site_stamp = 1;

    // Fill `s`'s symbol fields from the module's dynamic symbol table.
    void symbol_evidence(const std::string& module, uint64_t rva, SiteIdentity* s) const;

    // Section cache entry: pure function of (module, RVA), like the symbol one.
    struct SecCacheEntry {
        std::string module, section, kind;
        uint64_t    rva    = 0;
        uint64_t    stamp  = 0;
        bool        meta   = false;
        bool        valid  = false;
    };
    static constexpr size_t kSecCacheSlots = 256;
    mutable std::array<SecCacheEntry, kSecCacheSlots> m_sec_cache{};
    // Fill `s`'s section fields (and `in_metadata`) from the image's sections.
    void section_evidence(const std::string& module, uint64_t rva, SiteIdentity* s) const;

    // ── In-game mini bar state ─────────────────────────────────────────────
    bool                    m_hud_enabled         = false;
    bool                    m_hud_placed          = false;   // default position applied yet?
    uint64_t                m_hud_editing         = 0;       // address-list row being edited
    char                    m_hud_edit[64]        = {};
    ValueDomain             m_hud_edit_domain     = ValueDomain::Decimal;
    bool                    m_hud_edit_dirty      = false;
    std::string             m_hud_edit_error;
    std::string             m_hud_edit_counterpart;
    std::string             m_hud_status;

    // ── Live Object Map / evidence / persistence ───────────────────────────
    LiveObjectMap           m_object_map;
    AccessTrace             m_access_trace;
    ResearchWorkspace       m_workspace;
    AddressList             m_addresses;
    // Object-map root ids for the five live roots, in kRootLabels order.
    uint64_t                m_root_object_id[5]     = {0, 0, 0, 0, 0};
    uint64_t                m_frame_counter        = 0;
    uint64_t                m_session_seed         = 0;
    std::string             m_workspace_status;
    std::string             m_recovery_banner;
    bool                    m_workspace_checked    = false;
};

} // namespace swordfare::research
