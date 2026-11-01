// =============================================================================
// research_workspace.cpp
// =============================================================================

#include "game/research/research_workspace.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>

namespace fs = std::filesystem;

namespace swordfare::research {

namespace {

std::string hex16(uint64_t v) {
    char b[24];
    std::snprintf(b, sizeof(b), "0x%llX", static_cast<unsigned long long>(v));
    return b;
}

std::string now_stamp() {
    const std::time_t t = std::time(nullptr);
    std::tm tmv{};
#if defined(_WIN32)
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    char b[32];
    std::strftime(b, sizeof(b), "%Y-%m-%d_%H%M", &tmv);
    return b;
}

uint64_t now_seconds() { return static_cast<uint64_t>(std::time(nullptr)); }

// Split a TSV line into fields.
std::vector<std::string> split_tab(const std::string& line) {
    std::vector<std::string> f;
    size_t start = 0;
    while (true) {
        const size_t tab = line.find('\t', start);
        if (tab == std::string::npos) { f.push_back(line.substr(start)); break; }
        f.push_back(line.substr(start, tab - start));
        start = tab + 1;
    }
    return f;
}

} // namespace

// ---------------------------------------------------------------------------
// json
// ---------------------------------------------------------------------------
namespace json {

std::string escape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n";  break;
            case '\r': o += "\\r";  break;
            case '\t': o += "\\t";  break;
            default:
                if (c < 0x20) { char b[8]; std::snprintf(b, sizeof(b), "\\u%04X", c); o += b; }
                else          o += static_cast<char>(c);
        }
    }
    return o;
}

std::string quote(const std::string& s) { return "\"" + escape(s) + "\""; }

std::string number(double v) {
    char b[48];
    if (v == static_cast<double>(static_cast<long long>(v)))
        std::snprintf(b, sizeof(b), "%lld", static_cast<long long>(v));
    else
        std::snprintf(b, sizeof(b), "%.6g", v);
    return b;
}

std::string u64(uint64_t v) {
    char b[32];
    std::snprintf(b, sizeof(b), "%llu", static_cast<unsigned long long>(v));
    return b;
}

// A deliberately small recursive-descent reader for the subset we emit.
namespace {

struct Parser {
    const std::string& s;
    size_t i = 0;

    explicit Parser(const std::string& src) : s(src) {}

    void skip_ws() {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) ++i;
    }
    bool expect(char c) {
        skip_ws();
        if (i < s.size() && s[i] == c) { ++i; return true; }
        return false;
    }
    bool peek(char c) {
        skip_ws();
        return i < s.size() && s[i] == c;
    }

    bool parse_string(std::string* out) {
        skip_ws();
        if (i >= s.size() || s[i] != '"') return false;
        ++i;
        std::string v;
        while (i < s.size()) {
            const char c = s[i++];
            if (c == '"') { *out = v; return true; }
            if (c != '\\') { v += c; continue; }
            if (i >= s.size()) return false;
            const char e = s[i++];
            switch (e) {
                case '"':  v += '"';  break;
                case '\\': v += '\\'; break;
                case '/':  v += '/';  break;
                case 'n':  v += '\n'; break;
                case 'r':  v += '\r'; break;
                case 't':  v += '\t'; break;
                case 'b':  v += '\b'; break;
                case 'f':  v += '\f'; break;
                case 'u': {
                    if (i + 4 > s.size()) return false;
                    int code = 0;
                    for (int k = 0; k < 4; ++k) {
                        const char h = s[i++];
                        code <<= 4;
                        if (h >= '0' && h <= '9') code |= h - '0';
                        else if (h >= 'a' && h <= 'f') code |= h - 'a' + 10;
                        else if (h >= 'A' && h <= 'F') code |= h - 'A' + 10;
                        else return false;
                    }
                    v += static_cast<char>(code & 0x7F);
                    break;
                }
                default: return false;
            }
        }
        return false;
    }

