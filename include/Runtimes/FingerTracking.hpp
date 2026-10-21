#pragma once
#include "Anim/FingerTrackingMath.hpp"
#include <mutex>
#include <vector>

namespace cvr::finger {
// Runtime calls belong to the XR thread (except startup and joined shutdown).
// The animation thread receives a copy, never handles or runtime-owned buffers.
class Tracker {
public:
    void EnableExtensions(const std::vector<XrExtensionProperties>& available,std::vector<const char*>& enabled);
    void Start(XrInstance instance,XrSystemId system,XrSession session);
    void Stop();
    void Clear();
    void Sample(XrSpace space,XrTime time,bool focused);
    bool Read(int side,Pose& pose);
private:
    bool m_enabled{},m_sourceExtension{},m_rangeExtension{};
    PFN_xrCreateHandTrackerEXT m_create{};
    PFN_xrDestroyHandTrackerEXT m_destroy{};
    PFN_xrLocateHandJointsEXT m_locate{};
    XrHandTrackerEXT m_tracker[2]{};
    bool m_reportedActive[2]{},m_reportedError[2]{};
    struct Sampled { Pose pose{};std::uint64_t stamp{}; };
    std::mutex m_mutex;
    Sampled m_sample[2]{};
};
Tracker& Tracking();
}
