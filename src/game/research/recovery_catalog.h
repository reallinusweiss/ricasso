// =============================================================================
// recovery_catalog.h — Swordfare Live Memory Research System
//
// Manages two SQLite3 databases:
//   1. embedded_recovery.db  — compiled into the binary; read-only canonical
//      struct/field/vtable/proto/relationship data.  Never mutated at runtime.
//   2. user_research.db      — writable; lives in the user's data directory.
//      Stores user aliases, runtime observations, and session annotations.
//
// Thread-safety: all public methods are safe to call from any thread.
//   DB access is serialised by m_mu; all read queries return copies.
//
// Usage:
//   auto& cat = RecoveryCatalog::instance();
//   cat.init(user_data_dir);             // call once from main thread
//   auto* sf = cat.find_struct("GameSceneController");  // safe from any thread
// =============================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <mutex>
#include <memory>
#include <functional>
#include <unordered_map>

// Forward-declare SQLite3 handle so consumers don't need sqlite3.h
struct sqlite3;
struct sqlite3_stmt;

// Bytes of the embedded DB — defined in embedded_recovery_db.cpp (generated)
namespace swordfare::research {
    const unsigned char* embedded_recovery_db_data();
    size_t               embedded_recovery_db_size();
} // namespace swordfare::research

namespace swordfare::research {

// ---------------------------------------------------------------------------
// Plain-data structs returned by catalog queries (all heap-safe copies)
// ---------------------------------------------------------------------------

struct CatalogBuild {
    int         id          = 0;
    std::string version;       // e.g. "1.4.13"
    std::string arch;          // "arm32" / "arm64"
    uint64_t    load_base    = 0;
    std::string notes;
};

struct CatalogStruct {
    // Dense session-stable id: the index of this struct in ALL_STRUCTS order
    // (ORDER BY name).  The embedded DB itself keys structs by a TEXT id
    // ("GameSceneController"), which is preserved verbatim in `key`.
    int         id           = 0;
    int         build_id     = 0;
    std::string key;           // canonical TEXT id from the embedded DB
    std::string build;         // TEXT build id, e.g. "sre13-1.4.13-arm64"
    std::string name;          // e.g. "GameSceneController"
    uint32_t    size_bytes   = 0;   // size_arm64
    std::string source_file;   // recovery_sources citation for this struct
    std::string notes;         // structs.summary
};

struct CatalogField {
    int         id              = 0;
    int         struct_id       = 0;
    std::string field_name;
    std::string field_type;
    uint32_t    offset_arm64    = 0;
    uint32_t    offset_arm32    = 0;
    uint32_t    size_bytes      = 0;
    std::string confidence;     // "KNOWN" / "INFERRED" / "GUESSED"
    std::string notes;
};

struct CatalogVtableSlot {
    int         id           = 0;
    int         vtable_id    = 0;
    int         slot_index   = 0;
    uint64_t    vaddr_arm64  = 0;
    uint64_t    vaddr_arm32  = 0;
    std::string symbol_name;
    std::string sig;
    std::string notes;
};

struct CatalogVtable {
    int         id           = 0;
    int         struct_id    = 0;
    uint64_t    vptr_arm64   = 0;
    uint64_t    vptr_arm32   = 0;
    int         slot_count   = 0;
    std::string notes;
    std::vector<CatalogVtableSlot> slots;
};

struct CatalogProtoField {
    int         id           = 0;
    int         struct_id    = 0;
    int         tag          = 0;
    std::string wire_type;   // "varint" / "fixed32" / "fixed64" / "length_delimited"
    std::string cpp_field;
    std::string proto_name;
    std::string notes;
};

struct CatalogRelationship {
    int         id           = 0;
    int         from_id      = 0;
    int         to_id        = 0;
    std::string kind;        // "EMBEDDED" / "RAW_PTR" / "SHARED_PTR" / "INTRUSIVE_PTR" / "VECTOR" / "SET"
    uint32_t    offset_arm64 = 0;
    uint32_t    offset_arm32 = 0;
    std::string notes;
    // Populated on query if requested
    std::string from_name;
    std::string to_name;
};

// ---------------------------------------------------------------------------
// RecoveryCatalog — singleton
// ---------------------------------------------------------------------------
class RecoveryCatalog {
public:
    static RecoveryCatalog& instance();

    // ── Lifecycle ─────────────────────────────────────────────────────────
    // init() opens both databases.  user_data_dir must be an existing writable
    // directory (pass get_data_path() from data_path.h).  May be called only
    // once; subsequent calls are no-ops.
    void init(const std::string& user_data_dir);