    // Capture a value's raw text (used for arrays of objects, which we re-parse).
    bool capture_value(std::string* out) {
        skip_ws();
        const size_t start = i;
        int depth = 0;
        bool in_str = false;
        while (i < s.size()) {
            const char c = s[i];
            if (in_str) {
                if (c == '\\') { i += 2; continue; }
                if (c == '"') in_str = false;
                ++i;
                continue;
            }
            if (c == '"') { in_str = true; ++i; continue; }
            if (c == '{' || c == '[') { ++depth; ++i; continue; }
            if (c == '}' || c == ']') {
                if (depth == 0) break;         // end of the enclosing container
                --depth; ++i;
                if (depth == 0 && *s.rbegin() != c) { /* keep going */ }
                if (depth == 0) { *out = s.substr(start, i - start); return true; }
                continue;
            }
            // A scalar ends at , } ] or whitespace at depth 0 (only when it is
            // the top-level value, which we do not use).
            if (depth == 0 && (c == ',' || c == '}')) { *out = s.substr(start, i - start); return true; }
            ++i;
        }
        if (i > start) { *out = s.substr(start, i - start); return true; }
        return false;
    }

    // Skip over a value entirely.
    bool skip_value() {
        std::string tmp;
        return capture_value(&tmp);
    }

    bool parse_object(Obj* out);
};

bool Parser::parse_object(Obj* out) {
    if (!expect('{')) return false;
    skip_ws();
    std::string body;
    if (peek('}')) { ++i; out->text = "{}"; return true; }

    while (true) {
        std::string key;
        if (!parse_string(&key)) return false;
        if (!expect(':')) return false;
        skip_ws();
        std::string raw;
        if (!capture_value(&raw)) return false;
        if (!body.empty()) body += ",";
        body += json::quote(key) + ":" + raw;
        skip_ws();
        if (peek(',')) { ++i; continue; }
        if (expect('}')) break;
        return false;
    }
    out->text = "{" + body + "}";
    return true;
}

} // namespace

bool Obj::get_string(const std::string& key, std::string* out) const {
    Parser p(text);
    if (!p.expect('{')) return false;
    while (true) {
        std::string k;
        if (!p.parse_string(&k)) return false;
        if (!p.expect(':')) return false;
        p.skip_ws();
        if (k == key) {
            std::string v;
            if (!p.parse_string(&v)) return false;
            if (out) *out = v;
            return true;
        }
        if (!p.skip_value()) return false;
        p.skip_ws();
        if (p.peek(',')) { ++p.i; continue; }
        if (p.expect('}')) return false;
        return false;
    }
}

bool Obj::get_number(const std::string& key, uint64_t* out) const {
    Parser p(text);
    if (!p.expect('{')) return false;
    while (true) {
        std::string k;
        if (!p.parse_string(&k)) return false;
        if (!p.expect(':')) return false;
        p.skip_ws();
        if (k == key) {
            const size_t start = p.i;
            while (p.i < text.size() && (std::isdigit(static_cast<unsigned char>(text[p.i])) ||
                                         text[p.i] == '-' || text[p.i] == '+' ||
                                         text[p.i] == 'x' || text[p.i] == 'X' ||
                                         (text[p.i] >= 'a' && text[p.i] <= 'f') ||
                                         (text[p.i] >= 'A' && text[p.i] <= 'F')))
                ++p.i;
            const std::string tok = text.substr(start, p.i - start);
            if (tok.empty()) return false;
            if (out) *out = std::strtoull(tok.c_str(), nullptr, 0);
            return true;
        }
        if (!p.skip_value()) return false;
        p.skip_ws();
        if (p.peek(',')) { ++p.i; continue; }
        if (p.expect('}')) return false;
        return false;
    }
}

std::vector<Obj> Obj::get_array(const std::string& key) const {
    std::vector<Obj> out;
    Parser p(text);
    if (!p.expect('{')) return out;
    while (true) {
        std::string k;
        if (!p.parse_string(&k)) return out;
        if (!p.expect(':')) return out;
        p.skip_ws();
        if (k == key) {
            if (!p.expect('[')) return out;
            p.skip_ws();
            if (p.peek(']')) { ++p.i; return out; }
            while (true) {
                std::string raw;
                if (!p.capture_value(&raw)) return out;
                if (!raw.empty() && raw[0] == '{') {
                    Obj child;
                    if (parse_object(raw, &child)) out.push_back(std::move(child));
                }
                p.skip_ws();
                if (p.peek(',')) { ++p.i; p.skip_ws(); continue; }
                if (p.expect(']')) return out;
                return out;
            }
        }
        if (!p.skip_value()) return out;
        p.skip_ws();
        if (p.peek(',')) { ++p.i; continue; }
        return out;
    }
}

