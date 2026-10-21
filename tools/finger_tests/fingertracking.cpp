// Compile with src/Runtimes/FingerTracking.cpp and the pinned OpenXR headers.
// Mock only the runtime boundary; exercise the production tracker and math.
#include "Runtimes/FingerTracking.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <chrono>
#include <thread>

namespace {
int checks{},created{},destroyed{},located{};
bool supported=true,active=true,controllerSource=true,missingProc=false,createFails=false,locateFails=false;
bool rightCreateFails=false;
bool requestedController{},requestedUnobstructed{};
std::array<XrHandJointLocationEXT,XR_HAND_JOINT_COUNT_EXT> joints{};
void Check(bool condition,const char* description) {
    ++checks;if(!condition) { std::fprintf(stderr,"FAIL: %s\n",description);std::abort(); }
}
bool Near(float a,float b) { return std::abs(a-b)<.00003f; }
void MakeHand() {
    constexpr int first[5]={2,6,11,16,21};
    joints={};
    for(int f=0;f<5;++f) {
        float angle=0;XrVector3f p{float(f)*.03f,0,0};
        for(int j=0;j<(f==0 ? 4:5);++j) {
            auto& joint=joints[first[f]+j];joint.locationFlags=XR_SPACE_LOCATION_POSITION_VALID_BIT;
            joint.pose.position=p;
            p.x+=.025f*std::cos(angle);p.y+=.025f*std::sin(angle);
            angle+=.05f*float(f+1)*float(j+1);
        }
    }
}
XrHandJointLocationsEXT Locations() {
    XrHandJointLocationsEXT l{};l.type=XR_TYPE_HAND_JOINT_LOCATIONS_EXT;
    l.isActive=true;l.jointCount=XR_HAND_JOINT_COUNT_EXT;l.jointLocations=joints.data();return l;
}
std::vector<XrExtensionProperties> Extensions(bool source=true,bool range=true) {
    std::vector<XrExtensionProperties> result;
    for(const char* name:{XR_EXT_HAND_TRACKING_EXTENSION_NAME,
            source ? XR_EXT_HAND_TRACKING_DATA_SOURCE_EXTENSION_NAME:nullptr,
            range ? XR_EXT_HAND_JOINTS_MOTION_RANGE_EXTENSION_NAME:nullptr}) {
        if(!name)continue;
        XrExtensionProperties p{};p.type=XR_TYPE_EXTENSION_PROPERTIES;std::strcpy(p.extensionName,name);result.push_back(p);
    }
    return result;
}
XRAPI_ATTR XrResult XRAPI_CALL Create(XrSession,const XrHandTrackerCreateInfoEXT* info,XrHandTrackerEXT* tracker) {
    ++created;
    if(info->next) {
        const auto* source=static_cast<const XrHandTrackingDataSourceInfoEXT*>(info->next);
        requestedController=source->type==XR_TYPE_HAND_TRACKING_DATA_SOURCE_INFO_EXT && source->requestedDataSourceCount==1 &&
            source->requestedDataSources[0]==XR_HAND_TRACKING_DATA_SOURCE_CONTROLLER_EXT;
    }
    if(createFails || (rightCreateFails && info->hand==XR_HAND_RIGHT_EXT))return XR_ERROR_FEATURE_UNSUPPORTED;
    *tracker=reinterpret_cast<XrHandTrackerEXT>(std::uintptr_t(info->hand));return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL Destroy(XrHandTrackerEXT) { ++destroyed;return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL Locate(XrHandTrackerEXT,const XrHandJointsLocateInfoEXT* info,XrHandJointLocationsEXT* output) {
    ++located;
    if(locateFails)return XR_ERROR_TIME_INVALID;
    if(info->next) {
        const auto* range=static_cast<const XrHandJointsMotionRangeInfoEXT*>(info->next);
        requestedUnobstructed=range->type==XR_TYPE_HAND_JOINTS_MOTION_RANGE_INFO_EXT &&
            range->handJointsMotionRange==XR_HAND_JOINTS_MOTION_RANGE_UNOBSTRUCTED_EXT;
    }
    output->isActive=active;
    for(int i=0;i<XR_HAND_JOINT_COUNT_EXT;++i)output->jointLocations[i]=joints[i];
    if(output->next) {
        auto* source=static_cast<XrHandTrackingDataSourceStateEXT*>(output->next);
        source->isActive=active;source->dataSource=controllerSource ? XR_HAND_TRACKING_DATA_SOURCE_CONTROLLER_EXT:XR_HAND_TRACKING_DATA_SOURCE_UNOBSTRUCTED_EXT;
    }
    return XR_SUCCESS;
}
void SetDisabled(bool disabled) {
#ifdef _WIN32
    _putenv_s("CVR_XR_FINGER_TRACKING",disabled ? "0":"");
#else
    if(disabled)setenv("CVR_XR_FINGER_TRACKING","0",1);else unsetenv("CVR_XR_FINGER_TRACKING");
#endif
}
}
void Log(const char*,...) {}
XRAPI_ATTR XrResult XRAPI_CALL xrGetSystemProperties(XrInstance,XrSystemId,XrSystemProperties* properties) {
    static_cast<XrSystemHandTrackingPropertiesEXT*>(properties->next)->supportsHandTracking=supported;return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(XrInstance,const char* name,PFN_xrVoidFunction* function) {
    if(missingProc)return XR_ERROR_FUNCTION_UNSUPPORTED;
    if(!std::strcmp(name,"xrCreateHandTrackerEXT"))*function=reinterpret_cast<PFN_xrVoidFunction>(Create);
    else if(!std::strcmp(name,"xrDestroyHandTrackerEXT"))*function=reinterpret_cast<PFN_xrVoidFunction>(Destroy);
    else if(!std::strcmp(name,"xrLocateHandJointsEXT"))*function=reinterpret_cast<PFN_xrVoidFunction>(Locate);
    return XR_SUCCESS;
}

int main() {
    using namespace cvr::finger;
    SetDisabled(false);MakeHand();auto locations=Locations();Pose pose{};
    Check(FromJoints(locations,pose),"valid inferred controller joints do not need TRACKED flags");
    for(int f=0;f<5;++f)for(int j=0;j<(f==0 ? 2:3);++j)
        Check(Near(pose.bend[f][j],.05f*float(f+1)*float(j+1)),"each digit and phalanx retains its independent bend");
    for(auto& joint:joints) {
        auto p=joint.pose.position;joint.pose.position={3+p.y,-2+p.z,1+p.x};
    }
    Pose transformed{};Check(FromJoints(locations,transformed),"translated and rotated skeleton valid");
    for(int f=0;f<5;++f)for(int j=0;j<3;++j)Check(Near(pose.bend[f][j],transformed.bend[f][j]),"rigid motion leaves bend unchanged");
    locations.isActive=false;Check(!FromJoints(locations,pose),"inactive hand rejected");locations.isActive=true;
    locations.jointCount=25;Check(!FromJoints(locations,pose),"wrong joint count rejected");locations.jointCount=26;
    locations.jointLocations=nullptr;Check(!FromJoints(locations,pose),"null joint buffer rejected");locations.jointLocations=joints.data();
    MakeHand();joints[12].locationFlags=0;Check(!FromJoints(locations,pose),"invalid inferred position rejected");
    MakeHand();joints[12].pose.position.x=std::numeric_limits<float>::quiet_NaN();Check(!FromJoints(locations,pose),"NaN rejected");
    MakeHand();joints[12].pose.position.x=std::numeric_limits<float>::infinity();Check(!FromJoints(locations,pose),"infinity rejected");
    MakeHand();joints[12].pose.position=joints[11].pose.position;Check(!FromJoints(locations,pose),"collapsed bone rejected");
    float bend{};Check(Bend({0,0,0},{1,0,0},{2,0,0},bend)&&bend==0,"straight digit opens fully");
    Check(Bend({0,0,0},{1,0,0},{0,0,0},bend)&&Near(bend,1.91986218f),"implausible bend bounded");
    Check(!Fresh(0,1000)&&Fresh(1000,1150)&&!Fresh(1000,1151)&&!Fresh(1000,999),"missing stale and future samples rejected");
    Check(FreeHand(0,false,false)&&FreeHand(1,false,false),"both unarmed hands use tracking");
    Check(FreeHand(0,true,false)&&!FreeHand(1,true,false),"right weapon grip preserved");
    Check(!FreeHand(0,true,true)&&FreeHand(1,true,true),"left carried weapon grip preserved");
    Check(!FreeHand(0,false,true)&&FreeHand(1,false,true),"left carry reserves hand even when generic weapon flag clears");
    Check(!FreeHand(-1,false,false)&&!FreeHand(2,false,false),"invalid side rejected");
    const float identity[4]={0,0,0,1};float rotation[4]{};
    Check(LocalFlexion(identity,.6f,rotation)&&Near(rotation[2],-std::sin(.3f))&&Near(rotation[3],std::cos(.3f)),"flexion uses rig local negative Z");
    const float reference[4]={std::sin(.2f)*std::cos(.4f),std::sin(.2f)*std::sin(.4f),
        -std::cos(.2f)*std::sin(.4f),std::cos(.2f)*std::cos(.4f)};
    Check(LocalFlexion(reference,.6f,rotation),"pre-bent reference accepted");
    Check(Near(rotation[0],std::sin(.2f)*std::cos(.3f))&&Near(rotation[1],std::sin(.2f)*std::sin(.3f))&&
        Near(rotation[2],-std::cos(.2f)*std::sin(.3f))&&Near(rotation[3],std::cos(.2f)*std::cos(.3f)),"bind curl replaced while orthogonal swing preserved");
    Check(LocalFlexion(reference,0,rotation)&&Near(rotation[0],std::sin(.2f))&&Near(rotation[1],0)&&Near(rotation[2],0)&&Near(rotation[3],std::cos(.2f)),"fully open removes existing bind curl");
    float negative[4];for(int i=0;i<4;++i)negative[i]=-reference[i];float equivalent[4]{};
    Check(LocalFlexion(negative,0,equivalent),"sign-equivalent quaternion accepted");
    float dot=0;for(int i=0;i<4;++i)dot+=equivalent[i]*rotation[i];Check(Near(std::abs(dot),1),"quaternion sign does not alter pose");
    const float singular[4]={1,0,0,0},zero[4]={0,0,0,0};
    Check(!LocalFlexion(singular,.2f,rotation)&&!LocalFlexion(zero,.2f,rotation),"singular or invalid reference falls back");
    Check(!LocalFlexion(identity,std::numeric_limits<float>::quiet_NaN(),rotation),"invalid target bend falls back");

    Tracker tracker;std::vector<const char*> enabled;
    tracker.EnableExtensions({},enabled);tracker.Start(XR_NULL_HANDLE,1,XR_NULL_HANDLE);
    Check(enabled.empty()&&created==0,"unsupported extension leaves fallback and creates no tracker");
    tracker.EnableExtensions(Extensions(),enabled);Check(enabled.size()==3,"enables tracking and available optional extensions");
    supported=false;tracker.Start(XR_NULL_HANDLE,1,XR_NULL_HANDLE);Check(created==0,"unsupported system creates no tracker");supported=true;
    missingProc=true;tracker.Start(XR_NULL_HANDLE,1,XR_NULL_HANDLE);Check(created==0,"missing entrypoint retains fallback");missingProc=false;
    tracker.Start(XR_NULL_HANDLE,1,XR_NULL_HANDLE);
    Check(created==2&&requestedController,"both hands request controller-derived tracking");
    MakeHand();const auto space=reinterpret_cast<XrSpace>(std::uintptr_t(1));
    tracker.Sample(space,100,true);Check(located==2&&requestedUnobstructed,"locates both hands with unobstructed range");
    Check(tracker.Read(0,pose)&&tracker.Read(1,pose),"both active hands published");
    Check(!tracker.Read(-1,pose)&&!tracker.Read(2,pose),"read bounds checked");
    std::this_thread::sleep_for(std::chrono::milliseconds(MaxAgeMs+10));
    Check(!tracker.Read(0,pose),"stalled XR sampling expires in actual tracker");
    const int before=located;tracker.Sample(space,101,false);
    Check(located==before&&!tracker.Read(0,pose)&&!tracker.Read(1,pose),"focus loss clears immediately without locate calls");
    tracker.Sample(space,102,true);active=false;tracker.Sample(space,103,true);
    Check(!tracker.Read(0,pose),"inactive hand clears old valid sample");active=true;
    controllerSource=false;tracker.Sample(space,104,true);Check(!tracker.Read(0,pose),"unrequested source rejected");controllerSource=true;
    tracker.Sample(space,105,true);joints[14].locationFlags=0;tracker.Sample(space,106,true);
    Check(!tracker.Read(0,pose),"invalid runtime skeleton clears old valid sample");MakeHand();
    locateFails=true;tracker.Sample(space,107,true);Check(!tracker.Read(0,pose),"runtime locate failure falls back");locateFails=false;
    tracker.Sample(space,108,true);tracker.Clear();Check(!tracker.Read(0,pose),"session stop clears publication");
    tracker.Stop();Check(destroyed==2&&!tracker.Read(0,pose),"shutdown destroys trackers and clears publication");
    tracker.Stop();Check(destroyed==2,"repeated shutdown safe");
    enabled.clear();tracker.EnableExtensions(Extensions(false,false),enabled);
    requestedController=requestedUnobstructed=false;tracker.Start(XR_NULL_HANDLE,1,XR_NULL_HANDLE);tracker.Sample(space,109,true);
    Check(enabled.size()==1&&!requestedController&&!requestedUnobstructed&&tracker.Read(0,pose),"base extension works without optional chains");tracker.Stop();
    createFails=true;tracker.Start(XR_NULL_HANDLE,1,XR_NULL_HANDLE);const int priorDestroy=destroyed;
    tracker.Sample(space,110,true);Check(!tracker.Read(0,pose),"tracker creation failure falls back");tracker.Stop();
    Check(destroyed==priorDestroy,"failed creation not destroyed as a live handle");createFails=false;
    rightCreateFails=true;tracker.Start(XR_NULL_HANDLE,1,XR_NULL_HANDLE);tracker.Sample(space,111,true);
    Check(tracker.Read(0,pose)&&!tracker.Read(1,pose),"partial tracker creation keeps supported hand and falls back on other");
    const int beforePartialStop=destroyed;tracker.Stop();Check(destroyed==beforePartialStop+1,"partial creation destroys only valid tracker");rightCreateFails=false;
    SetDisabled(true);enabled.clear();tracker.EnableExtensions(Extensions(),enabled);tracker.Start(XR_NULL_HANDLE,1,XR_NULL_HANDLE);
    Check(enabled.empty()&&!tracker.Read(0,pose),"explicit rollback disables extension and publication");SetDisabled(false);
    std::printf("PASS: %d finger tracking checks\n",checks);
}
