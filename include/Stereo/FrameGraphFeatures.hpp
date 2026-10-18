#pragma once
#include <cstdint>
#include <mutex>

namespace cvr::stereo {
inline constexpr uint64_t ShadingRateImageFlagF1=1ull<<20; // graph feature 84
constexpr uint64_t SupportedShadingRateFlags(uint64_t flags,uint32_t nativeTileSize) {
    return nativeTileSize ? flags : flags&~ShadingRateImageFlagF1;
}

// The temporal-upscaler selection, as the scene builder reads it: graph groups 69/71/72/73 are
// DLSS/FSR2/FSR3/XeSS, and if NONE of them is set the builder emits the native TAAU/resolve
// instead -- which re-upscales at render resolution and so CROPS the view, a render-res
// viewport left sitting on a full-res target.
inline constexpr uint64_t DlssGroupFlagF1    = 1ull<<(69-64);   // 0x020
inline constexpr uint64_t UpscalerGroupMaskF1 =                 // 0x3A0
    (1ull<<(69-64))|(1ull<<(71-64))|(1ull<<(72-64))|(1ull<<(73-64));

// Give the second view MAIN's upscaler groups, whichever MAIN chose.
//
// The port used to force group 69 here, because on the machine it was written on MAIN was
// always on DLSS, making "mirror MAIN" and "force DLSS" the same instruction. They are not the
// same instruction on a Radeon, where MAIN picks FSR2, FSR3 or XeSS: the DLSS test then fails,
// nothing is forced, and the second view goes into the builder with no group at all -- taking
// the cropping native-resolve branch that the force exists to avoid.
//
// mainGroups==0 leaves the view untouched rather than clearing it: MAIN not having chosen yet
// is not evidence that it chose nothing, and a view already carrying its own group is better
// than a view pushed into the native resolve on a transient frame. Once MAIN has genuinely
// settled on no upscaler, both views sit on the native resolve together, which is consistent.
constexpr uint64_t MirrorUpscalerGroups(uint64_t viewF1,uint64_t mainGroups) {
    const uint64_t groups=mainGroups&UpscalerGroupMaskF1;
    return groups ? ((viewF1&~UpscalerGroupMaskF1)|groups) : viewF1;
}

struct FrameGraphFlags {
    uint64_t f0{},f1{};
    bool operator==(const FrameGraphFlags&) const = default;
};

class MainViewFeatureCache {
public:
    bool Observe(uintptr_t context,uintptr_t knownMain,uint64_t viewName,FrameGraphFlags flags) {
        if(!context || context!=knownMain || viewName || !(flags.f0|flags.f1))return false;
        std::lock_guard lock(mutex_);
        context_=context;flags_=flags;return true;
    }
    bool Read(uintptr_t knownMain,FrameGraphFlags& out) const {
        std::lock_guard lock(mutex_);
        if(!knownMain || context_!=knownMain)return false;
        out=flags_;return true;
    }
private:
    mutable std::mutex mutex_;
    uintptr_t context_{};
    FrameGraphFlags flags_{};
};
}
