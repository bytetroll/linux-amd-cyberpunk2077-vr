#include "Hooks/ScannerInput.hpp"
#include <iostream>
#include <stdexcept>
#include <limits>
void Check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main() try {
    using namespace cvr::input;
    DpadScannerToggle dpad;
    auto chord=[&](bool up,bool enabled=true,bool gameplay=true,bool allowed=true,bool available=true){
        return dpad.Update(enabled,gameplay,allowed,available,up);};
    Check(!chord(true).active,"held Dpad entry opened scanner");
    Check(chord(false).consumeUp,"inactive scanner leaked VR consumable binding");
    Check(chord(true).active,"Dpad chord did not open scanner");
    for(int n=0;n<1000;++n)Check(chord(true).active,"held Dpad repeated toggle");
    Check(chord(false).active,"Dpad release closed scanner");
    Check(!chord(true).active,"second Dpad chord did not close scanner");
    chord(false);Check(chord(true).active,"Dpad reopen failed");
    auto ui=chord(true,true,false,false);
    Check(!ui.active&&!ui.consumeUp,"menu retained scanner or consumed navigation");
    Check(!chord(true).active,"held menu return opened scanner");
    chord(false);chord(true);Check(!chord(false,true,true,true,false).active,"tracking loss retained scanner");
    Check(!chord(true).active,"unknown release rearmed Dpad scanner");
    chord(false);chord(true);Check(!chord(true,false).active,"config disable retained scanner");
    Check(!chord(true).active,"config re-enable replayed held Dpad");
    chord(false);chord(true);Check(!chord(false,true,true,false).active,"ineligible context retained scanner");
    Check(!chord(true).active,"held eligible return opened scanner");
    const std::uint16_t vr=0x1003,physical=1;
    Check(ScannerDpadButtons(vr,true)==0x1002,"Dpad mapping changed another VR button");
    Check((physical|ScannerDpadButtons(vr,true))==vr,"Dpad mapping consumed physical pad input");
    Check(ScannerDpadButtons(vr,false)==vr,"disabled mapping changed native input");
    DualGripScanner scan;
    auto tick=[&](float l,float r,bool toggle=true,bool allowed=true,bool available=true,bool busy=false){return scan.Update(allowed,available,busy,l,r,toggle);};
    Check(!tick(1,1).active,"held entry opened scanner");
    tick(0,0);Check(tick(1,1).active,"both grips did not open");
    Check(tick(0,1).active && tick(1,1).active,"single-hand regrip toggled scanner");
    Check(tick(0,0).active,"toggle closed on release");
    auto r=tick(1,1);Check(!r.active&&r.claimed,"second chord failed to close or leaked held grips");
    Check(tick(0,1).claimed,"partial release leaked grip");
    Check(!tick(0,0).claimed,"released chord retained ownership");
    tick(1,1);r=tick(1,1,true,false);Check(!r.active&&r.claimed,"menu/vehicle context retained scanner");
    Check(!tick(1,1).active,"held context return reopened scanner");
    tick(0,0);Check(tick(1,1).active,"context recovery failed");
    r=tick(0,0,true,true,false);Check(!r.active&&r.claimed,"missing input fabricated release");
    Check(!tick(1,1).active,"held tracking recovery reopened scanner");
    tick(0,0);Check(tick(1,1).active,"tracking recovery release failed");
    tick(0,0,false);Check(tick(1,1,false).active,"hold mode did not open");
    Check(tick(0,1,false).active && tick(1,1,false).active,"hold-mode one-hand regrip closed scanner");
    Check(!tick(0,0,false).active,"hold mode did not close on both release");
    Check(!tick(1,1,false,true,true,true).active,"reload-owned grips opened scanner");
    Check(!tick(1,1,false).active,"busy-release while held opened scanner");
    tick(0,0,false);Check(tick(1,1,false).active,"busy recovery failed");
    Check(!tick(std::numeric_limits<float>::quiet_NaN(),1,false).active,"invalid grip opened scanner");
    tick(0,0);Check(tick(1,1).active,"pre-configuration toggle failed");
    r=tick(1,1,false);Check(!r.active&&r.claimed,"mode switch retained active scanner or leaked held grips");
    Check(!tick(1,1,false).active,"mode switch rearmed without release");
    Check(!tick(0,0,false).claimed,"mode switch ownership did not clear on release");
    Check(tick(1,1,false).active,"hold-mode recovery failed");
    r=tick(1,1,false,false);Check(!r.active&&r.claimed,"feature disable lost held tail");
    Check(!tick(0,0,false,false).claimed,"disabled feature retained released grips");

    ScannerNavigation nav;
    Check(nav.Update(true,0,false,false)==0,"neutral navigated");
    Check(nav.Update(true,1,false,false)==1,"stick did not page up");
    for(int i=0;i<1000;++i)Check(nav.Update(true,1,false,false)==0,"held stick repeated");
    Check(nav.Update(true,.6f,false,false)==0 && nav.Update(true,1,false,false)==0,"stick jitter rearmed");
    nav.Update(true,0,false,false);Check(nav.Update(true,-1,false,false)==-1,"second stick step failed");
    nav.Update(true,0,false,false);Check(nav.Update(true,0,true,false)==1,"Dpad press failed");
    Check(nav.Update(true,0,true,false)==0,"held Dpad repeated");
    nav.Update(true,0,false,false);Check(nav.Update(true,0,true,false)==1,"Dpad release failed to rearm");
    nav.Update(false,1,false,false);Check(nav.Update(true,1,false,false)==0,"context entry with held stick navigated");
    nav.Update(true,0,false,false);Check(nav.Update(true,1,true,false)==1,"digital priority failed");
    Check(nav.Update(true,1,false,false)==0,"Dpad release manufactured analog step");
    std::cout<<"scanner input: PASS\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
