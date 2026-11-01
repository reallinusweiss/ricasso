// =============================================================================
// hex_inspector.cpp — Live guest-memory hex view implementation
// =============================================================================
//
// Layout (one row == m_cols bytes):
//   [Offset]  [HH HH HH HH  HH HH HH HH  |  HH HH HH HH  HH HH HH HH]  [ASCII]  [Field]
//
// All guest reads are bounds-checked; out-of-range bytes render as "??" and
// are never dereferenced.  No heap is allocated inside draw().
// =============================================================================

#include "game/research/hex_inspector.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <algorithm>

namespace swordfare::research {

// ---------------------------------------------------------------------------
// Palette (matches the rest of the Memory Research tab)
// ---------------------------------------------------------------------------
static const ImVec4 kFloatTint  (0.7f, 0.4f, 0.9f, 0.30f); // purple
static const ImVec4 kIntTint    (0.3f, 0.8f, 0.4f, 0.30f); // green
static const ImVec4 kPtrTint    (0.9f, 0.5f, 0.2f, 0.30f); // orange
static const ImVec4 kStringTint (0.2f, 0.8f, 0.9f, 0.30f); // cyan
static const ImVec4 kVectorTint (0.9f, 0.8f, 0.2f, 0.30f); // yellow
static const ImVec4 kBoolTint   (0.9f, 0.3f, 0.3f, 0.30f); // red
static const ImVec4 kUnknownTint(0.0f, 0.0f, 0.0f, 0.00f); // transparent
static const ImVec4 kOutOfRange (0.55f, 0.55f, 0.55f, 1.0f); // grey text

static bool contains_ci(const std::string& hay, const char* needle) {
    if (!needle || !*needle) return false;
    const size_t n = strlen(needle);
    if (hay.size() < n) return false;
    for (size_t i = 0; i + n <= hay.size(); ++i) {
        size_t j = 0;
        for (; j < n; ++j) {
            if (std::tolower((unsigned char)hay[i + j]) !=
                std::tolower((unsigned char)needle[j]))
                break;
        }
        if (j == n) return true;
    }
    return false;
}

// Map a field_type string to its tint color (alpha 0.30).
static ImVec4 type_tint(const std::string& type) {
    // Order matters: check the more specific names first.
    if (contains_ci(type, "string"))                                    return kStringTint;
    if (contains_ci(type, "vector"))                                    return kVectorTint;
    if (contains_ci(type, "bool"))                                      return kBoolTint;
    if (contains_ci(type, "float") || contains_ci(type, "double") ||
        contains_ci(type, "vec2")  || contains_ci(type, "vec3")   ||
        contains_ci(type, "vec4")  || contains_ci(type, "quat"))        return kFloatTint;
    if (contains_ci(type, "ptr")   || contains_ci(type, "pointer") ||
        contains_ci(type, "*")     || contains_ci(type, "ref"))         return kPtrTint;
    if (contains_ci(type, "int")   || contains_ci(type, "uint") ||
        contains_ci(type, "size_t")|| contains_ci(type, "char"))        return kIntTint;
    return kUnknownTint;
}

static ImVec4 opaque_of(ImVec4 c) {
    c.w = 1.0f;
    return c;
}

// ---------------------------------------------------------------------------
// set_address
// ---------------------------------------------------------------------------
void HexInspector::set_address(uint64_t va, const CatalogStruct* s,
                               const std::vector<CatalogField>* fields) {
    m_base_va  = va;
    m_struct   = s;
    m_fields   = fields;
    m_view_row = 0;
}

uint64_t HexInspector::view_address() const {
    return m_base_va + (uint64_t)m_view_row * (uint64_t)m_cols;
}

// ---------------------------------------------------------------------------
// highlight management
// ---------------------------------------------------------------------------
void HexInspector::highlight(uint64_t va, uint32_t len, ImVec4 color) {
    if (len == 0) len = 1;
    for (auto& h : m_highlights) {
        if (h.va == va && h.len == len) { h.color = color; return; }
    }
    Highlight h;
    h.va    = va;
    h.len   = len;
    h.color = color;
    m_highlights.push_back(h);
}

void HexInspector::clear_highlights() {
    m_highlights.clear();
}

void HexInspector::scroll_rows(int delta) {
    int next = m_view_row + delta;
    // Keep at least row 0; upper bound is only limited by mem_size in draw().
    m_view_row = std::max(0, next);
}

// ---------------------------------------------------------------------------
// field lookup
// ---------------------------------------------------------------------------
const CatalogField* HexInspector::field_at_offset(uint32_t offset) const {
    if (!m_fields) return nullptr;
    for (const auto& f : *m_fields) {
        uint32_t off = f.offset_arm64;
        if (off == 0 && f.offset_arm32 != 0) off = f.offset_arm32;
        uint32_t sz  = f.size_bytes ? f.size_bytes : 1;
        if (offset >= off && offset < off + sz)
            return &f;
    }
    return nullptr;
}

const CatalogField* HexInspector::field_starting_in_row(uint32_t row_off) const {
    if (!m_fields) return nullptr;
    for (const auto& f : *m_fields) {
        uint32_t off = f.offset_arm64;
        if (off == 0 && f.offset_arm32 != 0) off = f.offset_arm32;
        if (off >= row_off && off < row_off + (uint32_t)m_cols)
            return &f;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// draw
// ---------------------------------------------------------------------------
void HexInspector::draw(const uint8_t* guest_memory, uint64_t mem_size) {
    // ── Toolbar ───────────────────────────────────────────────────────────
    ImGui::SetNextItemWidth(170.0f);
    if (ImGui::InputTextWithHint("##hex_goto", "Go to offset: 0x...",
                                 m_goto_buf, sizeof(m_goto_buf),
                                 ImGuiInputTextFlags_CharsHexadecimal |
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
        uint64_t va = (uint64_t)strtoull(m_goto_buf, nullptr, 16);
        set_address(va, nullptr, nullptr);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Enter a guest VA in hex and press Enter");

    ImGui::SameLine();
    if (ImGui::SmallButton("Cols 8"))  set_columns(8);
    ImGui::SameLine();
    if (ImGui::SmallButton("Cols 16")) set_columns(16);

    ImGui::SameLine();
    if (m_struct)
        ImGui::TextColored(opaque_of(kIntTint), "  %s @ 0x%llX",
                           m_struct->name.c_str(),
                           (unsigned long long)m_base_va);
    else if (m_base_va)
        ImGui::TextColored(kOutOfRange, "  0x%llX",
                           (unsigned long long)m_base_va);

    // ── Placeholder when no usable address is set ─────────────────────────
    if (m_base_va == 0 || guest_memory == nullptr || m_base_va >= mem_size) {
        ImGui::Separator();
        ImGui::TextColored(kOutOfRange, "No address set");
        return;
    }

    ImGui::Separator();

    // ── Scrolling region ──────────────────────────────────────────────────
    const float row_h = ImGui::GetTextLineHeight() + ImGui::GetStyle().ItemSpacing.y;

    if (ImGui::BeginChild("##hex_child", ImVec2(0, 0), false,
                          ImGuiWindowFlags_HorizontalScrollbar)) {

        // Keyboard navigation (only while the child holds focus).
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
            if (ImGui::IsKeyPressed(ImGuiKey_PageDown))      scroll_rows(16);
            else if (ImGui::IsKeyPressed(ImGuiKey_PageUp))   scroll_rows(-16);
            else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) scroll_rows(1);
            else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))   scroll_rows(-1);
        }

        const uint64_t view_base = view_address();
        ImDrawList*    dl        = ImGui::GetWindowDrawList();

        // Scratch buffers — no per-frame heap.
        char   offbuf[24];
        char   bytebuf[4];
        char   asciibuf[40];
        char   toolbuf[192];

        for (int r = 0; r < m_display_rows; ++r) {
            const uint64_t row_va  = view_base + (uint64_t)r * (uint64_t)m_cols;
            const uint64_t row_off = (uint64_t)m_view_row * (uint64_t)m_cols +
                                     (uint64_t)r * (uint64_t)m_cols;

            // Offset column (grey, monospace-ish).
            snprintf(offbuf, sizeof(offbuf), "+0x%04llX",
                     (unsigned long long)row_off);
            ImGui::TextColored(kOutOfRange, "%s", offbuf);
            ImGui::SameLine(0.0f, 10.0f);

            asciibuf[0] = '\0';
            int  ascii_pos = 0;

            for (int b = 0; b < m_cols; ++b) {
                const uint64_t va       = row_va + (uint64_t)b;
                const bool     in_range = (va < mem_size);

                // Resolve colors: highlight wins over field tint.
                ImVec4 bg   = kUnknownTint;
                ImVec4 text = ImVec4(1, 1, 1, 1);

                const Highlight* hl = nullptr;
                for (const auto& h : m_highlights) {
                    if (va >= h.va && va < h.va + (uint64_t)h.len) { hl = &h; break; }
                }

                const CatalogField* f = nullptr;
                if (in_range && va >= m_base_va) {
                    uint64_t rel = va - m_base_va;
                    if (rel < 0xFFFFFFFFULL)
                        f = field_at_offset((uint32_t)rel);
                }

                if (hl) {
                    bg   = hl->color;
                    text = opaque_of(hl->color);
                } else if (f) {
                    ImVec4 t = type_tint(f->field_type);
                    if (t.w > 0.0f) { bg = t; text = opaque_of(t); }
                }

                // Draw background tint (field / highlight).
                if (bg.w > 0.0f) {
                    ImVec2 p  = ImGui::GetCursorScreenPos();
                    ImVec2 ts = ImGui::CalcTextSize("FF");
                    dl->AddRectFilled(p, ImVec2(p.x + ts.x + 2.0f, p.y + ts.y),
                                      ImGui::GetColorU32(bg));
                }

                if (!in_range) {
                    ImGui::TextColored(kOutOfRange, "??");
                    if (ascii_pos < (int)sizeof(asciibuf) - 1)
                        asciibuf[ascii_pos++] = ' ';
                } else {
                    unsigned byte = (unsigned)guest_memory[va];
                    snprintf(bytebuf, sizeof(bytebuf), "%02X", byte);
                    if (text.x == 1.0f && text.y == 1.0f && text.z == 1.0f)
                        ImGui::TextUnformatted(bytebuf);
                    else
                        ImGui::TextColored(text, "%s", bytebuf);

                    if (ascii_pos < (int)sizeof(asciibuf) - 1)
                        asciibuf[ascii_pos++] =
                            std::isprint((int)byte) ? (char)byte : '.';

                    if (f && ImGui::IsItemHovered()) {
                        snprintf(toolbuf, sizeof(toolbuf),
                                 "%s : %s @ +0x%X [%s]",
                                 f->field_name.c_str(), f->field_type.c_str(),
                                 (unsigned)(f->offset_arm64 ? f->offset_arm64
                                                            : f->offset_arm32),
                                 f->confidence.c_str());
                        ImGui::SetTooltip("%s", toolbuf);
                    }
                }

                // Spacing: gap between the two 8-byte groups, tighter inside.
                const bool last = (b == m_cols - 1);
                if (!last)
                    ImGui::SameLine(0.0f, (b == (m_cols / 2) - 1) ? 12.0f : 5.0f);
            }

            asciibuf[ascii_pos] = '\0';

            // ASCII column.
            ImGui::SameLine(0.0f, 14.0f);
            ImGui::TextColored(ImVec4(0.65f, 0.75f, 0.85f, 1.0f), "|%s|", asciibuf);

            // Field annotation — only when a field STARTS in this row.
            const CatalogField* fs = field_starting_in_row((uint32_t)row_off);
            if (fs) {
                ImVec4 t = type_tint(fs->field_type);
                if (t.w <= 0.0f) t = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
                ImGui::SameLine(0.0f, 16.0f);
                ImGui::TextColored(opaque_of(t), "<- %s (%s)",
                                   fs->field_name.c_str(), fs->field_type.c_str());
            }

            (void)row_h; // reserved for future row-height overrides
        }
    }
    ImGui::EndChild();
}

} // namespace swordfare::research
