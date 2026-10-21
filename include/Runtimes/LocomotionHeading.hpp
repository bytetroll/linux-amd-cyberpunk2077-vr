#pragma once
#include <cmath>
#include <cstdint>

namespace cvr::tracking {
// Heading of OpenXR -Z projected onto the floor: +Y is up, positive yaw
// turns left. Local pitch/roll must not rotate the movement axes themselves.
inline bool ProjectedForwardYaw(float x,float y,float z,float w,float* yaw) {
    const float norm=x*x+y*y+z*z+w*w;
    if(!yaw || !std::isfinite(norm) || norm<.5f || norm>1.5f)return false;
    const float horizontalX=2*(w*y+x*z)/norm;
    const float horizontalZ=1-2*(x*x+y*y)/norm;
    // Within about six degrees of vertical, small tracking errors make the
    // projected direction unstable. Keep the last usable movement heading.
    if(horizontalX*horizontalX+horizontalZ*horizontalZ<.01f)return false;
    *yaw=std::atan2(horizontalX,horizontalZ);
    return true;
}

class LocomotionHeading {
public:
    void Reset(uint64_t origin=0) { m_origin=origin;m_valid=false;m_yaw=0; }
    // Returns whether a NEW usable heading was accepted. Invalid poses retain
    // the previous heading only inside the same tracking coordinate frame.
    bool Observe(uint64_t origin,bool tracked,float x,float y,float z,float w) {
        if(origin!=m_origin)Reset(origin);
        float yaw{};
        if(!tracked || !ProjectedForwardYaw(x,y,z,w,&yaw))return false;
        m_yaw=yaw;m_valid=true;return true;
    }
    bool Valid(uint64_t origin) const { return m_valid && m_origin==origin; }
    float Value(uint64_t origin,float fallback=0) const { return Valid(origin)?m_yaw:fallback; }
private:
    uint64_t m_origin{};
    float m_yaw{};
    bool m_valid{};
};
}
