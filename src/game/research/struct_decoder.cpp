// =============================================================================
// struct_decoder.cpp — StructDecoder implementation
// =============================================================================

#include "game/research/struct_decoder.h"
#include <cstring>
#include <cstdio>
#include <cctype>
#include <algorithm>

namespace swordfare::research {

// ---------------------------------------------------------------------------
// Safe memory primitives
// ---------------------------------------------------------------------------
bool StructDecoder::safe_read(uint64_t va, void* out, uint32_t n) const {
    if (!m_mem || va == 0 || va + n > m_mem_size) return false;
    std::memcpy(out, m_mem + va, n);
    return true;
}
bool StructDecoder::read_u8 (uint64_t va, uint8_t&  v) const { return safe_read(va, &v, 1); }
bool StructDecoder::read_u16(uint64_t va, uint16_t& v) const { return safe_read(va, &v, 2); }
bool StructDecoder::read_u32(uint64_t va, uint32_t& v) const { return safe_read(va, &v, 4); }
bool StructDecoder::read_u64(uint64_t va, uint64_t& v) const { return safe_read(va, &v, 8); }

// ---------------------------------------------------------------------------
// read_sso_string  — decode ARM64 libstdc++ std::string (24-byte SSO layout)
//   [0..7]  : short: inline data (first 8 bytes); long: heap ptr
//   [8..15] : short: inline data (next 8 bytes); long: size
//   [16..23]: short: inline data + size-byte; long: capacity (+1)
//
// Short flag: LSB of byte[0] == 0 AND size is stored in the top 7 bits of
// byte 23 (some ABIs) OR short capacity = 23, with size in a different slot.
//
// Swordigo uses ARM64 Linux libstdc++ v3 (GCC LLVM-based Clang on Android):
//   Short flag: byte[23] bit7 == 0
//   Short size: byte[23] >> 1 (range 0..23)
//   Short data: bytes[0..22]
//   Long flag:  byte[23] bit7 == 1
//   Long heap:  *((uint64_t*)str+0)
//   Long size:  *((uint64_t*)str+8)
//
// However the Swordigo binary was compiled with Clang on Android (armeabi-v7a
// and arm64-v8a), which actually uses the __compressed_pair layout from libc++
// (LLVM):
//   Short flag: __size_ field == capacity+1 check — actually:
//   The simple heuristic: if the first byte (treated as the cap byte in short
//   mode) has its LSB set → long string.
//   Long heap:  word 0
//   Long size:  word 1
//   Short size: (byte[23] & 0x7F) >> 1 — standard libc++ short form
// ---------------------------------------------------------------------------
std::string StructDecoder::read_sso_string(uint64_t str_va) const {
    if (!m_mem || str_va == 0 || str_va + 24 > m_mem_size) return "";

    const uint8_t* p = m_mem + str_va;
    // Check libc++ short/long flag: byte[23] bit0 == 1 → long
    uint8_t flag = p[23];
    if ((flag & 1) == 0) {
        // Short string
        int sz = (flag >> 1) & 0x7F;
        if (sz < 0 || sz > 23) return "";
        char buf[24] = {};
        std::memcpy(buf, p, (size_t)sz);
        // Validate printable ASCII (for research output)
        for (int i = 0; i < sz; ++i)
            if (!std::isprint((unsigned char)buf[i])) buf[i] = '.';
        return std::string(buf, (size_t)sz);
    } else {
        // Long string: ptr at [0..7], size at [8..15]
        uint64_t heap_ptr, sz64;
        std::memcpy(&heap_ptr, p,     8);
        std::memcpy(&sz64,     p + 8, 8);
        if (heap_ptr == 0 || heap_ptr + sz64 > m_mem_size || sz64 > 128)
            return "";
        size_t sz = static_cast<size_t>(sz64);
        std::string result(sz, '\0');
        std::memcpy(result.data(), m_mem + heap_ptr, sz);
        for (auto& c : result)
            if (!std::isprint((unsigned char)(uint8_t)c)) c = '.';
        return result;
    }
}

// ---------------------------------------------------------------------------
// format_field — type-aware formatting
// ---------------------------------------------------------------------------
std::string StructDecoder::format_field(const CatalogField& f, uint64_t base) const {
    uint64_t field_va = base + f.offset_arm64;
    char buf[256];

    const std::string& t = f.field_type;

    // ── Integral types ─────────────────────────────────────────────────────
    if (t == "uint8_t" || t == "bool" || t == "uint8") {
        uint8_t v = 0;
        if (!read_u8(field_va, v)) return "<OOB>";
        if (t == "bool")
            snprintf(buf, sizeof(buf), "%s (0x%02X)", v ? "true" : "false", v);
        else
            snprintf(buf, sizeof(buf), "%u (0x%02X)", v, v);
        return buf;
    }
    if (t == "int8_t" || t == "char") {
        uint8_t v = 0;
        if (!read_u8(field_va, v)) return "<OOB>";
        snprintf(buf, sizeof(buf), "%d (0x%02X)", (int8_t)v, v);
        return buf;
    }
    if (t == "uint16_t" || t == "int16_t") {
        uint16_t v = 0;
        if (!read_u16(field_va, v)) return "<OOB>";
        snprintf(buf, sizeof(buf), "%u (0x%04X)", v, v);
        return buf;
    }
    if (t == "int32_t" || t == "int") {
        uint32_t v = 0;
        if (!read_u32(field_va, v)) return "<OOB>";
        snprintf(buf, sizeof(buf), "%d (0x%08X)", (int32_t)v, v);
        return buf;
    }
    if (t == "uint32_t" || t == "unsigned int" || t == "uint32") {
        uint32_t v = 0;
        if (!read_u32(field_va, v)) return "<OOB>";
        snprintf(buf, sizeof(buf), "%u (0x%08X)", v, v);
        return buf;
    }
    if (t == "float" || t == "float32") {
        uint32_t raw = 0;
        if (!read_u32(field_va, raw)) return "<OOB>";
        float fv;
        std::memcpy(&fv, &raw, 4);
        snprintf(buf, sizeof(buf), "%.6f (0x%08X)", (double)fv, raw);
        return buf;
    }
    if (t == "double" || t == "float64") {
        uint64_t raw = 0;
        if (!read_u64(field_va, raw)) return "<OOB>";
        double dv;
        std::memcpy(&dv, &raw, 8);
        snprintf(buf, sizeof(buf), "%.8f", dv);
        return buf;
    }
    if (t == "uint64_t" || t == "int64_t" || t == "size_t" || t == "ptrdiff_t") {
        uint64_t v = 0;
        if (!read_u64(field_va, v)) return "<OOB>";
        snprintf(buf, sizeof(buf), "0x%016llX", (unsigned long long)v);
        return buf;
    }

    // ── Pointer types ──────────────────────────────────────────────────────
    if (t.find('*') != std::string::npos ||
        t.find("_ptr") != std::string::npos ||
        t == "void*" || t == "uintptr_t") {
        uint64_t ptr = 0;
        if (!read_u64(field_va, ptr)) {
            // Try 32-bit pointer (ARM32 embedded structs)
            uint32_t ptr32 = 0;
            if (f.size_bytes == 4 && read_u32(field_va, ptr32)) {
                ptr = ptr32;
            } else {
                return "<OOB>";
            }
        }
        if (ptr == 0) return "nullptr";
        snprintf(buf, sizeof(buf), "0x%08llX", (unsigned long long)ptr);
        return buf;
    }

    // ── std::string (SSO) ─────────────────────────────────────────────────
    if (t == "std::string" || t == "string") {
        std::string s = read_sso_string(field_va);
        snprintf(buf, sizeof(buf), "\"%.*s\"", (int)std::min(s.size(), (size_t)64), s.c_str());
        return buf;
    }

    // ── std::vector<T> — show {begin, end, cap} ───────────────────────────
    if (t.substr(0, 12) == "std::vector<" || t == "vector") {
        uint64_t begin_ptr = 0, end_ptr = 0, cap_ptr = 0;
        bool ok = read_u64(field_va,      begin_ptr) &&
                  read_u64(field_va + 8,  end_ptr) &&
                  read_u64(field_va + 16, cap_ptr);
        if (!ok) return "<OOB>";
        // Compute element count: (end - begin) / elem_size
        // We don't know elem_size here, so just show bytes
        int64_t used_bytes = (int64_t)(end_ptr - begin_ptr);
        int64_t cap_bytes  = (int64_t)(cap_ptr - begin_ptr);
        snprintf(buf, sizeof(buf),
                 "ptr=0x%08llX  used=%lld B  cap=%lld B",
                 (unsigned long long)begin_ptr,
                 (long long)std::max((int64_t)0, used_bytes),
                 (long long)std::max((int64_t)0, cap_bytes));
        return buf;
    }

    // ── boost::shared_ptr / boost::intrusive_ptr ──────────────────────────
    if (t.find("shared_ptr") != std::string::npos) {
        uint64_t raw_ptr = 0, ctrl = 0;
        bool ok = read_u64(field_va, raw_ptr) && read_u64(field_va + 8, ctrl);
        if (!ok) return "<OOB>";
        snprintf(buf, sizeof(buf), "ptr=0x%08llX ctrl=0x%08llX",
                 (unsigned long long)raw_ptr, (unsigned long long)ctrl);
        return buf;
    }
    if (t.find("intrusive_ptr") != std::string::npos) {
        uint64_t raw_ptr = 0;
        if (!read_u64(field_va, raw_ptr)) return "<OOB>";
        if (raw_ptr == 0) return "nullptr";
        uint32_t refcount = 0;
        read_u32(raw_ptr + 8, refcount); // intrusive refcount at obj+0x08
        snprintf(buf, sizeof(buf), "0x%08llX (ref=%u)",
                 (unsigned long long)raw_ptr, refcount);
        return buf;
    }

    // ── RB-tree (std::set / std::map) — show only header node ────────────
    if (t.find("std::set") != std::string::npos ||
        t.find("std::map") != std::string::npos) {
        // 24-byte header: {color, parent, left, right, ...}
        // Just show whether the tree appears populated (node_count via header)
        uint64_t h0 = 0, h1 = 0, h2 = 0;
        bool ok = read_u64(field_va,      h0) &&
                  read_u64(field_va + 8,  h1) &&
                  read_u64(field_va + 16, h2);
        if (!ok) return "<OOB>";
        snprintf(buf, sizeof(buf), "RBTree hdr=[0x%llX 0x%llX 0x%llX]",
                 (unsigned long long)h0, (unsigned long long)h1,
                 (unsigned long long)h2);
        return buf;
    }

    // ── Fallback: show raw hex bytes ──────────────────────────────────────
    uint32_t show_bytes = std::min(f.size_bytes, (uint32_t)8);
    if (show_bytes == 0) show_bytes = 4;
    uint64_t raw = 0;
    if (!safe_read(field_va, &raw, show_bytes)) return "<OOB>";
    snprintf(buf, sizeof(buf), "0x%0*llX  (%u B)",
             (int)(show_bytes * 2), (unsigned long long)raw, f.size_bytes);
    return buf;
}

// ---------------------------------------------------------------------------
// decode
// ---------------------------------------------------------------------------
std::vector<DecodedField>
StructDecoder::decode(uint64_t base,
                      const CatalogStruct& cs,
                      const std::vector<CatalogField>& fields) const {
    std::vector<DecodedField> out;
    out.reserve(fields.size());
    for (const auto& f : fields) {
        DecodedField df;
        df.field_name   = f.field_name;
        df.field_type   = f.field_type;
        df.offset_arm64 = f.offset_arm64;
        df.size_bytes   = f.size_bytes;
        df.confidence   = f.confidence;
        uint64_t field_va = base + f.offset_arm64;
        df.in_range = (field_va + std::max((uint32_t)1, f.size_bytes) <= m_mem_size);
        if (df.in_range) {
            read_u64(field_va, df.raw_u64);
            df.value_str = format_field(f, base);
        } else {
            df.value_str = "<OUT OF RANGE>";
        }
        out.push_back(std::move(df));
    }
    return out;
}

std::optional<DecodedField>
StructDecoder::decode_field(uint64_t base,
                             const CatalogStruct& cs,
                             const std::vector<CatalogField>& fields,
                             const std::string& field_name) const {
    for (const auto& f : fields) {
        if (f.field_name != field_name) continue;
        DecodedField df;
        df.field_name   = f.field_name;
        df.field_type   = f.field_type;
        df.offset_arm64 = f.offset_arm64;
        df.size_bytes   = f.size_bytes;
        df.confidence   = f.confidence;
        uint64_t field_va = base + f.offset_arm64;
        df.in_range = (field_va + std::max((uint32_t)1, f.size_bytes) <= m_mem_size);
        if (df.in_range) {
            read_u64(field_va, df.raw_u64);
            df.value_str = format_field(f, base);
        } else {
            df.value_str = "<OUT OF RANGE>";
        }
        return df;
    }
    return std::nullopt;
}

std::string StructDecoder::format_ptr(uint64_t ptr_va) const {
    char buf[64];
    snprintf(buf, sizeof(buf), "0x%08llX", (unsigned long long)ptr_va);
    return buf;
}

} // namespace swordfare::research
