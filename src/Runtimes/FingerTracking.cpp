#include "Runtimes/FingerTracking.hpp"
#include <chrono>
#include <cstdlib>
#include <cstring>

extern void Log(const char*,...);
namespace cvr::finger {
namespace {
std::uint64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
}
Tracker& Tracking() { static Tracker tracker;return tracker; }

void Tracker::EnableExtensions(const std::vector<XrExtensionProperties>& available,std::vector<const char*>& enabled) {
    m_enabled=m_sourceExtension=m_rangeExtension=false;
    const char* disabled=std::getenv("CVR_XR_FINGER_TRACKING");
    if(disabled && !std::strcmp(disabled,"0")) {
        Log("OpenXRManager[Fingers]: disabled by CVR_XR_FINGER_TRACKING=0\n");return;
    }
    for(const auto& p:available) {
        if(!std::strcmp(p.extensionName,XR_EXT_HAND_TRACKING_EXTENSION_NAME))m_enabled=true;
        if(!std::strcmp(p.extensionName,XR_EXT_HAND_TRACKING_DATA_SOURCE_EXTENSION_NAME))m_sourceExtension=true;
        if(!std::strcmp(p.extensionName,XR_EXT_HAND_JOINTS_MOTION_RANGE_EXTENSION_NAME))m_rangeExtension=true;
    }
    if(m_enabled) {
        enabled.push_back(XR_EXT_HAND_TRACKING_EXTENSION_NAME);
        if(m_sourceExtension)enabled.push_back(XR_EXT_HAND_TRACKING_DATA_SOURCE_EXTENSION_NAME);
        if(m_rangeExtension)enabled.push_back(XR_EXT_HAND_JOINTS_MOTION_RANGE_EXTENSION_NAME);
    }
    Log("OpenXRManager[Fingers]: handTracking=%d controllerSource=%d unobstructedRange=%d\n",
        m_enabled,m_enabled&&m_sourceExtension,m_enabled&&m_rangeExtension);
}

void Tracker::Start(XrInstance instance,XrSystemId system,XrSession session) {
    Stop();
    if(!m_enabled)return;
    XrSystemHandTrackingPropertiesEXT hand{XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT};
    XrSystemProperties properties{XR_TYPE_SYSTEM_PROPERTIES};properties.next=&hand;
    if(XR_FAILED(xrGetSystemProperties(instance,system,&properties)) || !hand.supportsHandTracking) {
        Log("OpenXRManager[Fingers]: system has no hand tracking; retaining game finger animation\n");return;
    }
    xrGetInstanceProcAddr(instance,"xrCreateHandTrackerEXT",reinterpret_cast<PFN_xrVoidFunction*>(&m_create));
    xrGetInstanceProcAddr(instance,"xrDestroyHandTrackerEXT",reinterpret_cast<PFN_xrVoidFunction*>(&m_destroy));
    xrGetInstanceProcAddr(instance,"xrLocateHandJointsEXT",reinterpret_cast<PFN_xrVoidFunction*>(&m_locate));
    if(!m_create || !m_destroy || !m_locate) {
        Log("OpenXRManager[Fingers]: missing hand tracking functions; retaining game finger animation\n");return;
    }
    XrHandTrackingDataSourceEXT controller=XR_HAND_TRACKING_DATA_SOURCE_CONTROLLER_EXT;
    XrHandTrackingDataSourceInfoEXT source{XR_TYPE_HAND_TRACKING_DATA_SOURCE_INFO_EXT};
    source.requestedDataSourceCount=1;source.requestedDataSources=&controller;
    for(int side=0;side<2;++side) {
        XrHandTrackerCreateInfoEXT info{XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT};
        info.hand=side==0 ? XR_HAND_LEFT_EXT:XR_HAND_RIGHT_EXT;info.handJointSet=XR_HAND_JOINT_SET_DEFAULT_EXT;
        if(m_sourceExtension)info.next=&source;
        const XrResult result=m_create(session,&info,&m_tracker[side]);
        Log("OpenXRManager[Fingers]: create side=%d result=%d source=%s\n",side,result,
            m_sourceExtension ? "controller":"runtime default");
        if(XR_FAILED(result))m_tracker[side]=XR_NULL_HANDLE;
    }
}

void Tracker::Stop() {
    for(int side=0;side<2;++side) {
        if(m_tracker[side] && m_destroy)m_destroy(m_tracker[side]);
        m_tracker[side]=XR_NULL_HANDLE;m_reportedActive[side]=m_reportedError[side]=false;
    }
    m_create=nullptr;m_destroy=nullptr;m_locate=nullptr;
    Clear();
}

void Tracker::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);m_sample[0]={};m_sample[1]={};
}

void Tracker::Sample(XrSpace space,XrTime time,bool focused) {
    if(!m_locate)return;
    Sampled samples[2]{};
    if(focused && space && time>0 && m_locate)for(int side=0;side<2;++side) {
        if(!m_tracker[side])continue;
        XrHandJointLocationEXT joints[XR_HAND_JOINT_COUNT_EXT]{};
        XrHandTrackingDataSourceStateEXT source{XR_TYPE_HAND_TRACKING_DATA_SOURCE_STATE_EXT};
        XrHandJointLocationsEXT locations{XR_TYPE_HAND_JOINT_LOCATIONS_EXT};
        locations.jointCount=XR_HAND_JOINT_COUNT_EXT;locations.jointLocations=joints;
        if(m_sourceExtension)locations.next=&source;
        XrHandJointsMotionRangeInfoEXT range{XR_TYPE_HAND_JOINTS_MOTION_RANGE_INFO_EXT};
        range.handJointsMotionRange=XR_HAND_JOINTS_MOTION_RANGE_UNOBSTRUCTED_EXT;
        XrHandJointsLocateInfoEXT info{XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT};info.baseSpace=space;info.time=time;
        if(m_rangeExtension)info.next=&range;
        const XrResult result=m_locate(m_tracker[side],&info,&locations);
        if(XR_FAILED(result)) {
            if(!m_reportedError[side])Log("OpenXRManager[Fingers]: locate side=%d result=%d; retaining game finger animation\n",side,result);
            m_reportedError[side]=true;continue;
        }
        if(m_sourceExtension && (!source.isActive || source.dataSource!=XR_HAND_TRACKING_DATA_SOURCE_CONTROLLER_EXT))continue;
        if(!FromJoints(locations,samples[side].pose))continue;
        samples[side].stamp=NowMs();
        if(!m_reportedActive[side]) {
            Log("OpenXRManager[Fingers]: active side=%d source=%s; five independent finger bends\n",side,
                m_sourceExtension ? "controller":"runtime default");m_reportedActive[side]=true;
        }
    }
    std::lock_guard<std::mutex> lock(m_mutex);m_sample[0]=samples[0];m_sample[1]=samples[1];
}

bool Tracker::Read(int side,Pose& pose) {
    if(side<0 || side>1)return false;
    std::lock_guard<std::mutex> lock(m_mutex);
    if(!Fresh(m_sample[side].stamp,NowMs()))return false;
    pose=m_sample[side].pose;return true;
}
}