bool parse_object(const std::string& src, Obj* out) {
    if (!out) return false;
    Parser p(src);
    Obj o;
    if (!p.parse_object(&o)) return false;
    p.skip_ws();
    if (p.i != src.size()) return false;
    *out = std::move(o);
    return true;
}

} // namespace json

// ---------------------------------------------------------------------------
// ResearchWorkspace
// ---------------------------------------------------------------------------
std::string ResearchWorkspace::path_of(const std::string& relative) const {
    if (m_root.empty()) return relative;
    return (fs::path(m_root) / relative).string();
}

std::string ResearchWorkspace::build_profile_path(const std::string& build_id) const {
    return path_of((fs::path("builds") / (build_id + ".json")).string());
}

bool ResearchWorkspace::open(const std::string& root, std::string* err) {
    m_root = root.empty() ? std::string(".swordfare") : root;
    std::error_code ec;
    for (const char* sub : {"builds", "research", "sessions"}) {
        fs::create_directories(fs::path(m_root) / sub, ec);
        if (ec) {
            if (err) *err = "cannot create " + path_of(sub) + ": " + ec.message();
            m_open = false;
            return false;
        }
    }
    m_open = true;
    // Adopt whatever research already exists — this is the whole point of the
    // layout: reopening after a crash must find the previous work.
    std::string load_err;
    load_research(&load_err);
    return true;
}

bool ResearchWorkspace::write_text_file(const std::string& path,
                                        const std::string& text,
                                        std::string* err) const {
    // Write to a temporary and rename, so a kill mid-write cannot leave a
    // truncated file where the researcher's research used to be.
    const std::string tmp = path + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "wb");
    if (!f) { if (err) *err = "cannot write " + tmp; return false; }
    const size_t n = std::fwrite(text.data(), 1, text.size(), f);
    std::fclose(f);
    if (n != text.size()) { if (err) *err = "short write to " + tmp; return false; }

    std::error_code ec;
    fs::rename(tmp, path, ec);
    if (ec) {
        fs::remove(path, ec);
        ec.clear();
        fs::rename(tmp, path, ec);
    }
    if (ec) { if (err) *err = "cannot replace " + path + ": " + ec.message(); return false; }
    return true;
}

bool ResearchWorkspace::read_text_file(const std::string& path, std::string* out) const {
    if (!out) return false;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::string data;
    char buf[4096];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) data.append(buf, n);
    std::fclose(f);
    *out = std::move(data);
    return true;
}

