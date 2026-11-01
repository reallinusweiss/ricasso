#pragma once
// template_sources.h — where the Template Palette gets its entries (master
// TODO 2.3). Two kinds of addable sources are scanned from asset roots:
//   * Template — an ObjectLibrary (.scl) template: adding it creates a scene
//     object linked to (or materialized from) that template.
//   * Model — a .pod model asset: adding it creates an object with a Model
//     component whose Name is the pod stem, plus a LocalAABB measured from
//     the model's own bounds at add time.
// Qt-free on purpose so the palette can be unit-tested without a GUI.

#include <string>
#include <vector>

namespace av {

struct SceneData;

struct TemplateSourceEntry {
    enum Kind { Template, Model };
    Kind kind = Template;
    std::string name;          // template name (Template) or pod stem (Model)
    std::string source_path;   // .scl file path (Template), pod path (Model)
    float scaling = 1.0f;      // template scaling, 1.0 for models
};

// Scan `roots` (recursively, bounded depth) for .scl template libraries and
// .pod model assets. Scene-embedded libraries are appended too, deduped by
// name (scene-embedded entries win — they resolve without extra files).
// Returns entries sorted by kind then name.
std::vector<TemplateSourceEntry> scan_template_sources(
    const std::vector<std::string>& roots, const SceneData& scene);

} // namespace av