#pragma once
// render_model.h — format-agnostic animation/skinning seam for the viewer.
//
// WHY THIS EXISTS (see docs/formats_and_schemas/pod_master/why_not_a_native_gltf_renderer.md):
// The GPU renderer (av_renderer.h) is already format-agnostic — it uploads raw
// float arrays (upload_mesh) and draws them (render_mesh/pbr_render_mesh) with
// zero POD/glTF types. The ONLY place a source format leaks into the draw loop
// is animation + skinning, which today is done through two PODModel-typed
// helpers: av::get_node_matrix() and av::skin_mesh(). That single coupling is
// the entire reason a .glb is down-converted to POD just to *view* it.
//
// AnimSampler is that seam expressed as an interface: "given a node and a
// frame, give me the node's world matrix and (if skinned) the CPU-skinned
// positions/normals." A PodAnimSampler wraps the shipping POD helpers verbatim
// (so behaviour is byte-identical to today). A future GltfAnimSampler can
// sample tg3 animation/skin data directly, letting the viewer render glTF
// WITHOUT the POD round-trip — while the POD conversion remains an explicit
// "export to game asset" step rather than a mandatory view-time step.
//
// This header is deliberately dependency-light (no OpenGL/ImGui) so both the
// viewer and headless tools can share it. Adoption is incremental: the viewer's
// draw loop can switch its get_node_matrix/skin_mesh calls to an AnimSampler*
// without any renderer change.

#include <cstddef>
#include <vector>

namespace av {

struct PODModel;

// Node/animation sampling interface. Frame is a float so callers can blend
// between integer keyframes exactly as get_node_matrix() already does.
class AnimSampler {
public:
    virtual ~AnimSampler() = default;

    // Number of animatable nodes (skeleton + mesh nodes).
    virtual int node_count() const = 0;

    // Number of mesh-bearing nodes; a node index < mesh_node_count() draws a mesh.
    virtual int mesh_node_count() const = 0;

    // Animation length in frames (0/1 for static models).
    virtual int frame_count() const = 0;

    // World matrix (column-major 4x4) of `node` at `frame`. Writes 16 floats.
    virtual void node_world_matrix(int node, float frame, float out16[16]) const = 0;

    // If `node` is a skinned mesh node, fill CPU-skinned `positions`/`normals`
    // (flat xyz) for `frame` and return true. Return false for unskinned nodes
    // (the caller then draws the mesh's static positions with node_world_matrix).
    virtual bool skin_node(int node, float frame,
                           std::vector<float>& positions,
                           std::vector<float>& normals) const = 0;
};

// AnimSampler backed by a PODModel — a thin, behaviour-preserving wrapper over
// the shipping av::get_node_matrix() / av::skin_mesh(). Holds a reference to the
// model; the model must outlive the sampler.
class PodAnimSampler final : public AnimSampler {
public:
    explicit PodAnimSampler(const PODModel& model) : model_(model) {}

    int  node_count() const override;
    int  mesh_node_count() const override;
    int  frame_count() const override;
    void node_world_matrix(int node, float frame, float out16[16]) const override;
    bool skin_node(int node, float frame,
                   std::vector<float>& positions,
                   std::vector<float>& normals) const override;

private:
    const PODModel& model_;
};

} // namespace av
