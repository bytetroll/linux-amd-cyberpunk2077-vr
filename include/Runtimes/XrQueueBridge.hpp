#pragma once
#include <d3d12.h>
#include <openxr/openxr.h>

namespace cvr::xr {
// One bridge for the mod's single OpenXR session. Initialize before creating
// the session; use only from its submission thread until that thread is joined.
ID3D12CommandQueue* InitializeQueueBridge(ID3D12Device*,ID3D12CommandQueue*,bool steamVR);
bool QueueBridgeFailed();
void FailQueueBridge(const char* operation);
XrResult WaitSwapchainImage(XrSwapchain,const XrSwapchainImageWaitInfo*);
bool SignalSwapchainWork(ID3D12CommandQueue*,ID3D12Fence*,uint64_t);
XrResult ReleaseSwapchainImage(XrSwapchain,const XrSwapchainImageReleaseInfo*,
                             ID3D12Fence* writtenFence=nullptr,uint64_t writtenValue=0);
bool DrainQueueBridge();
void ShutdownQueueBridge();
}