    // Shut down both DB handles.  Safe to call from any thread.
    void shutdown();

    bool is_ready() const;

    // ── Struct queries ────────────────────────────────────────────────────
    // Returns nullptr (as empty optional) if not found.
    std::optional<CatalogStruct> find_struct(const std::string& name) const;
    std::optional<CatalogStruct> find_struct_by_id(int id) const;
    std::vector<CatalogStruct>   all_structs() const;

    // ── Field queries ─────────────────────────────────────────────────────
    std::vector<CatalogField>    fields_for_struct(int struct_id) const;
    // Resolve field name → offset for a given struct (arm64 path).
    std::optional<uint32_t>      field_offset64(const std::string& struct_name,
                                                const std::string& field_name) const;
    std::optional<uint32_t>      field_offset32(const std::string& struct_name,
                                                const std::string& field_name) const;

    // ── Vtable queries ────────────────────────────────────────────────────
    std::optional<CatalogVtable> vtable_for_struct(int struct_id,
                                                   bool include_slots = true) const;
    // Look up a vtable by its vptr value (arm64).
    std::optional<CatalogVtable> vtable_by_vptr64(uint64_t vptr) const;

    // ── Proto queries ─────────────────────────────────────────────────────
    std::vector<CatalogProtoField> proto_fields_for_struct(int struct_id) const;

    // ── Relationship queries ──────────────────────────────────────────────
    std::vector<CatalogRelationship> relationships_from(int struct_id) const;
    std::vector<CatalogRelationship> relationships_to(int struct_id) const;

    // ── User research DB writes ───────────────────────────────────────────
    // All writes go to user_research.db — never to the embedded canonical DB.

    // Record a runtime observation (VA → struct name mapping confirmed live).
    void record_runtime_observation(const std::string& struct_name,
                                    uint64_t           guest_va,
                                    const std::string& note = "");

    // Store a user-defined alias for a field offset.
    void set_field_alias(int struct_id, uint32_t offset_arm64,
                         const std::string& alias_name,
                         const std::string& note = "");

    // ── Diagnostics ───────────────────────────────────────────────────────
    // Returns a short multi-line status string for the debug overlay.
    std::string status_summary() const;

    // Enumerate all tables and row counts (for diagnostics).
    struct TableStat { std::string name; int row_count = 0; };
    std::vector<TableStat> table_stats() const;

private:
    RecoveryCatalog()  = default;
    ~RecoveryCatalog() = default;
    RecoveryCatalog(const RecoveryCatalog&) = delete;
    RecoveryCatalog& operator=(const RecoveryCatalog&) = delete;

    // Open embedded DB from compiled-in bytes using sqlite3_deserialize.
    bool open_embedded_db();
    // Open or create the user research DB at the given path.
    bool open_user_db(const std::string& path);
    // Create user DB schema (idempotent, uses IF NOT EXISTS).
    bool ensure_user_schema();

    // Internal helpers — must be called with m_mu held.
    std::optional<CatalogStruct> _find_struct_locked(const std::string& name) const;
    std::vector<CatalogField>    _fields_locked(int struct_id) const;
    std::optional<CatalogVtable> _vtable_locked(int struct_id, bool slots) const;
    std::vector<CatalogRelationship> _relationships_locked(int struct_id,
                                                           bool from_side) const;

    // ── Embedded-schema bridge ────────────────────────────────────────────
    // The embedded DB keys every table by TEXT ids (struct_id = "Scene"),
    // while the public API exposes dense int ids.  The index below is built
    // lazily from `structs ORDER BY name` (the same order all_structs()
    // returns), so an int id is exactly the row's position in that list and
    // stays stable for the lifetime of the process.  All three helpers
    // require m_mu to be held.
    void _build_struct_index_locked() const;
    std::optional<int> _id_for_key_locked(const std::string& key) const;
    const std::string* _key_for_id_locked(int id) const;

    mutable std::vector<std::string>            m_struct_key_by_id;
    mutable std::unordered_map<std::string, int> m_struct_id_by_key;
    mutable bool                                 m_struct_index_built = false;

    mutable std::mutex m_mu;
    sqlite3*  m_embedded = nullptr;   // in-memory, read-only canonical DB
    sqlite3*  m_user     = nullptr;   // on-disk, writable user research DB
    bool      m_ready    = false;
    std::string m_user_db_path;
};

} // namespace swordfare::research
