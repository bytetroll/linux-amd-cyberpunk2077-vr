#pragma once
#include <cstdint>
#define WINAPI
#define __declspec(x)
using BOOL=int;
constexpr BOOL FALSE=0;
using DWORD=uint32_t;
using LONG=int32_t;
using HWND=void*;
using HMODULE=void*;
using FARPROC=void(*)();
struct RECT { LONG left,top,right,bottom; };
using LPRECT=RECT*;
struct POINT { LONG x,y; };
struct MOUSEINPUT { LONG dx{},dy{};DWORD mouseData{},dwFlags{},time{};uintptr_t dwExtraInfo{}; };
struct INPUT { DWORD type{};MOUSEINPUT mi; };
constexpr DWORD INPUT_MOUSE=0,MOUSEEVENTF_MOVE=1,MOUSEEVENTF_LEFTDOWN=2,MOUSEEVENTF_LEFTUP=4;
constexpr DWORD MOUSEEVENTF_ABSOLUTE=0x8000,MOUSEEVENTF_VIRTUALDESK=0x4000;
constexpr int GA_ROOT=2,SM_XVIRTUALSCREEN=76,SM_YVIRTUALSCREEN=77,SM_CXVIRTUALSCREEN=78,SM_CYVIRTUALSCREEN=79;
HWND GetForegroundWindow();
BOOL IsWindow(HWND);
BOOL IsWindowVisible(HWND);
BOOL IsIconic(HWND);
HWND GetAncestor(HWND,int);
HWND WindowFromPoint(POINT);
HMODULE GetModuleHandleW(const wchar_t*);
FARPROC GetProcAddress(HMODULE,const char*);
uint64_t GetTickCount64();
BOOL ClientToScreen(HWND,POINT*);
unsigned SendInput(unsigned,INPUT*,int);
