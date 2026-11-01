// =============================================================================
// hex_inspector.h — Live guest-memory hex view with struct annotation overlay
//
// A scrollable Dear ImGui hex widget for the Memory Research tab.  It renders
// raw bytes straight out of g_guest_memory (identity-mapped: host_ptr =
// guest_memory + guest_va) and overlays the type annotations of a known
// CatalogStruct on top of the bytes each field covers.
//
// Safety (mirrors the research-system invariants):
//   • Every read is bounds-checked: va + 1 <= mem_size (GUEST_MEM_SIZE).
//   • Out-of-range bytes render as "??" in grey — never dereferenced.
//   • No per-frame heap allocation in draw(); all scratch lives in members.
// =============================================================================
#pragma once

#include "game/research/recovery_catalog.h"
#include "imgui/imgui.h"
#include <cstdint>
#include <vector>

namespace swordfare::research {

// ---------------------------------------------------------------------------
// HexInspector
// ---------------------------------------------------------------------------
class HexInspector {
public:
    // Set the base guest VA and optional CatalogStruct + fields to overlay.
    // Passing s == nullptr / fields == nullptr clears the overlay.
    void set_address(uint64_t va, const CatalogStruct* s = nullptr,
                     const std::vector<CatalogField>* fields = nullptr);

    // Called once per ImGui frame.  Renders in the current ImGui window region.
    // guest_memory: g_guest_memory pointer.  mem_size: 0xE0000000.
    void draw(const uint8_t* guest_memory, uint64_t mem_size);

    // Column count: 8 or 16 bytes per row (toggle).
    void set_columns(int cols) { m_cols = (cols == 8) ? 8 : 16; }
    int  columns() const { return m_cols; }

    // Highlighted VA range (for watchpoint matches, etc.)
    void highlight(uint64_t va, uint32_t len, ImVec4 color);
    void clear_highlights();

    // ── Navigation / configuration helpers ────────────────────────────────
    void     scroll_rows(int delta);              // +down / -up in rows
    void     set_display_rows(int rows) { if (rows > 0) m_display_rows = rows; }
    int      display_rows() const { return m_display_rows; }
    uint64_t base_address() const { return m_base_va; }
    uint64_t view_address() const;                // m_base_va + m_view_row*m_cols

private:
    // ── State ─────────────────────────────────────────────────────────────
    uint64_t                          m_base_va      = 0;
    const CatalogStruct*              m_struct       = nullptr;
    const std::vector<CatalogField>*  m_fields       = nullptr;
    int                               m_cols         = 16;   // bytes per row
    int                               m_display_rows = 32;   // rows shown
    int                               m_view_row     = 0;    // first visible row
    char                              m_goto_buf[32] = {};   // hex VA input

    struct Highlight { uint64_t va; uint32_t len; ImVec4 color; };
    std::vector<Highlight>            m_highlights;

    // Returns the CatalogField that covers the byte at 'offset' (relative to
    // m_base_va), or nullptr.
    const CatalogField* field_at_offset(uint32_t offset) const;

    // Returns the first field that STARTS inside [row_off, row_off+m_cols),
    // or nullptr.  Used for the far-right row annotation.
    const CatalogField* field_starting_in_row(uint32_t row_off) const;
};

} // namespace swordfare::research
