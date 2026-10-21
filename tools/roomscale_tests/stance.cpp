#include "Anim/PhysicalStance.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
unsigned checks{};
void Check(bool condition,const char* message) {
    ++checks;
    if(!condition) { std::cerr<<"FAIL: "<<message<<'\n';std::exit(1); }
}
void Near(float actual,float expected,const char* message) {
    Check(std::isfinite(actual) && std::abs(actual-expected)<.00001f,message);
}
// The existing OpenXR publisher rotates the .08 m up / .15 m forward optical
// lever, then subtracts it from recenter-relative head height (shared slot 90).
float PublishedNeck(float headY,float pitch) {
    const float opticalY=.08f*std::cos(pitch)+.15f*std::sin(pitch);
    return headY-opticalY;
}
float OldDrop(float neckY,float threshold,float bend) {
    const float hinge=cvr::body::HingeDrop(bend);
    const float ordinary=-neckY-threshold-hinge;
    const float reach=-(neckY+.08f)-.02f-hinge;
    return std::clamp(ordinary+(reach-ordinary)*cvr::body::BendWeight(bend),0.0f,.7f);
}
}
int main() {
    using namespace cvr::body;
    Near(RelativeNeckHeight(PublishedNeck(0,0)),0,"upright neck must have zero displacement");
    // Demonstrate the old failure at the configured 20 cm crouch boundary.
    Near(OldDrop(PublishedNeck(-.20f,0),.20f,0),.08f,"legacy fixture must expose the 8 cm datum error");
    Near(PhysicalSquatDrop(PublishedNeck(-.20f,0),.20f,0),0,"deadzone boundary must not squat");
    for(float drop:{0.0f,.02f,.10f,.13f,.19f,.20f,.21f,.30f,.80f,1.0f}) {
        Near(PhysicalSquatDrop(PublishedNeck(-drop,0),.20f,0),std::clamp(drop-.20f,0.0f,.7f),
             "physical crouch must ramp from the configured threshold");
    }
    // Pitch about a stationary neck moves the optical centre but must not add
    // squat. The same cancellation must hold when already physically crouched.
    for(int degrees=-80;degrees<=80;degrees+=10) {
        const float pitch=degrees*.01745329252f;
        const float opticalY=.08f*std::cos(pitch)+.15f*std::sin(pitch);
        const float headY=opticalY-.08f;
        Near(PhysicalSquatDrop(PublishedNeck(headY,pitch),.20f,0),0,"head pivot manufactured crouch");
        Near(PhysicalSquatDrop(PublishedNeck(headY-.30f,pitch),.20f,0),.10f,"head pivot changed true crouch");
    }
    // The confirmed floor-reach path was already normalized; keep its result.
    const float fullBend=.6f;
    for(float drop:{.2f,.4f,.6f,.9f}) {
        const float neck=PublishedNeck(-drop,0);
        Near(PhysicalSquatDrop(neck,.20f,fullBend),OldDrop(neck,.20f,fullBend),
             "full floor reach changed its existing knee-flex threshold");
    }
    Near(PhysicalSquatDrop(-.08f-HingeDrop(fullBend),.20f,fullBend),0,"hinge descent counted twice as squat");
    const float partialBend=.20f,neck=PublishedNeck(-.50f,0);
    Near(OldDrop(neck,.20f,partialBend)-PhysicalSquatDrop(neck,.20f,partialBend),
         .08f*(1-BendWeight(partialBend)),"partial bend must correct only the ordinary-crouch datum");
    Near(PhysicalSquatDrop(0,.20f,0),0,"zero neck height is valid, not an absent sample");
    Near(PhysicalSquatDrop(PublishedNeck(0,0),.20f,0),0,"standing or recentering must remove squat target");
    // No absolute user height enters the result: recenter supplies displacement.
    for(float standing:{1.5f,1.7f,1.83f,2.0f}) {
        Near(PhysicalSquatDrop(PublishedNeck(standing-standing,0),.20f,0),0,"standing height changed neutral stance");
        Near(PhysicalSquatDrop(PublishedNeck((standing-.30f)-standing,0),.20f,0),.10f,"standing height changed crouch depth");
    }
    const float bad=std::numeric_limits<float>::quiet_NaN();
    Near(PhysicalSquatDrop(bad,.20f,0),0,"invalid neck sample must fail neutral");
    Near(PhysicalSquatDrop(-.30f,bad,0),0,"invalid threshold must fail neutral");
    Near(PhysicalSquatDrop(-.30f,.20f,bad),0,"invalid bend must fail neutral");
    Near(PhysicalSquatDrop(-std::numeric_limits<float>::infinity(),.20f,0),0,"infinite neck sample must fail neutral");
    std::cout<<checks<<" physical stance checks passed\n";
}
