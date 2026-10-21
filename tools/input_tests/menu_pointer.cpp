#include "Hooks/NativeMenuPointer.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
void Check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main()try {
    using namespace cvr::input;
    XrPosef aim{{0,0,0,1},{0,0,0}},panel{{0,0,0,1},{0,0,-1.5f}};
    auto h=IntersectNativeMenu(aim,panel,2,2);
    Check(h.valid&&h.u==.5f&&h.v==.5f,"center ray missed");
    aim.position={1,1,0};h=IntersectNativeMenu(aim,panel,2,2);
    Check(h.valid&&h.u==1&&h.v==0,"top-right UV mapping incorrect");
    Check(MenuClientPixel(h.u,1440)==1439&&MenuClientPixel(h.v,1600)==0,"client edge out of range");
    aim.position.x=1.01f;Check(!IntersectNativeMenu(aim,panel,2,2).valid,"off-panel ray admitted");
    aim.position={0,0,-2};Check(!IntersectNativeMenu(aim,panel,2,2).valid,"rear ray admitted");
    aim.position={0,0,0};aim.orientation={0,1,0,0};Check(!IntersectNativeMenu(aim,panel,2,2).valid,"away ray admitted");
    aim.orientation={0,0,0,0};Check(!IntersectNativeMenu(aim,panel,2,2).valid,"invalid quaternion admitted");
    aim.orientation={0,0,0,1};aim.position.x=std::numeric_limits<float>::quiet_NaN();
    Check(!IntersectNativeMenu(aim,panel,2,2).valid,"nonfinite origin admitted");
    // Rotate and translate both ray and panel: the hit must retain its UV.
    const XrQuaternionf yaw{0,.70710678f,0,.70710678f};
    aim={yaw,{3,2,1}};panel={yaw,{1.5f,2,1}};
    h=IntersectNativeMenu(aim,panel,2,2);Check(h.valid&&std::fabs(h.u-.5f)<.0001f,"rotated panel mismatch");

    MenuPointerButton button;
    auto step=[&](float trigger,bool hit=true,bool focus=true,bool fresh=true,bool available=true,uint64_t origin=1,bool enabled=true){
        return button.Update(enabled,focus,fresh,hit,trigger,available,origin);};
    Check(!step(1),"held entry pressed");step(0);Check(step(1),"fresh squeeze did not press");
    for(int i=0;i<1000;++i)Check(step(1),"held squeeze ended drag");
    Check(step(.4f),"trigger jitter ended drag");
    Check(!step(.35f),"release threshold retained drag");
    Check(!step(1),"partial release rearmed drag");
    Check(!step(0) && step(1),"full release failed to rearm drag");
    Check(!step(1,false) && !step(1),"ray miss resumed held drag");
    step(0);Check(!step(1,false)&&!step(1),"off-panel press clicked when returning");
    step(0);Check(!step(1,true,false)&&!step(1),"focus return clicked held trigger");
    step(0);Check(!step(1,true,true,false)&&!step(1),"stale pose return clicked held trigger");
    step(0);Check(!step(1,true,true,true,false)&&!step(1),"inactive action return clicked held trigger");
    step(0);Check(!step(1,true,true,true,true,2),"recenter clicked held trigger");
    step(0,true,true,true,true,2);Check(step(1,true,true,true,true,2),"recenter recovery failed");
    step(0);Check(!step(1,true,true,true,true,1,false)&&!step(1),"overlay close leaked press");
    step(0);Check(step(1),"drag setup failed");
    Check(!step(std::numeric_limits<float>::quiet_NaN()) && !step(1),"invalid trigger resumed held drag");
    step(0);Check(!step(1.1f) && !step(1),"out-of-range trigger rearmed drag");
    std::cout<<"native menu pointer: PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
