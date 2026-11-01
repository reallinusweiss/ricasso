// =============================================================================
// struct_decoder.h — Guest memory struct field reader
//
// Given a guest VA and a CatalogStruct from RecoveryCatalog, reads the field
// values from g_guest_memory and formats them as display strings.
//
// Safety rules (from the plan):
//   • All reads are bounds-checked against GUEST_MEM_SIZE.
//   • Pointer-typed fields are read but NOT dereferenced to avoid cascaded
//     faults; they are displayed as hex addresses only.
//   • std::string (SSO) fields are decoded safely: short path reads up to
//     23 bytes inline; long path reads the heap pointer + size, then copies
//     up to 128 bytes from guest memory if the heap pointer is in range.
//   • std::vector fields show {begin, end, cap} as 3 × uint64_t.
//   • RB-tree (std::set/std::map) fields show only the sentinel header —
//     no tree traversal.
//   • boost::intrusive_ptr / shared_ptr fields show the contained pointer
//     and (for intrusive) the refcount at obj+0x08.
//   • No memcpy of entire structs — only field-by-field reads.
// =============================================================================
#pragma once

#include "game/research/recovery_catalog.h"
#include <cstdint>
#include <string>
#include <vector>

namespace swordfare::research {

// ---------------------------------------------------------------------------
// DecodedField — one field's value after read + format
// ---------------------------------------------------------------------------
struct DecodedField {
    std::string field_name;
    std::string field_type;
    uint32_t    offset_arm64 = 0;
    uint32_t    size_bytes   = 0;
    std::string confidence;
    std::string value_str;     // formatted for display
    uint64_t    raw_u64  = 0;  // raw first 8 bytes (or 0 if out-of-range)
    bool        in_range = false;  // false = VA+offset is outside guest memory
};

// ---------------------------------------------------------------------------
// StructDecoder
// ---------------------------------------------------------------------------
class StructDecoder {
public:
    // guest_memory: pointer to the 4 GiB host-mapped buffer (g_guest_memory).
    // guest_mem_size: size of that buffer in bytes (GUEST_MEM_SIZE = 0xE0000000).
    explicit StructDecoder(const uint8_t* guest_memory, uint64_t guest_mem_size)
        : m_mem(guest_memory), m_mem_size(guest_mem_size) {}

    const uint8_t* guest_memory() const { return m_mem; }
    uint64_t       guest_mem_size() const { return m_mem_size; }

    // Decode all known fields of 'cs' at guest VA 'base'.
    // Returns one DecodedField per entry in RecoveryCatalog::fields_for_struct().
    std::vector<DecodedField> decode(uint64_t base,
                                     const CatalogStruct& cs,
                                     const std::vector<CatalogField>& fields) const;

    // Decode a single field by name; returns nullopt if not found.
    std::optional<DecodedField> decode_field(uint64_t base,
                                              const CatalogStruct& cs,
                                              const std::vector<CatalogField>& fields,
                                              const std::string& field_name) const;

    // Format a raw VA as "0x%08X  (struct_name?)" using the vtable catalog.
    std::string format_ptr(uint64_t ptr_va) const;

    // Attempt to read an SSO std::string from guest memory and return it as
    // a host std::string.  Returns empty string on any bounds violation.
    std::string read_sso_string(uint64_t str_va) const;

private:
    // Safely read N bytes from guest memory at va → out.
    // Returns false if any byte would be out of range.
    bool safe_read(uint64_t va, void* out, uint32_t n) const;

    // Read a uint8/16/32/64 safely.
    bool read_u8 (uint64_t va, uint8_t&  out) const;
    bool read_u16(uint64_t va, uint16_t& out) const;
    bool read_u32(uint64_t va, uint32_t& out) const;
    bool read_u64(uint64_t va, uint64_t& out) const;

    // Format a field value based on its type tag.
    std::string format_field(const CatalogField& f, uint64_t base) const;

    const uint8_t* m_mem      = nullptr;
    uint64_t       m_mem_size = 0;
};

} // namespace swordfare::research
