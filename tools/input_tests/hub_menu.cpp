#include "Hooks/HubMenuInput.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main()try {
    cvr::input::HubMenuPress menu;
    auto tick=[&](float force,std::uint64_t now,bool enabled=true,bool available=true){return menu.Update(enabled,available,force,now);};
    Check(!tick(1,1000),"held startup opened menu");
    Check(!tick(0,1001),"release opened menu");
    Check(!tick(.5f,1002),"light touch opened menu");
    Check(tick(.55f,1003),"press did not open menu");
    Check(tick(.3f,1100),"pressure jitter shortened pulse");
    Check(!tick(1,1123),"held press extended Start into map hold");
    for(unsigned n=1124;n<2200;++n)Check(!tick(1,n),"held thumb repeated menu");
    tick(0,2200);Check(tick(1,2201),"release did not rearm");
    Check(!tick(1,2210,false),"focus/overlay loss retained pulse");
    Check(!tick(1,2211),"held focus recovery opened menu");
    tick(0,2220);Check(tick(1,2221),"fresh press after focus recovery failed");
    Check(!tick(0,2222,true,false),"unavailable input retained pulse");
    Check(!tick(1,2223),"unavailable input manufactured release");
    tick(0,2230);Check(tick(1,2231),"fresh press after input recovery failed");
    tick(0,2240);Check(tick(1,2241),"quick repress lost active pulse");
    Check(!tick(1,2351),"quick repress extended pulse");
    tick(0,2400);Check(!tick(std::numeric_limits<float>::quiet_NaN(),2401),"NaN force accepted");
    Check(!tick(1,2402),"invalid force retained arming");
    tick(0,2410);Check(!tick(1.1f,2411),"out-of-range force accepted");
    tick(0,2420);Check(!tick(-.1f,2421),"negative force accepted");
    tick(0,2500);Check(!tick(1,2000),"clock rollback generated pulse");
    Check(!tick(1,2501),"held recovery after rollback opened menu");
    std::cout<<"hub menu input: PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
