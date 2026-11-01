#pragma once
// pod_stamp.h — provenance for converted .POD files, kept OUT of the POD bytes.
//
// Why a sidecar and not a header field: the POD format's only version slot is
// the PowerVR "AB.POD.2.0" string, which the game engine parses. Writing our
// own revision into it would change a file the engine reads; writing a new tag
// risks a loader that rejects unknown tags. A `<file>.POD.meta` JSON sidecar
// carries the provenance with exactly zero risk to the game, and native
// Swordigo assets simply have no sidecar and are never warned about.
//
// What it buys: `pod_load()` can tell you that the file in front of it was
// written by a different pipeline revision than the one running, which is the
// only reliable signal that a model may be a stale bake. See
// pod_pipeline_revision.h and the interconversion report §8b.4.

#include <string>

namespace av {

// Revision of the POD import/convert pipeline this build implements.
// See pod_pipeline_revision.h.
std::string pod_pipeline_revision();

// Path of the sidecar that belongs to a .POD.
std::string pod_stamp_path(const std::string& pod_path);

struct PodStamp {
    bool        present    = false;
    std::string revision;              // pipeline revision that wrote the POD
    std::string source;                // source model the POD was converted from
    std::string converter;             // "glb2pod" / "fbx2pod" / "obj2pod"
    bool        rigid_skin = false;
    int         mesh_vertices = 0;     // size of the bake, for a sanity check
};

// Write the sidecar next to `pod_path`. Failure is never fatal to a conversion
// (the POD itself is already correct); returns false with `err` set.
bool pod_stamp_write(const std::string& pod_path, const PodStamp& stamp,
                     std::string* err = nullptr);

// Read the sidecar. `present` is false when there is none (native assets).
PodStamp pod_stamp_read(const std::string& pod_path);

// True when the POD has a sidecar whose revision differs from this build's.
// `reason` gets a human-readable explanation naming both revisions.
bool pod_is_stale(const std::string& pod_path, std::string* reason = nullptr);

// One-line warning printed at most once per path per process. Called by
// pod_load(); safe to call from anywhere.
void pod_warn_if_stale(const std::string& pod_path);

} // namespace av
