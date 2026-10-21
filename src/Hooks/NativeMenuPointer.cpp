#include "Hooks/NativeMenuPointer.hpp"
#include "Hooks/PhysicalClientRect.hpp"
#include "Overlay/LauncherDialog.hpp"
#include <windows.h>
#include <mutex>

extern HWND g_gameHwnd;
extern void Log(const char*,...);
extern "C" __declspec(dllexport) int32_t CyberpunkVR_NativeMenuPointer=1;

namespace cvr::input {
namespace {
std::mutex frameMutex,inputMutex;
NativeMenuPointerFrame current;
MenuPointerButton button;
uint64_t lastInputPollMs{},mouseOrigin{};
bool mouseDown=false,pendingRelease=false;
void ReleaseOwnedMouse() {
    if(!mouseDown && !pendingRelease)return;
    INPUT release{};release.type=INPUT_MOUSE;release.mi.dwFlags=MOUSEEVENTF_LEFTUP;
    pendingRelease=SendInput(1,&release,sizeof(INPUT))!=1;
    mouseDown=pendingRelease;
    if(!mouseDown)mouseOrigin=0;
}
bool Foreground(HWND window) {
    const HWND foreground=GetForegroundWindow();
    return window && IsWindow(window) && IsWindowVisible(window) && !IsIconic(window) &&
        (foreground==window || GetAncestor(foreground,GA_ROOT)==window) && !IsLauncherOpen();
}
bool PixelBelongsToGame(HWND window,POINT pixel) {
    const HWND target=WindowFromPoint(pixel);
    return target && (target==window || GetAncestor(target,GA_ROOT)==window);
}
struct User32 {
    using Metrics=int(WINAPI*)(int);
    Metrics metrics{};
    User32() {
        // The plugin's IAT intentionally reports virtual sizes to the game.
        // Absolute OS input needs actual client/desktop pixels instead.
        const auto module=GetModuleHandleW(L"user32.dll");
        metrics=reinterpret_cast<Metrics>(GetProcAddress(module,"GetSystemMetrics"));
    }
};
}
void PublishNativeMenuPointer(const NativeMenuPointerFrame& frame) {
    std::lock_guard lock(frameMutex);current=frame;
}
void MaintainNativeMenuPointer(bool cancel) {
    std::lock_guard inputLock(inputMutex);
    const auto now=GetTickCount64();
    NativeMenuPointerFrame frame;
    {std::lock_guard lock(frameMutex);frame=current;}
    const bool fresh=frame.active && frame.aimValid && frame.stampMs && now>=frame.stampMs && now-frame.stampMs<=150;
    const bool inputFresh=lastInputPollMs && now>=lastInputPollMs && now-lastInputPollMs<=250;
    const bool hit=fresh && IntersectNativeMenu(frame.aim,frame.panel,frame.size.width,frame.size.height).valid;
    if(cancel || pendingRelease || CyberpunkVR_NativeMenuPointer==0 || !Foreground(g_gameHwnd) ||
       !inputFresh || !hit || (mouseDown && frame.origin!=mouseOrigin)) {
        button.Update(false,false,false,false,0,false,0);
        ReleaseOwnedMouse();
        if(cancel)lastInputPollMs=0;
    }
}
bool UpdateNativeMenuPointer(bool nativeMenu,bool overlayCapture,float trigger,bool triggerAvailable) {
    std::lock_guard inputLock(inputMutex);
    static User32 os;
    static uint64_t movedStamp{};
    static unsigned buttonLogs{};
    const auto now=GetTickCount64();
    lastInputPollMs=now;
    const bool enabled=CyberpunkVR_NativeMenuPointer!=0 && nativeMenu && !overlayCapture;
    if(pendingRelease) {
        // Release only a press we successfully inserted, even after focus/menu
        // changes. A failed mouse-up must never allow another mouse-down.
        ReleaseOwnedMouse();
        button.Update(false,false,false,false,0,false,0);
        if(pendingRelease)return enabled;
    }
    NativeMenuPointerFrame frame;
    {std::lock_guard lock(frameMutex);frame=current;}
    const HWND window=g_gameHwnd;
    const bool fresh=frame.active && frame.aimValid && frame.stampMs && now>=frame.stampMs && now-frame.stampMs<=150;
    const auto hit=fresh?IntersectNativeMenu(frame.aim,frame.panel,frame.size.width,frame.size.height):MenuPointerHit{};
    bool target=enabled && Foreground(window) && fresh && hit.valid && os.metrics;
    POINT pixel{};
    int x=0,y=0,width=0,height=0;
    if(target) {
        RECT client{};
        target=PhysicalClientRect(window,&client) && client.right>client.left && client.bottom>client.top;
        if(target) {
            pixel={MenuClientPixel(hit.u,client.right-client.left),MenuClientPixel(hit.v,client.bottom-client.top)};
            target=ClientToScreen(window,&pixel)!=0;
        }
        if(target) {
            x=os.metrics(SM_XVIRTUALSCREEN);y=os.metrics(SM_YVIRTUALSCREEN);
            width=os.metrics(SM_CXVIRTUALSCREEN);height=os.metrics(SM_CYVIRTUALSCREEN);
            target=width>0 && height>0 && pixel.x>=x && pixel.x<x+width && pixel.y>=y && pixel.y<y+height &&
                PixelBelongsToGame(window,pixel);
        }
    }
    // Move before either button edge: the release must drop at this sample's
    // destination, not the previous frame's cursor position.
    const bool buttonEdge=(trigger>=.65f && !mouseDown) || (trigger<=.35f && mouseDown);
    if(target && (movedStamp!=frame.stampMs || buttonEdge)) {
        INPUT move{};move.type=INPUT_MOUSE;
        move.mi.dx=LONG(std::clamp((double(pixel.x-x)+.5)*65536.0/width,0.0,65535.0));
        move.mi.dy=LONG(std::clamp((double(pixel.y-y)+.5)*65536.0/height,0.0,65535.0));
        move.mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_VIRTUALDESK;
        target=Foreground(window) && PixelBelongsToGame(window,pixel) && SendInput(1,&move,sizeof(INPUT))==1;
        if(target)movedStamp=frame.stampMs;
    }
    target=target && Foreground(window) && PixelBelongsToGame(window,pixel);
    const bool desired=button.Update(enabled,Foreground(window),fresh,target,trigger,triggerAvailable,frame.origin);
    if(mouseDown && !desired) {
        ReleaseOwnedMouse();
        if(pendingRelease)button.Update(false,false,false,false,0,false,0);
    } else if(desired && !mouseDown) {
        INPUT press{};press.type=INPUT_MOUSE;press.mi.dwFlags=MOUSEEVENTF_LEFTDOWN;
        mouseDown=SendInput(1,&press,sizeof(INPUT))==1;
        if(mouseDown)mouseOrigin=frame.origin;
        if(!mouseDown)button.Update(false,false,false,false,0,false,0);
        if(buttonLogs<24)Log("[menupointer] down=%d u=%.3f v=%.3f ageMs=%llu sample=%u/24\n",
            int(mouseDown),hit.u,hit.v,static_cast<unsigned long long>(now-frame.stampMs),++buttonLogs);
    }
    return enabled;
}
} // namespace cvr::input
