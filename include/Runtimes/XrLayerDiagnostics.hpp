#pragma once
#include <cstdint>
#include <openxr/openxr.h>
#include "Utils/PoseValidity.hpp"

namespace cvr::xrdiag {

struct LayerDiagnostic {
    uint32_t layerIndex{};
    int32_t viewIndex{-1}; // Projection view index; -1 for a layer without a view.
    XrStructureType type{XR_TYPE_UNKNOWN};
    const XrCompositionLayerBaseHeader* layer{};
    XrSpace space{XR_NULL_HANDLE};
    XrEyeVisibility eyeVisibility{XR_EYE_VISIBILITY_BOTH};
    const XrPosef* pose{};
    const XrFovf* fov{};
    const XrSwapchainSubImage* subImage{};
};

inline cvr::pose::Rigid ToRigid(const XrPosef& pose) {
    return {pose.orientation.x, pose.orientation.y, pose.orientation.z, pose.orientation.w,
            pose.position.x, pose.position.y, pose.position.z};
}

// Read-only traversal of the exact layer list given to xrEndFrame. Unknown/null
// layers and projections without a view array still produce an entry, so a
// diagnostic cannot silently hide them. Borrowed pointers live only as long as
// the caller's frame structures. This does not validate pointer accessibility.
template<class Visitor>
uint32_t VisitLayers(const XrFrameEndInfo& frame, Visitor&& visitor) {
    if (!frame.layers) return 0;
    uint32_t entries = 0;
    for (uint32_t index = 0; index < frame.layerCount; ++index) {
        LayerDiagnostic entry{};
        entry.layerIndex = index;
        entry.layer = frame.layers[index];
        if (entry.layer) {
            entry.type = entry.layer->type;
            entry.space = entry.layer->space;
            if (entry.type == XR_TYPE_COMPOSITION_LAYER_PROJECTION) {
                const auto& projection = *reinterpret_cast<const XrCompositionLayerProjection*>(entry.layer);
                if (projection.views && projection.viewCount) {
                    for (uint32_t view = 0; view < projection.viewCount; ++view) {
                        const auto& source = projection.views[view];
                        entry.viewIndex = static_cast<int32_t>(view);
                        entry.pose = &source.pose;
                        entry.fov = &source.fov;
                        entry.subImage = &source.subImage;
                        visitor(entry);
                        ++entries;
                    }
                    continue;
                }
            } else if (entry.type == XR_TYPE_COMPOSITION_LAYER_QUAD) {
                const auto& quad = *reinterpret_cast<const XrCompositionLayerQuad*>(entry.layer);
                entry.eyeVisibility = quad.eyeVisibility;
                entry.pose = &quad.pose;
                entry.subImage = &quad.subImage;
            }
        }
        visitor(entry);
        ++entries;
    }
    return entries;
}

} // namespace cvr::xrdiag
