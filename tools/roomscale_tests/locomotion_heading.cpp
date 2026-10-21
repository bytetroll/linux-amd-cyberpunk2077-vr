#include "Runtimes/LocomotionHeading.hpp"
#include "Runtimes/RoomscaleMovement.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
constexpr float pi=3.14159265358979323846f;
constexpr float rad=pi/180;
unsigned checks{};
void Check(bool value,const char* message) {
    ++checks;
    if(!value) { std::cerr<<"FAIL: "<<message<<'\n';std::exit(1); }
}
void Near(float actual,float expected,const char* message) {
    Check(std::isfinite(actual) && std::abs(std::remainder(actual-expected,2*pi))<.00001f,message);
}
struct Q { float x,y,z,w; };
Q Mul(Q a,Q b) {
    return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
            a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
            a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
            a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};
}
Q Pose(float yaw,float pitch,float roll) {
    const Q y{0,std::sin(yaw*rad/2),0,std::cos(yaw*rad/2)};
    const Q p{std::sin(pitch*rad/2),0,0,std::cos(pitch*rad/2)};
    const Q r{0,0,std::sin(roll*rad/2),std::cos(roll*rad/2)};
    return Mul(Mul(y,p),r);
}
bool Observe(cvr::tracking::LocomotionHeading& h,uint64_t origin,bool tracked,Q q) {
    return h.Observe(origin,tracked,q.x,q.y,q.z,q.w);
}
}

int main() {
    using cvr::tracking::ProjectedForwardYaw;
    // Looking up/down and rolling a headset/controller must preserve its
    // direction on the floor. Test signed, cardinal and diagonal headings.
    for(float yaw:{-179.f,-135.f,-90.f,-45.f,0.f,45.f,90.f,135.f,179.f})
        for(float pitch:{-80.f,-45.f,0.f,45.f,80.f})
            for(float roll:{-179.f,-120.f,-45.f,0.f,45.f,120.f,179.f}) {
                const auto q=Pose(yaw,pitch,roll);
                float heading{};
                Check(ProjectedForwardYaw(q.x,q.y,q.z,q.w,&heading),"usable floor projection rejected");
                Near(heading,yaw*rad,"pitch/roll steers the stick away from the view");
                const auto forward=cvr::roomscale::Rotate({0,1},heading);
                Near(forward.x,-std::sin(yaw*rad),"forward stick has wrong horizontal sign");
                Near(forward.y,std::cos(yaw*rad),"forward stick has wrong forward component");
                const float scale=.94f;
                Check(ProjectedForwardYaw(q.x*scale,q.y*scale,q.z*scale,q.w*scale,&heading),
                      "slightly non-unit runtime quaternion rejected");
                Near(heading,yaw*rad,"quaternion normalization changed heading");
                Check(ProjectedForwardYaw(-q.x,-q.y,-q.z,-q.w,&heading),"opposite quaternion sign rejected");
                Near(heading,yaw*rad,"equivalent negative quaternion changed heading");
            }

    // The old denominator makes a controller at 45deg yaw / 120deg local roll
    // walk at about 116.6deg. Keep a concrete regression beyond flat yaw poses.
    const auto rolled=Pose(45,0,120);
    const float oldYaw=std::atan2(2*(rolled.w*rolled.y+rolled.x*rolled.z),
                                1-2*(rolled.y*rolled.y+rolled.z*rolled.z));
    Check(std::abs(oldYaw-45*rad)>60*rad,"regression fixture no longer exposes the old basis");

    cvr::tracking::LocomotionHeading heading;
    Near(heading.Value(1,-.4f),-.4f,"uninitialized heading ignores supplied fallback");
    Check(Observe(heading,1,true,Pose(45,30,45)),"first usable heading rejected");
    Near(heading.Value(1),45*rad,"first heading not published");
    for(float pitch:{-90.f,-89.f,-85.f,85.f,89.f,90.f}) {
        Check(!Observe(heading,1,true,Pose(-130,pitch,75)),"near-vertical projection accepted");
        Near(heading.Value(1),45*rad,"near-vertical pose turned locomotion");
    }
    Check(!Observe(heading,1,false,Pose(-90,0,0)),"invalid tracking accepted");
    Near(heading.Value(1),45*rad,"tracking dropout discarded the last heading");
    for(auto q:{Q{0,0,0,0},Q{0,0,0,5},Q{NAN,0,0,1},Q{0,INFINITY,0,1}}) {
        Check(!Observe(heading,1,true,q),"invalid quaternion accepted");
        Near(heading.Value(1),45*rad,"invalid quaternion contaminated the heading");
    }
    Check(Observe(heading,1,true,Pose(-179,20,75)),"tracking recovery rejected");
    Near(heading.Value(1),-179*rad,"tracking recovery retained stale direction");
    Check(Observe(heading,1,true,Pose(179,-20,-75)),"heading across wrap rejected");
    Near(heading.Value(1),179*rad,"heading wrap altered direction");

    Near(heading.Value(2,-.4f),-.4f,"stale coordinate frame leaked before the new sample");
    Check(!Observe(heading,2,true,Pose(80,90,0)),"vertical first sample accepted after recenter");
    Check(!heading.Valid(2),"old tracking frame survived a recenter");
    Near(heading.Value(2,-.4f),-.4f,"new origin without heading ignored fallback");
    Check(Observe(heading,2,true,Pose(-35,20,15)),"first usable sample after recenter rejected");
    Near(heading.Value(2),-35*rad,"recenter blended incompatible coordinate frames");
    heading.Reset(2); // input-context/shutdown reset
    Check(!heading.Valid(2),"context reset retained controller direction");
    Near(heading.Value(2,.6f),.6f,"context reset did not use HMD fallback");
    std::cout<<checks<<" locomotion heading checks passed\n";
}
