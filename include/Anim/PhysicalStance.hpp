#pragma once
#include "Anim/BodyBend.hpp"

namespace cvr::body {
// Shared pose slot 90 is a neck pivot measured from the neutral EYE height.
// The optical centre sits 8 cm above the neck, so upright slot 90 is -.08 m.
// Convert to displacement from the neutral NECK before applying a deadzone.
inline constexpr float NeutralOpticalHeight=.08f;
inline float RelativeNeckHeight(float neckPivotY) {
    return neckPivotY+NeutralOpticalHeight;
}
inline float PhysicalSquatDrop(float neckPivotY,float deadzone,float bendAngle) {
    if(!std::isfinite(neckPivotY) || !std::isfinite(deadzone) || !std::isfinite(bendAngle))return 0;
    const float neckY=RelativeNeckHeight(neckPivotY),hingeDrop=HingeDrop(bendAngle);
    const float ordinary=-neckY-std::max(0.0f,deadzone)-hingeDrop;
    // A confirmed floor reach keeps its existing 2 cm knee-flex threshold.
    const float reach=-neckY-.02f-hingeDrop;
    const float weight=BendWeight(bendAngle);
    return std::clamp(ordinary+(reach-ordinary)*weight,0.0f,.7f);
}
}
