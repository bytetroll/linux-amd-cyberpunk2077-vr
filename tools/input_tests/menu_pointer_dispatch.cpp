// Exercise the actual Windows dispatch implementation against a tiny user32
// substitute, including drag lifetime and failure recovery at the OS boundary.
#include <windows.h>
#include "Hooks/NativeMenuPointer.hpp"
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
HWND g_gameHwnd=reinterpret_cast<HWND>(1);
static bool focused=true,loseOnMove=false,launcher=false,occluded=false,occludeOnMove=false;
static unsigned releaseFailures=0,downFailures=0,moveFailures=0;
static uint64_t now=1000;
static std::vector<INPUT> sent;
struct Attempt { DWORD flags;bool accepted; };
static std::vector<Attempt> attempts;
HWND GetForegroundWindow(){return focused?g_gameHwnd:reinterpret_cast<HWND>(2);}
BOOL IsWindow(HWND h){return h!=nullptr;}
BOOL IsWindowVisible(HWND){return true;}
BOOL IsIconic(HWND){return false;}
HWND GetAncestor(HWND h,int){return h;}
HWND WindowFromPoint(POINT){return occluded?reinterpret_cast<HWND>(2):g_gameHwnd;}
HMODULE GetModuleHandleW(const wchar_t*){return reinterpret_cast<HMODULE>(3);}
static BOOL Client(HWND,LPRECT r){*r={0,0,800,600};return true;}
static int Metrics(int n){return n==SM_CXVIRTUALSCREEN?1920:n==SM_CYVIRTUALSCREEN?1080:0;}
FARPROC GetProcAddress(HMODULE,const char* n){return std::strcmp(n,"GetClientRect")==0?reinterpret_cast<FARPROC>(Client):reinterpret_cast<FARPROC>(Metrics);}
uint64_t GetTickCount64(){return now;}
BOOL ClientToScreen(HWND,POINT* p){p->x+=200;p->y+=100;return true;}
unsigned SendInput(unsigned count,INPUT* input,int){
    if(count!=1)throw std::runtime_error("drag dispatcher unexpectedly batched input");
    const auto flags=input[0].mi.dwFlags;
    unsigned* failures=flags==MOUSEEVENTF_LEFTUP?&releaseFailures:
        flags==MOUSEEVENTF_LEFTDOWN?&downFailures:&moveFailures;
    if(*failures){--*failures;attempts.push_back({flags,false});return 0;}
    attempts.push_back({flags,true});sent.push_back(input[0]);
    if(loseOnMove&&(flags&MOUSEEVENTF_MOVE)){focused=false;loseOnMove=false;}
    if(occludeOnMove&&(flags&MOUSEEVENTF_MOVE)){occluded=true;occludeOnMove=false;}
    return 1;
}
extern "C" int IsLauncherOpen(){return launcher;}
void Log(const char*,...){}
#include "../../src/Hooks/NativeMenuPointer.cpp"
static void Check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
static unsigned Buttons(DWORD flags){unsigned n=0;for(const auto& i:sent)if(i.mi.dwFlags&flags)++n;return n;}
static unsigned Failed(DWORD flags){unsigned n=0;for(const auto& i:attempts)if((i.flags&flags)&&!i.accepted)++n;return n;}
static void Clear(){sent.clear();attempts.clear();}
static void Position(const INPUT& event,int x,int y,const char* why){
    Check((event.mi.dwFlags&MOUSEEVENTF_MOVE)!=0 && event.mi.dx==LONG((x+.5)*65536/1920) &&
        event.mi.dy==LONG((y+.5)*65536/1080),why);
}
int main()try {
    using namespace cvr::input;
    NativeMenuPointerFrame sample;
    auto publish=[&]{sample.stampMs=++now;PublishNativeMenuPointer(sample);};
    auto reset=[&]{
        focused=true;launcher=occluded=loseOnMove=occludeOnMove=false;
        releaseFailures=downFailures=moveFailures=0;CyberpunkVR_NativeMenuPointer=1;
        UpdateNativeMenuPointer(false,false,0,false);
        Clear();sample={};sample.active=sample.aimValid=true;sample.origin=1;
        sample.aim={{0,0,0,1},{0,0,0}};sample.panel={{0,0,0,1},{0,0,-1.5f}};sample.size={2,2};publish();
    };
    auto arm=[&]{reset();UpdateNativeMenuPointer(true,false,0,true);Clear();};
    auto drag=[&]{arm();UpdateNativeMenuPointer(true,false,1,true);
        Check(Buttons(MOUSEEVENTF_LEFTDOWN)==1 && Buttons(MOUSEEVENTF_LEFTUP)==0,"drag setup failed");Clear();};

    reset();Check(UpdateNativeMenuPointer(true,false,0,true),"enabled pointer did not own trigger");
    Check(sent.size()==1,"initial move count wrong");Position(sent[0],600,400,"native client pixel mapping bypass failed");
    Clear();UpdateNativeMenuPointer(true,false,1,true);
    Check(sent.size()==2&&sent[1].mi.dwFlags==MOUSEEVENTF_LEFTDOWN,"press was not move then down");
    Check(Buttons(MOUSEEVENTF_LEFTUP)==0,"press released before dragging");
    for(int i=0;i<20;++i)UpdateNativeMenuPointer(true,false,1,true);
    Check(Buttons(MOUSEEVENTF_LEFTDOWN)==1&&Buttons(MOUSEEVENTF_LEFTUP)==0,"held trigger repeated or released mouse button");
    Clear();sample.aim.position={.5f,-.5f,0};publish();UpdateNativeMenuPointer(true,false,1,true);
    Check(sent.size()==1,"held drag generated a button edge");Position(sent[0],800,550,"held drag did not move");
    Clear();sample.aim.position={-.5f,.5f,0};publish();UpdateNativeMenuPointer(true,false,0,true);
    Check(sent.size()==2&&sent[1].mi.dwFlags==MOUSEEVENTF_LEFTUP,"drop was not move then up");
    Position(sent[0],400,250,"drop used previous frame destination");
    Clear();UpdateNativeMenuPointer(true,false,0,true);Check(sent.empty(),"released trigger repeated up");

    // Each interruption releases an owned press, and a held squeeze cannot
    // start another drag when the interrupted condition becomes valid again.
    for(int reason=0;reason<11;++reason){
        drag();bool menu=true,overlay=false,available=true;
        switch(reason){
            case 0:focused=false;break;
            case 1:menu=false;break;
            case 2:overlay=true;break;
            case 3:now+=151;break;
            case 4:++sample.origin;publish();break;
            case 5:occluded=true;break;
            case 6:sample.aim.position.x=2;publish();break;
            case 7:available=false;break;
            case 8:sample.active=false;publish();break;
            case 9:launcher=true;break;
            case 10:CyberpunkVR_NativeMenuPointer=0;break;
        }
        UpdateNativeMenuPointer(menu,overlay,1,available);
        Check(Buttons(MOUSEEVENTF_LEFTUP)==1&&Buttons(MOUSEEVENTF_LEFTDOWN)==0,"interrupted drag did not release exactly once");
        focused=true;launcher=occluded=false;CyberpunkVR_NativeMenuPointer=1;
        sample.active=sample.aimValid=true;sample.aim.position={0,0,0};publish();Clear();
        UpdateNativeMenuPointer(true,false,1,true);
        Check(Buttons(MOUSEEVENTF_LEFTDOWN)==0,"held recovery restarted drag");
        UpdateNativeMenuPointer(true,false,0,true);UpdateNativeMenuPointer(true,false,1,true);
        Check(Buttons(MOUSEEVENTF_LEFTDOWN)==1,"released recovery failed to rearm");
    }
    drag();UpdateNativeMenuPointer(true,false,std::numeric_limits<float>::quiet_NaN(),true);
    Check(Buttons(MOUSEEVENTF_LEFTUP)==1,"nonfinite trigger retained mouse press");
    UpdateNativeMenuPointer(true,false,1,true);Check(Buttons(MOUSEEVENTF_LEFTDOWN)==0,"nonfinite recovery resumed held drag");

    arm();focused=false;UpdateNativeMenuPointer(true,false,1,true);Check(sent.empty(),"unfocused pointer generated OS input");
    focused=true;UpdateNativeMenuPointer(true,false,1,true);Check(Buttons(MOUSEEVENTF_LEFTDOWN)==0,"focus recovery accepted held press");
    arm();loseOnMove=true;UpdateNativeMenuPointer(true,false,1,true);
    Check(Buttons(MOUSEEVENTF_LEFTDOWN)==0,"focus lost during move still pressed");
    focused=true;UpdateNativeMenuPointer(true,false,1,true);Check(Buttons(MOUSEEVENTF_LEFTDOWN)==0,"focus lost during move rearmed press");
    arm();occluded=true;UpdateNativeMenuPointer(true,false,1,true);Check(sent.empty(),"occluding window received pointer events");
    occluded=false;UpdateNativeMenuPointer(true,false,1,true);Check(Buttons(MOUSEEVENTF_LEFTDOWN)==0,"uncovering window accepted held press");
    arm();occludeOnMove=true;UpdateNativeMenuPointer(true,false,1,true);
    Check(Buttons(MOUSEEVENTF_LEFTDOWN)==0,"occlusion appearing during move still pressed");

    drag();releaseFailures=2;UpdateNativeMenuPointer(true,false,0,true);
    Check(Buttons(MOUSEEVENTF_LEFTUP)==0&&Failed(MOUSEEVENTF_LEFTUP)==1,"failed mouse-up fixture did not fail");
    focused=false;UpdateNativeMenuPointer(false,true,1,false);
    Check(Buttons(MOUSEEVENTF_LEFTUP)==0&&Failed(MOUSEEVENTF_LEFTUP)==2,"pending mouse-up was not retried");
    UpdateNativeMenuPointer(false,true,1,false);
    Check(Buttons(MOUSEEVENTF_LEFTUP)==1&&Buttons(MOUSEEVENTF_LEFTDOWN)==0,"pending release lost across focus/menu exit");
    focused=true;publish();UpdateNativeMenuPointer(true,false,1,true);
    Check(Buttons(MOUSEEVENTF_LEFTDOWN)==0,"release retry rearmed held trigger");

    arm();downFailures=1;UpdateNativeMenuPointer(true,false,1,true);
    for(int i=0;i<3;++i)UpdateNativeMenuPointer(true,false,1,true);
    Check(Failed(MOUSEEVENTF_LEFTDOWN)==1&&Buttons(MOUSEEVENTF_LEFTDOWN)==0&&Buttons(MOUSEEVENTF_LEFTUP)==0,"failed down retried or released unowned button");
    UpdateNativeMenuPointer(true,false,0,true);UpdateNativeMenuPointer(true,false,1,true);
    Check(Buttons(MOUSEEVENTF_LEFTDOWN)==1,"failed down blocked fresh released press");
    arm();moveFailures=1;UpdateNativeMenuPointer(true,false,1,true);UpdateNativeMenuPointer(true,false,1,true);
    Check(Failed(MOUSEEVENTF_MOVE)==1&&Buttons(MOUSEEVENTF_LEFTDOWN)==0,"failed pre-press move rearmed held trigger");
    drag();moveFailures=1;sample.aim.position.x=.5f;publish();UpdateNativeMenuPointer(true,false,1,true);
    Check(Buttons(MOUSEEVENTF_LEFTUP)==1&&Buttons(MOUSEEVENTF_LEFTDOWN)==0,"failed drag move retained mouse button");
    UpdateNativeMenuPointer(true,false,1,true);Check(Buttons(MOUSEEVENTF_LEFTDOWN)==0,"failed drag move rearmed held trigger");
    drag();moveFailures=1;publish();UpdateNativeMenuPointer(true,false,0,true);
    Check(Buttons(MOUSEEVENTF_LEFTUP)==1,"failed drop move prevented release cleanup");

    // The existing worker can clear an owned press without another XInput poll.
    // Keeping the render sample fresh while aging the input heartbeat proves
    // that a paused game cannot keep a global OS button held indefinitely.
    drag();MaintainNativeMenuPointer();Check(sent.empty(),"healthy maintenance changed drag");
    for(int reason=0;reason<9;++reason){
        drag();bool forceCancel=false;
        switch(reason){
            case 0:focused=false;break;
            case 1:now+=151;break;
            case 2:now+=251;publish();break;
            case 3:sample.active=false;publish();break;
            case 4:CyberpunkVR_NativeMenuPointer=0;break;
            case 5:++sample.origin;publish();break;
            case 6:sample.aim.position.x=2;publish();break;
            case 7:sample.aimValid=false;publish();break;
            case 8:forceCancel=true;break;
        }
        MaintainNativeMenuPointer(forceCancel);
        Check(sent.size()==1&&sent[0].mi.dwFlags==MOUSEEVENTF_LEFTUP,"maintenance failed release-only cancellation");
        MaintainNativeMenuPointer(forceCancel);Check(sent.size()==1,"maintenance repeated successful up");
        focused=true;CyberpunkVR_NativeMenuPointer=1;sample.active=sample.aimValid=true;sample.aim.position={0,0,0};publish();Clear();
        UpdateNativeMenuPointer(true,false,1,true);
        Check(Buttons(MOUSEEVENTF_LEFTDOWN)==0,"maintenance rearmed held squeeze");
        UpdateNativeMenuPointer(true,false,0,true);UpdateNativeMenuPointer(true,false,1,true);
        Check(Buttons(MOUSEEVENTF_LEFTDOWN)==1,"maintenance prevented fresh released press");
    }
    drag();releaseFailures=2;MaintainNativeMenuPointer(true);
    Check(sent.empty()&&Failed(MOUSEEVENTF_LEFTUP)==1,"shutdown release failure was not recorded");
    MaintainNativeMenuPointer();Check(sent.empty()&&Failed(MOUSEEVENTF_LEFTUP)==2,"watchdog lost pending up after shutdown cancel");
    MaintainNativeMenuPointer();Check(sent.size()==1&&sent[0].mi.dwFlags==MOUSEEVENTF_LEFTUP,"watchdog failed pending-up recovery");
    publish();UpdateNativeMenuPointer(true,false,1,true);
    Check(Buttons(MOUSEEVENTF_LEFTDOWN)==0,"watchdog up retry rearmed held squeeze");
    arm();MaintainNativeMenuPointer(true);Check(sent.empty(),"unowned cancellation released physical mouse button");
    UpdateNativeMenuPointer(true,false,1,true);Check(Buttons(MOUSEEVENTF_LEFTDOWN)==0,"unowned cancellation retained arming");
    reset();CyberpunkVR_NativeMenuPointer=0;
    Check(!UpdateNativeMenuPointer(true,false,1,true)&&sent.empty(),"disabled pointer claimed trigger");
    std::cout<<"native menu pointer dispatch: PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
