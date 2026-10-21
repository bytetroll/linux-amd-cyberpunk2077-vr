#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <openxr/openxr.h>

namespace cvr::finger {
// Thumb has two phalanges; the other digits have three. Angles are bends in
// radians, independent of world position/orientation and controller grip value.
struct Pose { float bend[5][3]{}; };
inline constexpr std::uint64_t MaxAgeMs=150;
inline bool Fresh(std::uint64_t stamp,std::uint64_t now) {
    return stamp!=0 && now>=stamp && now-stamp<=MaxAgeMs;
}
inline bool FreeHand(int side,bool weaponEquipped,bool carriedLeft) {
    return side>=0 && side<2 && (carriedLeft ? side==1:(!weaponEquipped || side==0));
}
inline bool LocalFlexion(const float* reference,float bend,float* output) {
    if(!reference || !output || !std::isfinite(bend) || bend<0 || bend>1.919863f)return false;
    for(int i=0;i<4;++i)if(!std::isfinite(reference[i]))return false;
    const float x=reference[0],y=reference[1],z=reference[2],w=reference[3];
    const float norm=x*x+y*y+z*z+w*w,twistNorm=z*z+w*w;
    if(!std::isfinite(norm) || norm<.5f || norm>1.5f || twistNorm<1e-10f)return false;
    // Rig flexion is local -Z. Replace, rather than add to, its bind-pose Z
    // twist. The orthogonal swing (spread/roll) survives and pre-bent reference
    // fingers do not double-curl. A singular 180-degree swing falls back.
    const float half=-.5f*bend-std::atan2(z,w),s=std::sin(half),c=std::cos(half),scale=1/std::sqrt(norm);
    const float q[4]={(x*c+y*s)*scale,(y*c-x*s)*scale,(z*c+w*s)*scale,(w*c-z*s)*scale};
    for(int i=0;i<4;++i)output[i]=q[i];
    return true;
}
inline bool Bend(const XrVector3f& a,const XrVector3f& b,const XrVector3f& c,float& out) {
    const double ux=double(b.x)-a.x,uy=double(b.y)-a.y,uz=double(b.z)-a.z;
    const double vx=double(c.x)-b.x,vy=double(c.y)-b.y,vz=double(c.z)-b.z;
    const double u2=ux*ux+uy*uy+uz*uz,v2=vx*vx+vy*vy+vz*vz;
    if(!std::isfinite(u2) || !std::isfinite(v2) || u2<1e-10 || v2<1e-10)return false;
    const double dot=(ux*vx+uy*vy+uz*vz)/std::sqrt(u2*v2);
    if(!std::isfinite(dot))return false;
    // Reject collapsed joints above; limit implausible hyperflexion rather than
    // letting a noisy/inferred skeleton turn a phalanx inside the palm.
    out=std::min(float(std::acos(std::clamp(dot,-1.0,1.0))),1.91986218f);
    return true;
}
inline bool FromJoints(const XrHandJointLocationsEXT& locations,Pose& output) {
    if(!locations.isActive || locations.jointCount!=XR_HAND_JOINT_COUNT_EXT || !locations.jointLocations)return false;
    // The controller-derived skeleton legitimately clears TRACKED bits for
    // inferred joints. VALID position data is what the bend calculation uses.
    for(int i=XR_HAND_JOINT_THUMB_METACARPAL_EXT;i<XR_HAND_JOINT_COUNT_EXT;++i) {
        const auto& j=locations.jointLocations[i];
        const auto& p=j.pose.position;
        if(!(j.locationFlags&XR_SPACE_LOCATION_POSITION_VALID_BIT) ||
           !std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))return false;
    }
    Pose candidate{};
    constexpr int first[5]={XR_HAND_JOINT_THUMB_METACARPAL_EXT,XR_HAND_JOINT_INDEX_METACARPAL_EXT,
        XR_HAND_JOINT_MIDDLE_METACARPAL_EXT,XR_HAND_JOINT_RING_METACARPAL_EXT,XR_HAND_JOINT_LITTLE_METACARPAL_EXT};
    for(int finger=0;finger<5;++finger)for(int joint=0;joint<(finger==0 ? 2:3);++joint) {
        const int i=first[finger]+joint;
        if(!Bend(locations.jointLocations[i].pose.position,locations.jointLocations[i+1].pose.position,
                 locations.jointLocations[i+2].pose.position,candidate.bend[finger][joint]))return false;
    }
    output=candidate;return true;
}
}
