#include "tools/template_sources.h"
#include "tools/scene_loader.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <system_error>
#include <unordered_set>

namespace fs = std::filesystem;

namespace av {

namespace {

std::string lowercase(const std::string& value) {
    std::string out = value;
    for (char& ch : out)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return out;
}

// Bounded recursive scan of one root: yields regular files with one of the
// given lowercase extensions, skipping hidden/build/cache directories.
void walk_for_extensions(const fs::path& root,
                         const std::unordered_set<std::string>& extensions,
                         int max_depth,
                         const std::function<void(const fs::path&)>& on_file) {
    std::error_code ec;
    if (!fs::is_directory(root, ec)) return;
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
         it != end && !ec; it.increment(ec)) {
        if (it.depth() > max_depth) { it.disable_recursion_pending(); continue; }
        if (it->is_directory(ec)) {
            const std::string fn = it->path().filename().string();
            if (!fn.empty() && (fn[0] == '.' || fn == "build" || fn == "bin" ||
                                fn == "CMakeFiles" || fn == "node_modules")) {
                it.disable_recursion_pending();
                continue;
            }
            continue;
        }
        if (!it->is_regular_file(ec)) continue;
        if (extensions.count(lowercase(it->path().extension().string())))
            on_file(it->path());
    }
}

} // namespace

std::vector<TemplateSourceEntry> scan_template_sources(
    const std::vector<std::string>& roots, const SceneData& scene) {
    std::vector<TemplateSourceEntry> result;
    std::unordered_set<std::string> seen;

    auto add_entry = [&](TemplateSourceEntry e) {
        if (e.name.empty() || !seen.insert(lowercase(e.name)).second) return;
        result.push_back(std::move(e));
    };

    // 1. Scene-embedded templates first (they resolve without extra files).
    for (const auto& info : scene_list_templates(scene)) {
        add_entry(TemplateSourceEntry{TemplateSourceEntry::Template,
                                      info.name, std::string(), info.scaling});
    }

    // Root dedupe: the editor passes every imported-library directory + home
    // asset dirs, and a scene's 48 libraries typically all live under one big
    // assets tree. Walking each overlapping root separately re-read every .scl
    // ~48 times (≈1.3 s freeze per template refresh). Canonicalize, drop exact
    // duplicates and roots that are inside an already-scanned root.
    std::vector<fs::path> scan_roots;
    for (const auto& root : roots) {
        if (root.empty()) continue;
        std::error_code ec;
        fs::path canon = fs::weakly_canonical(root, ec);
        if (ec || canon.empty()) canon = fs::path(root);
        const std::string canon_str = canon.string();
        bool subsumed = false;
        for (const auto& have : scan_roots) {
            if (canon_str == have.string()) { subsumed = true; break; }
            const std::string h = have.string();
            if (canon_str.size() > h.size() && canon_str.compare(0, h.size(), h) == 0 &&
                canon_str[h.size()] == '/') { subsumed = true; break; }
        }
        if (!subsumed) scan_roots.push_back(canon);
    }

    // 2. .scl template libraries under the (deduped) roots.
    for (const auto& root : scan_roots) {
        walk_for_extensions(root, {".scl"}, 4, [&](const fs::path& scl_path) {
            std::error_code ec;
            if (!fs::is_regular_file(scl_path, ec)) return;
            std::ifstream in(scl_path, std::ios::binary);
            if (!in) return;
            std::string bytes((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
            if (bytes.empty()) return;
            for (const auto& entry : scl_load_templates(bytes)) {
                add_entry(TemplateSourceEntry{TemplateSourceEntry::Template,
                                              entry.name, scl_path.string(),
                                              entry.scaling});
            }
        });
    }

    // 3. .pod model assets under the (deduped) roots.
    for (const auto& root : scan_roots) {
        walk_for_extensions(root, {".pod"}, 4, [&](const fs::path& pod_path) {
            TemplateSourceEntry e;
            e.kind = TemplateSourceEntry::Model;
            e.source_path = pod_path.string();
            e.name = pod_path.stem().string();   // ModelComponent.Name = pod stem
            e.scaling = 1.0f;
            add_entry(std::move(e));
        });
    }

    std::sort(result.begin(), result.end(),
              [](const TemplateSourceEntry& a, const TemplateSourceEntry& b) {
                  if (a.kind != b.kind) return a.kind < b.kind;
                  return lowercase(a.name) < lowercase(b.name);
              });
    return result;
}

} // namespace av