#pragma once
#include "Utils/XrMath.hpp"
#include <algorithm>
#include <cstdint>

namespace cvr::input {
struct MenuPointerHit { bool valid{}; float u{},v{}; };
inline bool MenuPointerPoseValid(const XrPosef& p) {
    const float v[]={p.position.x,p.position.y,p.position.z,p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w};
    for(float x:v)if(!std::isfinite(x))return false;
    const auto& q=p.orientation;
    const float n=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
    return std::fabs(n-1.f)<.002f;
}
inline MenuPointerHit IntersectNativeMenu(const XrPosef& aim,const XrPosef& panel,float width,float height) {
    if(!MenuPointerPoseValid(aim) || !MenuPointerPoseValid(panel) ||
       !std::isfinite(width) || !std::isfinite(height) || width<=0 || height<=0)return {};
    const auto inv=ConjugateQuat(panel.orientation);
    const auto origin=RotateVector(inv,{aim.position.x-panel.position.x,aim.position.y-panel.position.y,aim.position.z-panel.position.z});
    const auto direction=RotateVector(inv,RotateVector(aim.orientation,{0,0,-1}));
    if(direction.z>=-.0001f || origin.z<=0)return {};
    const float distance=-origin.z/direction.z;
    if(!std::isfinite(distance) || distance<=0 || distance>10)return {};
    const float u=(origin.x+direction.x*distance)/width+.5f;
    const float v=.5f-(origin.y+direction.y*distance)/height;
    if(!std::isfinite(u) || !std::isfinite(v) || u<0 || u>1 || v<0 || v>1)return {};
    return {true,u,v};
}
inline int MenuClientPixel(float value,int extent) {
    return extent>0 && std::isfinite(value) ? std::clamp(int(value*extent),0,extent-1) : 0;
}
// Returns the desired mouse-button state, including throughout a drag. Any
// interruption spends the squeeze; another press requires a released trigger.
class MenuPointerButton {
    bool armed{},down{},dragging{};
    uint64_t origin{};
public:
    bool Update(bool enabled,bool foreground,bool fresh,bool hit,float trigger,bool available,uint64_t nextOrigin) {
        if(!enabled || !foreground || !fresh || !available || !std::isfinite(trigger) || trigger<0 || trigger>1) {
            armed=down=dragging=false;origin=0;return false;
        }
        if(origin!=nextOrigin){armed=down=dragging=false;origin=nextOrigin;}
        if(trigger<=.3f){armed=true;down=dragging=false;return false;}
        const bool pressed=down ? trigger>.35f : trigger>=.65f;
        if(pressed && !down){dragging=armed && hit;armed=false;}
        if(!pressed || !hit)dragging=false;
        down=pressed;
        return dragging;
    }
};
struct NativeMenuPointerFrame {
    bool active{},aimValid{};
    XrPosef aim{{0,0,0,1},{}},panel{{0,0,0,1},{}};
    XrExtent2Df size{};
    uint64_t stampMs{},origin{};
};
void PublishNativeMenuPointer(const NativeMenuPointerFrame& frame);
// Returns true when the pointer owns the native-menu trigger, even if the ray
// misses. Physical A and stick navigation remain available as a fallback.
bool UpdateNativeMenuPointer(bool nativeMenu,bool overlayCapture,float trigger,bool triggerAvailable);
// Existing worker's watchdog and shutdown cleanup. Releases only a successfully
// inserted press; never moves the cursor or starts a new press.
void MaintainNativeMenuPointer(bool cancel=false);
} // namespace cvr::input