std::vector<std::string> ResearchWorkspace::list_dir(const std::string& dir) const {
    std::vector<std::string> out;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        if (e.is_regular_file(ec)) out.push_back(e.path().string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

// ── mappings / observations ─────────────────────────────────────────────────
void ResearchWorkspace::set_mapping(const Mapping& m) {
    for (auto& e : m_mappings) {
        if (e.var_id == m.var_id) { e = m; return; }
    }
    m_mappings.push_back(m);
}

std::optional<Mapping> ResearchWorkspace::mapping(uint64_t var_id) const {
    for (const auto& e : m_mappings)
        if (e.var_id == var_id) return e;
    return std::nullopt;
}

void ResearchWorkspace::add_observation(const Observation& o) {
    m_observations.push_back(o);
}

std::vector<Observation> ResearchWorkspace::observations_for(uint64_t var_id) const {
    std::vector<Observation> out;
    for (const auto& o : m_observations)
        if (o.var_id == var_id) out.push_back(o);
    return out;
}

// ── research/ persistence ──────────────────────────────────────────────────
bool ResearchWorkspace::save_research(std::string* err) const {
    if (!m_open) { if (err) *err = "workspace not open"; return false; }

    // symbols.json — keyed by var_id, which is what makes a rename survive.
    {
        std::string s = "{\n  \"version\": 1,\n  \"symbols\": [\n";
        const auto recs = m_symbols.all();
        for (size_t i = 0; i < recs.size(); ++i) {
            const auto& r = recs[i];
            s += "    {";
            s += "\"var_id\": " + json::u64(r.var_id);
            s += ", \"var_id_hex\": " + json::quote(hex16(r.var_id));
            s += ", \"base_name\": " + json::quote(r.base_name);
            s += ", \"user_name\": " + json::quote(r.user_name);
            s += ", \"provenance\": " + json::quote(provenance_name(r.provenance));
            s += ", \"canonical\": " + json::quote(r.canonical);
            s += ", \"first_seen_boot\": " + json::u64(r.first_seen_boot);
            s += "}";
            if (i + 1 < recs.size()) s += ",";
            s += "\n";
        }
        s += "  ]\n}\n";
        if (!write_text_file(path_of("research/symbols.json"), s, err)) return false;
    }

    // mappings.json
    {
        std::string s = "{\n  \"version\": 1,\n  \"mappings\": [\n";
        for (size_t i = 0; i < m_mappings.size(); ++i) {
            const auto& m = m_mappings[i];
            s += "    {";
            s += "\"var_id\": " + json::u64(m.var_id);
            s += ", \"module\": " + json::quote(m.module);
            s += ", \"rva\": " + json::quote(hex16(m.rva));
            s += ", \"offset\": " + json::quote(hex16(m.offset));
            s += ", \"width\": " + json::u64(m.width);
            s += ", \"type\": " + json::quote(m.type_name);
            s += ", \"kind\": " + json::quote(mem_kind_name(m.kind));
            s += ", \"container\": " + json::quote(m.container);
            s += ", \"note\": " + json::quote(m.note);
            s += "}";
            if (i + 1 < m_mappings.size()) s += ",";
            s += "\n";
        }
        s += "  ]\n}\n";
        if (!write_text_file(path_of("research/mappings.json"), s, err)) return false;
    }

    // observations.json
    {
        std::string s = "{\n  \"version\": 1,\n  \"observations\": [\n";
        for (size_t i = 0; i < m_observations.size(); ++i) {
            const auto& o = m_observations[i];
            s += "    {";
            s += "\"var_id\": " + json::u64(o.var_id);
            s += ", \"seen_at\": " + json::u64(o.seen_at);
            s += ", \"state\": " + json::quote(o.state);
            s += ", \"confidence\": " + json::number(o.confidence);
            s += ", \"what\": " + json::quote(o.what);
            s += ", \"evidence\": " + json::quote(o.evidence);
            s += "}";
            if (i + 1 < m_observations.size()) s += ",";
            s += "\n";
        }
        s += "  ]\n}\n";
        if (!write_text_file(path_of("research/observations.json"), s, err)) return false;
    }
    return true;
}

bool ResearchWorkspace::load_research(std::string* err) {
    m_mappings.clear();
    m_observations.clear();

    std::string text;
    if (read_text_file(path_of("research/symbols.json"), &text) && !text.empty()) {
        json::Obj root;
        if (!json::parse_object(text, &root)) {
            if (err) *err = "symbols.json is malformed; leaving the identity store untouched";
        } else {
            for (const auto& sym : root.get_array("symbols")) {
                Identity id;
                uint64_t    raw = 0;
                std::string s;
                if (sym.get_number("var_id", &raw)) id.var_id = raw;
                if (sym.get_string("base_name", &s))  id.base_name = s;
                if (sym.get_string("canonical", &s))  id.canonical = s;
                if (sym.get_string("provenance", &s)) {
                    if      (s == "OBSERVED")   id.provenance = Provenance::Observed;
                    else if (s == "SUSPECTED")  id.provenance = Provenance::Inferred;
                    else if (s == "CORRELATED") id.provenance = Provenance::Correlated;
                    else if (s == "RECOVERED")  id.provenance = Provenance::Recovered;
                    else if (s == "CONFIRMED")  id.provenance = Provenance::Confirmed;
                    else                        id.provenance = Provenance::Unknown;
                }
                uint64_t boot = 0;
                sym.get_number("first_seen_boot", &boot);
                m_symbols.remember(id, boot);
                std::string user;
                if (sym.get_string("user_name", &user) && !user.empty())
                    m_symbols.rename(id.var_id, user);
            }
        }
    }

    if (read_text_file(path_of("research/mappings.json"), &text) && !text.empty()) {
        json::Obj root;
        if (json::parse_object(text, &root)) {
            for (const auto& mv : root.get_array("mappings")) {
                Mapping m;
                uint64_t raw = 0;
                std::string s;
                if (mv.get_number("var_id", &raw)) m.var_id = raw;
                if (mv.get_string("module", &s))   m.module = s;
                if (mv.get_string("rva", &s))      m.rva = std::strtoull(s.c_str(), nullptr, 0);
                if (mv.get_string("offset", &s))   m.offset = std::strtoull(s.c_str(), nullptr, 0);
                if (mv.get_number("width", &raw))  m.width = raw;
                if (mv.get_string("type", &s))     m.type_name = s;
                if (mv.get_string("container", &s)) m.container = s;
                if (mv.get_string("note", &s))     m.note = s;
                if (mv.get_string("kind", &s)) {
                    if      (s == "global")        m.kind = MemKind::Global;
                    else if (s == "stack-local")   m.kind = MemKind::StackLocal;
                    else if (s == "struct-field")  m.kind = MemKind::StructField;
                    else if (s == "heap-instance") m.kind = MemKind::HeapInstance;
                    else if (s == "vtable-ptr")    m.kind = MemKind::VtablePtr;
                    else if (s == "string")        m.kind = MemKind::String;
                    else if (s == "array-element") m.kind = MemKind::ArrayElement;
                    else                           m.kind = MemKind::Unknown;
                }
                m_mappings.push_back(m);
            }
        }
    }

    if (read_text_file(path_of("research/observations.json"), &text) && !text.empty()) {
        json::Obj root;
        if (json::parse_object(text, &root)) {
            for (const auto& ov : root.get_array("observations")) {
                Observation o;
                uint64_t raw = 0;
                std::string s;
                if (ov.get_number("var_id", &raw)) o.var_id = raw;
                if (ov.get_number("seen_at", &raw)) o.seen_at = raw;
                if (ov.get_string("state", &s))    o.state = s;
                if (ov.get_string("what", &s))     o.what = s;
                if (ov.get_string("evidence", &s)) o.evidence = s;
                m_observations.push_back(o);
            }
        }
    }
    return true;
}

std::string ResearchWorkspace::explain(uint64_t var_id) const {
    std::string s;
    const auto rec = m_symbols.find(var_id);
    if (rec) {
        s += "identity " + hex16(var_id) + "\n";
        s += "  resolved name : " + (rec->base_name.empty() ? std::string("<none>") : rec->base_name) + "\n";
        if (!rec->user_name.empty()) s += "  renamed to    : " + rec->user_name + "  (identity unchanged)\n";
        s += std::string("  provenance    : ") + provenance_name(rec->provenance) + "\n";
        s += "  static key    : " + rec->canonical + "\n";
    } else {
        s += "identity " + hex16(var_id) + " is not in the research store yet\n";
    }

    if (const auto m = mapping(var_id)) {
        s += "  static site   : " + m->module + "+" + hex16(m->rva) +
             "  offset +" + hex16(m->offset) + "\n";
        if (!m->container.empty()) s += "  container     : " + m->container + "\n";
        if (!m->type_name.empty()) s += "  type          : " + m->type_name +
                                        " (" + std::to_string(m->width) + "B)\n";
    }

    const auto obs = observations_for(var_id);
    if (obs.empty()) {
        s += "  no runtime observations recorded\n";
    } else {
        s += "  why we think this:\n";
        for (const auto& o : obs) {
            char b[64];
            std::snprintf(b, sizeof(b), "    [%s %3.0f%%] ", o.state.c_str(), o.confidence * 100.0);
            s += std::string(b) + o.what;
            if (!o.evidence.empty()) s += "   (" + o.evidence + ")";
            s += "\n";
        }
    }
    return s;
}

// ── builds/ ────────────────────────────────────────────────────────────────
bool ResearchWorkspace::save_build_profile(const BuildProfile& p, std::string* err) const {
    std::string s = "{\n";
    s += "  \"build_id\": " + json::quote(p.build_id) + ",\n";
    s += "  \"game_version\": " + json::quote(p.game_version) + ",\n";
    s += "  \"abi\": " + json::quote(p.abi) + ",\n";
    s += "  \"text_size\": " + json::quote(hex16(p.text_size)) + ",\n";
    s += "  \"notes\": " + json::quote(p.notes) + ",\n";
    s += "  \"modules\": [\n";
    for (size_t i = 0; i < p.modules.size(); ++i) {
        const auto& m = p.modules[i];
        s += "    {\"name\": " + json::quote(m.name) +
             ", \"base_va\": " + json::quote(hex16(m.base_va)) +
             ", \"rva_begin\": " + json::quote(hex16(m.rva_begin)) +
             ", \"rva_end\": " + json::quote(hex16(m.rva_end)) +
             ", \"sha\": " + json::quote(m.sha) + "}";
        if (i + 1 < p.modules.size()) s += ",";
        s += "\n";
    }
    s += "  ]\n}\n";
    return write_text_file(build_profile_path(p.build_id), s, err);
}

std::optional<BuildProfile> ResearchWorkspace::load_build_profile(const std::string& build_id,
                                                                 std::string* err) const {
    std::string text;
    if (!read_text_file(build_profile_path(build_id), &text)) {
        if (err) *err = "no build profile for '" + build_id + "'";
        return std::nullopt;
    }
    json::Obj root;
    if (!json::parse_object(text, &root)) {
        if (err) *err = "build profile '" + build_id + "' is malformed";
        return std::nullopt;
    }
    BuildProfile p;
    std::string s;
    if (root.get_string("build_id", &s))     p.build_id = s;
    if (root.get_string("game_version", &s)) p.game_version = s;
    if (root.get_string("abi", &s))          p.abi = s;
    if (root.get_string("text_size", &s))    p.text_size = std::strtoull(s.c_str(), nullptr, 0);
    if (root.get_string("notes", &s))        p.notes = s;
    for (const auto& mv : root.get_array("modules")) {
        BuildProfileModule m;
        std::string t;
        if (mv.get_string("name", &t))      m.name = t;
        if (mv.get_string("base_va", &t))   m.base_va = std::strtoull(t.c_str(), nullptr, 0);
        if (mv.get_string("rva_begin", &t)) m.rva_begin = std::strtoull(t.c_str(), nullptr, 0);
        if (mv.get_string("rva_end", &t))   m.rva_end = std::strtoull(t.c_str(), nullptr, 0);
        if (mv.get_string("sha", &t))       m.sha = t;
        p.modules.push_back(m);
    }
    return p;
}

// ── sessions/ ──────────────────────────────────────────────────────────────
bool ResearchWorkspace::begin_session(uint64_t session_seed, std::string* err) {
    if (!m_open) { if (err) *err = "workspace not open"; return false; }
    m_session_seed = session_seed;
    m_checkpoints  = 0;
    m_session_path = path_of((fs::path("sessions") / (now_stamp() + ".session")).string());
    m_session_open = true;

    std::string s = "# swordfare session\n";
    s += "status\trunning\n";
    s += "seed\t" + hex16(session_seed) + "\n";
    s += "started\t" + std::to_string(now_seconds()) + "\n";
    s += "# rows: kind\truntime_va\tvar_id\tname\ttype\tvalue\n";
    return write_text_file(m_session_path, s, err);
}

bool ResearchWorkspace::checkpoint(const std::vector<AddressEntry>& rows,
                                   const std::vector<LiveObject>& objects,
                                   std::string* err) {
    if (!m_open || !m_session_open) {
        if (err) *err = "no open session";
        return false;
    }
    ++m_checkpoints;
    std::string s = "# swordfare session\n";
    s += "status\trunning\n";
    s += "seed\t" + hex16(m_session_seed) + "\n";
    s += "started\t" + std::to_string(now_seconds()) + "\n";
    s += "checkpoint\t" + std::to_string(m_checkpoints) + "\n";
    s += "# rows: kind\truntime_va\tvar_id\tname\ttype\tvalue\n";
    // The rows carry the runtime address (transient) AND the identity (stable).
    // Only the first is expected to change after a restart.
    for (const auto& r : rows) {
        s += "row\t" + hex16(r.runtime_va) + "\t" + hex16(r.identity.var_id) + "\t" +
             r.label() + "\t" + value_type_name(r.type) + "\t" +
             (r.user_label.empty() ? r.display_name : r.user_label) + "\n";
    }
    for (const auto& o : objects) {
        s += "object\t" + hex16(o.runtime_va) + "\t" + hex16(o.identity.var_id) + "\t" +
             o.display_name() + "\t-\t" + (o.moved() ? "moved" : "stable") + "\n";
    }
    return write_text_file(m_session_path, s, err);
}

bool ResearchWorkspace::end_session(const std::string& reason, std::string* err) {
    if (!m_open || !m_session_open) {
        if (err) *err = "no open session";
        return false;
    }
    std::string s;
    read_text_file(m_session_path, &s);
    // Replace the status line in place, preserving the rest of the record.
    const std::string from = "status\trunning\n";
    const size_t pos = s.find(from);
    if (pos != std::string::npos)
        s.replace(pos, from.size(), "status\tclosed\t" + reason + "\n");
    m_session_open = false;
    return write_text_file(m_session_path, s, err);
}

std::string ResearchWorkspace::latest_session_file() const {
    const auto files = list_dir(path_of("sessions"));
    std::string best;
    for (const auto& f : files) {
        if (f.size() < 8) continue;
        if (f.compare(f.size() - 8, 8, ".session") != 0) continue;
        if (f > best) best = f;    // timestamped names sort chronologically
    }
    return best;
}

bool ResearchWorkspace::crash_recovered() const {
    const std::string f = latest_session_file();
    if (f.empty()) return false;
    std::string s;
    if (!read_text_file(f, &s)) return false;
    return s.find("status\trunning") != std::string::npos;
}

std::string ResearchWorkspace::latest_session_summary() const {
    const std::string f = latest_session_file();
    if (f.empty()) return "no previous session found.";
    std::string s;
    if (!read_text_file(f, &s)) return "cannot read " + f;

    std::string out = "previous session: " + fs::path(f).filename().string();
    if (s.find("status\trunning") != std::string::npos)
        out += "  (did NOT shut down cleanly — recovering)";
    else
        out += "  (closed cleanly)";
    out += "\n";

    size_t rows = 0;
    std::string sample;
    size_t start = 0;
    while (start < s.size()) {
        const size_t nl = s.find('\n', start);
        if (nl == std::string::npos) break;
        const std::string line = s.substr(start, nl - start);
        start = nl + 1;
        if (line.rfind("row\t", 0) == 0) {
            ++rows;
            if (rows <= 8) {
                const auto f2 = split_tab(line);
                if (f2.size() >= 5) {
                    sample += "    " + f2[3] + "  [" + f2[2] + "]  was at " + f2[1];
                    if (f2.size() >= 6) sample += "  = " + f2[5];
                    sample += "\n";
                }
            }
        }
    }
    out += "  " + std::to_string(rows) + " tracked address(es); identities are unchanged\n";
    out += sample;
    return out;
}

std::string ResearchWorkspace::summary() const {
    char b[256];
    std::snprintf(b, sizeof(b),
                  "%s: %zu identity(ies), %zu mapping(s), %zu observation(s), session %s",
                  m_root.c_str(), m_symbols.size(), m_mappings.size(), m_observations.size(),
                  m_session_open ? "open" : "closed");
    return b;
}

} // namespace swordfare::research
