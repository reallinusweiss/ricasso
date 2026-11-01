// render_model.cpp — PodAnimSampler: AnimSampler over the shipping POD helpers.
//
// Every method forwards to the existing, verified POD animation/skinning code
// so switching the viewer to AnimSampler* is a no-behaviour-change refactor.
// A GltfAnimSampler (sampling tg3 data directly) can be added later behind the
// same interface to render glTF without the POD round-trip.

#include "render_model.h"
#include "pod_loader.h" // PODModel, get_node_matrix, skin_mesh

namespace av {

int PodAnimSampler::node_count() const {
    return static_cast<int>(model_.nodes.size());
}

int PodAnimSampler::mesh_node_count() const {
    return model_.num_mesh_nodes;
}

int PodAnimSampler::frame_count() const {
    return model_.num_frames;
}

void PodAnimSampler::node_world_matrix(int node, float frame, float out16[16]) const {
    // get_node_matrix already handles frame blending, matrix vs T/R/S nodes,
    // and parent chains (see pod_loader.cpp).
    get_node_matrix(model_, node, frame, out16);
}

bool PodAnimSampler::skin_node(int node, float frame,
                               std::vector<float>& positions,
                               std::vector<float>& normals) const {
    return skin_mesh(model_, node, frame, positions, normals);
}

} // namespace av
