#include "Runtimes/XrQueueBridge.hpp"
#include "Runtimes/QueueHandoff.hpp"
#include <windows.h>
#include <wrl.h>

extern void Log(const char*,...);
namespace cvr::xr {
namespace {
Microsoft::WRL::ComPtr<ID3D12CommandQueue> renderQueue,runtimeQueue;
Microsoft::WRL::ComPtr<ID3D12Fence> acquiredFence,drainFence;
QueueHandoff handoff;
bool failureLogged=false;
uint64_t drainValue=0;
}
void FailQueueBridge(const char* operation) {
    handoff.Fail();
    if(!failureLogged){failureLogged=true;Log("[xrqueue] %s failed; layer submission stopped until session restart.\n",operation);}
}
bool QueueBridgeFailed(){return handoff.Failed();}
ID3D12CommandQueue* InitializeQueueBridge(ID3D12Device* device,ID3D12CommandQueue* queue,bool steamVR) {
    ShutdownQueueBridge();renderQueue=queue;
    if(FAILED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&drainFence)))) {
        FailQueueBridge("drain fence creation");return nullptr;
    }
    const auto ntdll=GetModuleHandleW(L"ntdll.dll");
    const bool wine=ntdll && GetProcAddress(ntdll,"wine_get_version");
    char setting[8]{};
    const bool shared=GetEnvironmentVariableA("CVR_XR_SHARED_QUEUE",setting,sizeof(setting)) && setting[0]=='1';
    if(wine && steamVR && !shared) {
        auto desc=queue->GetDesc();desc.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Priority=D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;desc.Flags=D3D12_COMMAND_QUEUE_FLAG_NONE;
        if(FAILED(device->CreateCommandQueue(&desc,IID_PPV_ARGS(&runtimeQueue))) ||
           FAILED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&acquiredFence)))) {
            runtimeQueue.Reset();acquiredFence.Reset();
            Log("[xrqueue] Dedicated queue initialization failed; using original shared queue.\n");
        } else runtimeQueue->SetName(L"OpenXR_runtime_handoff_queue");
    }
    Log("[xrqueue] mode=%s wine=%d steamvr=%d optout=%d; rendering stays on game queue.\n",
        runtimeQueue?"dedicated-runtime":"shared",wine,steamVR,shared);
    return runtimeQueue?runtimeQueue.Get():queue;
}
XrResult WaitSwapchainImage(XrSwapchain swapchain,const XrSwapchainImageWaitInfo* info) {
    if(handoff.Failed())return XR_ERROR_RUNTIME_FAILURE;
    const auto result=xrWaitSwapchainImage(swapchain,info);
    if(runtimeQueue && (result==XR_SUCCESS || result==XR_SESSION_LOSS_PENDING)) {
        // The runtime guarantees availability on its binding queue only.
        // Signal BEFORE enqueueing the game wait, including when both virtual
        // D3D12 queues happen to map onto one physical Vulkan queue.
        if(!handoff.Acquire(
            [&](uint64_t value){return SUCCEEDED(runtimeQueue->Signal(acquiredFence.Get(),value));},
            [&](uint64_t value){return SUCCEEDED(renderQueue->Wait(acquiredFence.Get(),value));})) {
            FailQueueBridge("acquire fence handoff");return XR_ERROR_RUNTIME_FAILURE;
        }
    }
    return result;
}
bool SignalSwapchainWork(ID3D12CommandQueue* queue,ID3D12Fence* fence,uint64_t value) {
    if(!queue || !fence || !value || (renderQueue && queue!=renderQueue.Get())) {
        FailQueueBridge("invalid render submission");return false;
    }
    if(!handoff.Submitted([&]{return SUCCEEDED(queue->Signal(fence,value));})) {
        FailQueueBridge("render fence signal");return false;
    }
    return true;
}
XrResult ReleaseSwapchainImage(XrSwapchain swapchain,const XrSwapchainImageReleaseInfo* info,
        ID3D12Fence* writtenFence,uint64_t writtenValue) {
    if(handoff.Failed())return XR_ERROR_RUNTIME_FAILURE;
    if(writtenValue && !writtenFence){FailQueueBridge("missing render fence");return XR_ERROR_RUNTIME_FAILURE;}
    XrResult result=XR_ERROR_RUNTIME_FAILURE;
    // Wait only for the already-signaled copy, not for newer unrelated game
    // work. No game submission mutex may be held while entering the runtime.
    if(!handoff.Release(runtimeQueue && writtenValue,
        [&]{return SUCCEEDED(runtimeQueue->Wait(writtenFence,writtenValue));},
        [&]{result=xrReleaseSwapchainImage(swapchain,info);return XR_SUCCEEDED(result);}))
        FailQueueBridge("release fence handoff or runtime release");
    return result;
}
bool DrainQueueBridge() {
    if(!renderQueue)return true;
    // Teardown runs after joining the XR thread. Both queues must finish using
    // handoff fences before the session and its image resources are destroyed.
    const auto value=++drainValue;
    if(!drainFence || FAILED(renderQueue->Signal(drainFence.Get(),value)) ||
       (runtimeQueue && (FAILED(runtimeQueue->Wait(drainFence.Get(),value)) ||
        FAILED(runtimeQueue->Signal(drainFence.Get(),++drainValue))))) {
        FailQueueBridge("shutdown signal");return false;
    }
    if(drainFence->GetCompletedValue()>=drainValue)return true;
    const auto event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(!event){FailQueueBridge("shutdown event");return false;}
    const bool done=SUCCEEDED(drainFence->SetEventOnCompletion(drainValue,event)) && WaitForSingleObject(event,5000)==WAIT_OBJECT_0;
    CloseHandle(event);
    if(!done)FailQueueBridge("shutdown drain");
    return done;
}
void ShutdownQueueBridge() {
    runtimeQueue.Reset();renderQueue.Reset();acquiredFence.Reset();drainFence.Reset();
    handoff={};failureLogged=false;drainValue=0;
}
}
