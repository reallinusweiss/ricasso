// pod_stamp.cpp — see pod_stamp.h.

#include "pod_stamp.h"
#include "pod_pipeline_revision.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace av {

std::string pod_pipeline_revision() {
    return std::to_string(SWORDIGO_POD_PIPELINE_REVISION);
}

std::string pod_stamp_path(const std::string& pod_path) {
    return pod_path + ".meta";
}

namespace {

// Minimal JSON scalar extraction: the sidecar is one flat object of strings,
// bools and ints that we write ourselves, so a full parser would be dead weight
// in a library the game runtime links.
std::string json_find(const std::string& text, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    size_t k = text.find(needle);
    if (k == std::string::npos) return {};
    size_t colon = text.find(':', k + needle.size());
    if (colon == std::string::npos) return {};
    size_t i = colon + 1;
    while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) ++i;
    if (i >= text.size()) return {};
    if (text[i] == '"') {
        ++i;
        std::string out;
        while (i < text.size() && text[i] != '"') {
            if (text[i] == '\\' && i + 1 < text.size()) ++i;
            out.push_back(text[i++]);
        }
        return out;
    }
    size_t e = i;
    while (e < text.size() && text[e] != ',' && text[e] != '}' && text[e] != '\n') ++e;
    std::string out = text.substr(i, e - i);
    while (!out.empty() && (out.back() == ' ' || out.back() == '\r')) out.pop_back();
    return out;
}

std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '"' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

} // namespace

bool pod_stamp_write(const std::string& pod_path, const PodStamp& stamp, std::string* err) {
    const std::string path = pod_stamp_path(pod_path);
    std::error_code ec;
    fs::path parent = fs::path(path).parent_path();
    if (!parent.empty() && !fs::exists(parent, ec)) {
        if (err) *err = "sidecar directory does not exist: " + parent.string();
        return false;
    }

    std::ostringstream js;
    js << "{\"tool\":\"swordigo-pod-converter\""
       << ",\"converter\":\"" << json_escape(stamp.converter) << "\""
       << ",\"revision\":\"" << json_escape(stamp.revision.empty() ? pod_pipeline_revision()
                                                                   : stamp.revision) << "\""
       << ",\"source\":\"" << json_escape(stamp.source) << "\""
       << ",\"rigidSkin\":" << (stamp.rigid_skin ? "true" : "false")
       << ",\"meshVertices\":" << stamp.mesh_vertices
       << "}\n";

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f.is_open()) {
        if (err) *err = "cannot write stamp: " + path;
        return false;
    }
    const std::string text = js.str();
    f.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(f);
}

PodStamp pod_stamp_read(const std::string& pod_path) {
    PodStamp stamp;
    const std::string path = pod_stamp_path(pod_path);
    std::error_code ec;
    if (!fs::exists(path, ec)) return stamp;

    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return stamp;
    std::ostringstream ss;
    ss << f.rdbuf();
    const std::string text = ss.str();
    if (text.empty()) return stamp;

    stamp.present   = true;
    stamp.revision  = json_find(text, "revision");
    stamp.source    = json_find(text, "source");
    stamp.converter = json_find(text, "converter");
    stamp.rigid_skin = json_find(text, "rigidSkin") == "true";
    const std::string verts = json_find(text, "meshVertices");
    if (!verts.empty()) {
        try { stamp.mesh_vertices = std::stoi(verts); } catch (...) { stamp.mesh_vertices = 0; }
    }
    return stamp;
}

bool pod_is_stale(const std::string& pod_path, std::string* reason) {
    const PodStamp stamp = pod_stamp_read(pod_path);
    if (!stamp.present) return false;                  // native asset, nothing to say
    const std::string here = pod_pipeline_revision();
    if (stamp.revision.empty() || stamp.revision == here) return false;

    if (reason) {
        std::ostringstream s;
        s << "written by POD pipeline revision " << stamp.revision
          << ", this build is " << here;
        if (!stamp.source.empty()) s << "; source " << stamp.source;
        *reason = s.str();
    }
    return true;
}

void pod_warn_if_stale(const std::string& pod_path) {
    // One warning per path per process: pod_load() runs on every scene load and
    // in the game runtime, and a repeated warning is noise, not information.
    static std::mutex mutex;
    static std::set<std::string> already_warned;

    std::string reason;
    if (!pod_is_stale(pod_path, &reason)) return;

    std::lock_guard<std::mutex> lock(mutex);
    if (!already_warned.insert(pod_path).second) return;

    std::fprintf(stderr,
                 "[POD] WARNING: %s is a STALE BAKE — %s.\n"
                 "[POD]          Its mesh and skeleton may live in different spaces, which\n"
                 "[POD]          tears or offsets the model under animation even though the\n"
                 "[POD]          file is valid. Re-convert it from the source model.\n",
                 pod_path.c_str(), reason.c_str());
}

} // namespace av
