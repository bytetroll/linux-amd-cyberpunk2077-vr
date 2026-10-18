#include "Runtimes/XrLayerDiagnostics.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>

void Check(bool ok, const char* why) {
    if (!ok) throw std::runtime_error(why);
}

int main() try {
    using namespace cvr::xrdiag;
    XrCompositionLayerProjectionView views[2]{};
    for (auto& view : views) {
        view.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
        view.pose.orientation.w = 1;
        view.fov = {-.8f, .8f, .9f, -.9f};
        view.subImage.imageRect.extent = {1440, 1600};
    }
    views[0].pose.position.x = -.033f;
    views[1].pose.position.x = .033f;
    XrCompositionLayerProjection projection{};
    projection.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
    projection.viewCount = 2;
    projection.views = views;

    XrCompositionLayerQuad quads[3]{};
    for (auto& quad : quads) {
        quad.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
        quad.pose.orientation.w = 1;
        quad.pose.position.z = -1.5f;
        quad.subImage.imageRect.extent = {1024, 1280};
    }
    quads[0].eyeVisibility = XR_EYE_VISIBILITY_LEFT;
    quads[1].eyeVisibility = XR_EYE_VISIBILITY_RIGHT;
    quads[2].eyeVisibility = XR_EYE_VISIBILITY_BOTH;
    // The regression: healthy base views do not imply healthy supplemental HUD.
    quads[1].pose.orientation.w = 0;

    XrCompositionLayerBaseHeader unknown{};
    unknown.type = XR_TYPE_UNKNOWN;
    const XrCompositionLayerBaseHeader* layers[] = {
        reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projection),
        reinterpret_cast<const XrCompositionLayerBaseHeader*>(&quads[0]),
        reinterpret_cast<const XrCompositionLayerBaseHeader*>(&quads[1]),
        reinterpret_cast<const XrCompositionLayerBaseHeader*>(&quads[2]),
        nullptr, &unknown
    };
    XrFrameEndInfo frame{};
    frame.type = XR_TYPE_FRAME_END_INFO;
    frame.layerCount = 6;
    frame.layers = layers;
    std::vector<LayerDiagnostic> entries;
    const auto collect = [&](const LayerDiagnostic& entry) { entries.push_back(entry); };
    Check(VisitLayers(frame, collect) == 7 && entries.size() == 7, "mixed layer list was not fully visited");
    for (int i = 0; i < 2; ++i) {
        const auto& entry = entries[i];
        Check(entry.layerIndex == 0 && entry.viewIndex == i, "projection view identity lost");
        Check(entry.type == XR_TYPE_COMPOSITION_LAYER_PROJECTION && entry.layer == layers[0], "projection layer identity lost");
        Check(entry.pose == &views[i].pose && entry.fov == &views[i].fov && entry.subImage == &views[i].subImage,
              "projection diagnostic does not reference submitted data");
        Check(cvr::pose::Classify(ToRigid(*entry.pose)) == cvr::pose::Verdict::Ok, "healthy projection misclassified");
    }
    unsigned invalid = 0;
    for (int i = 0; i < 3; ++i) {
        const auto& entry = entries[i + 2];
        Check(entry.layerIndex == static_cast<uint32_t>(i + 1) && entry.viewIndex == -1, "quad layer identity lost");
        Check(entry.type == XR_TYPE_COMPOSITION_LAYER_QUAD && entry.layer == layers[i + 1], "quad type or pointer lost");
        Check(entry.pose == &quads[i].pose && entry.subImage == &quads[i].subImage && !entry.fov,
              "quad diagnostic does not reference submitted data");
        Check(entry.eyeVisibility == quads[i].eyeVisibility, "quad eye visibility lost");
        if (cvr::pose::Classify(ToRigid(*entry.pose)) != cvr::pose::Verdict::Ok) {
            ++invalid;
            Check(entry.layerIndex == 2 && cvr::pose::Classify(ToRigid(*entry.pose)) == cvr::pose::Verdict::ZeroQuat,
                  "invalid supplemental quad was not identified");
        }
    }
    Check(invalid == 1 && quads[1].pose.orientation.w == 0, "diagnostics missed or changed invalid supplemental pose");
    Check(entries[5].layerIndex == 4 && !entries[5].layer && !entries[5].pose, "null layer disappeared");
    Check(entries[6].layerIndex == 5 && entries[6].layer == &unknown && !entries[6].pose,
          "unsupported layer disappeared");

    entries.clear();
    frame.layerCount = 0;
    Check(VisitLayers(frame, collect) == 0 && entries.empty(), "empty list invoked callback");
    frame.layerCount = 6;
    frame.layers = nullptr;
    Check(VisitLayers(frame, collect) == 0 && entries.empty(), "null layer array invoked callback");

    frame.layers = layers;
    frame.layerCount = 1;
    projection.views = nullptr;
    Check(VisitLayers(frame, collect) == 1 && entries.size() == 1 && !entries[0].pose && entries[0].layer == layers[0],
          "projection with missing views was not safely reported");
    entries.clear();
    projection.views = views;
    projection.viewCount = 0;
    Check(VisitLayers(frame, collect) == 1 && entries.size() == 1 && entries[0].viewIndex == -1 && !entries[0].pose,
          "projection with no views was not safely reported");
    std::cout << "layerdiagnostics PASS\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << "layerdiagnostics FAIL: " << error.what() << '\n';
    return 1;
}
