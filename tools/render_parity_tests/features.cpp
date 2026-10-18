#include "Stereo/FrameGraphFeatures.hpp"
#include <iostream>
#include <stdexcept>
#include <atomic>
#include <thread>

void Check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main()try {
    using namespace cvr::stereo;
    MainViewFeatureCache cache;
    constexpr uintptr_t mainContext=0x24990f00e10,otherContext=mainContext+0x1000;
    constexpr FrameGraphFlags rtOn{0x3C9C3D7DAF057F53,0x0517F028},rtOff{0x3C00017FAF65FF53,0x0517F028};
    FrameGraphFlags observed{1,2};
    Check(!cache.Read(mainContext,observed) && observed==FrameGraphFlags{1,2},"empty cache changed fallback");
    Check(cache.Observe(mainContext,mainContext,0,rtOn),"MAIN RT-on sample rejected");
    Check(cache.Read(mainContext,observed) && observed==rtOn,"MAIN sample missing");
    Check(cache.Observe(mainContext,mainContext,0,rtOff) && cache.Read(mainContext,observed) && observed==rtOff,
        "RT-off flags with fewer bits did not replace RT-on template");
    Check(!cache.Observe(otherContext,mainContext,0,{~0ull,~0ull}),"unnamed reflection/UI view poisoned MAIN template");
    Check(!cache.Observe(mainContext,mainContext,0x77AD6D6871650500,rtOn),"VRCAM fed its own forced flags back into MAIN");
    Check(!cache.Observe(mainContext,mainContext,0,{}) && cache.Read(mainContext,observed) && observed==rtOff,
        "cleared context flags destroyed a valid producer sample");
    Check(!cache.Read(otherContext,observed) && !cache.Read(0,observed),"replaced/unbound MAIN reused an old owner's flags");
    const FrameGraphFlags noUpscaler{rtOff.f0,rtOff.f1&~0x3A0ull};
    Check(cache.Observe(mainContext,mainContext,0,noUpscaler) && cache.Read(mainContext,observed) && observed==noUpscaler,
        "disabled upscaler bits were retained");
    Check(cache.Observe(mainContext,mainContext,0,rtOn) && cache.Read(mainContext,observed) && observed==rtOn,"RT re-enable failed");
    constexpr FrameGraphFlags pairA{1,0xAAAAAAAA},pairB{2,0xBBBBBBBB};
    cache.Observe(mainContext,mainContext,0,pairA);
    std::atomic<bool> started=false,done=false,torn=false;
    std::thread writer([&]{while(!started.load()){}for(unsigned i=0;i<100000;++i)cache.Observe(mainContext,mainContext,0,i%2?pairA:pairB);done=true;});
    started=true;
    do {
        FrameGraphFlags pair;
        if(!cache.Read(mainContext,pair) || (pair!=pairA && pair!=pairB))torn=true;
    }while(!done.load());
    writer.join();Check(!torn.load(),"feature words came from different publications");
    constexpr uint64_t amdMain=0x0507F008,forcedRtt=0x0517F009;
    const auto safe=SupportedShadingRateFlags(forcedRtt,0);
    Check(safe==(amdMain|1ull),"recorded AMD graph still requests an unsupported shading-rate image");
    Check((safe^forcedRtt)==ShadingRateImageFlagF1,"unrelated graph feature changed");
    Check(SupportedShadingRateFlags(amdMain,0)==amdMain,"already supported AMD flags changed");
    Check(SupportedShadingRateFlags(safe|forcedRtt,0)==safe,"builder OR reinstated unsupported VRS");
    // Model the native guarded allocator from the supplied dump. The failing
    // graph takes ceil(width/tile); the corrected graph never enters it.
    auto allocate=[](uint64_t flags,unsigned width,unsigned tile){
        if(!(flags&ShadingRateImageFlagF1))return 0u;
        if(!tile)throw std::runtime_error("native divide by zero would execute");
        return (width+tile-1)/tile;
    };
    Check(allocate(safe,1832,0)==0,"unsupported allocation reached the native divider");
    for(unsigned tile:{1u,8u,16u,32u}) {
        Check(SupportedShadingRateFlags(forcedRtt,tile)==forcedRtt,"supported device lost VRS");
        Check(allocate(SupportedShadingRateFlags(forcedRtt,tile),1832,tile)==(1832+tile-1)/tile,"supported allocation dimensions changed");
    }
    for(unsigned bit=0;bit<64;++bit) {
        const uint64_t flags=1ull<<bit;
        Check(SupportedShadingRateFlags(flags,0)==(bit==20?0:flags),"another feature was masked");
        Check(SupportedShadingRateFlags(flags,16)==flags,"NVIDIA-capable mask changed");
    }
    // ---- the second view's upscaler groups must be MAIN's, not DLSS specifically ----
    // The builder's own predicate: with no group among 69/71/72/73 it emits the native
    // TAAU/resolve, which re-upscales at render resolution and crops the view.
    auto cropsTheView=[](uint64_t f1){return (f1&UpscalerGroupMaskF1)==0;};
    constexpr uint64_t fsr2=1ull<<(71-64),fsr3=1ull<<(72-64),xess=1ull<<(73-64);
    // What the force used to be: group 69, and only when MAIN had group 69.
    auto forceDlssOnly=[](uint64_t viewF1,uint64_t mainGroups){
        return (mainGroups&DlssGroupFlagF1) ? ((viewF1&~UpscalerGroupMaskF1)|DlssGroupFlagF1) : viewF1;};
    // MAIN's F1 as recorded on the reporter's RX 5700 XT (#93). It carries NO upscaler group,
    // so that machine's MAIN was on the native resolve -- and the second view must join it
    // there rather than be pushed onto an upscaler MAIN is not using.
    constexpr uint64_t amdMainF1=0x0507F008;
    Check((amdMainF1&UpscalerGroupMaskF1)==0,"recorded AMD MAIN F1 unexpectedly names an upscaler");
    Check(MirrorUpscalerGroups(amdMainF1,amdMainF1&UpscalerGroupMaskF1)==amdMainF1,
        "a MAIN with no upscaler had groups invented for its second view");
    // The failure this fix exists for: MAIN on FSR3, which the DLSS-only force cannot see.
    constexpr uint64_t vrcamBare=amdMainF1,mainFsr3=amdMainF1|fsr3;
    Check(cropsTheView(forceDlssOnly(vrcamBare,mainFsr3&UpscalerGroupMaskF1)),
        "the DLSS-only force was expected to leave an FSR3 MAIN's second view cropping");
    const auto mirrored=MirrorUpscalerGroups(vrcamBare,mainFsr3&UpscalerGroupMaskF1);
    Check(!cropsTheView(mirrored),"second view still takes the cropping native-resolve branch under FSR3");
    Check((mirrored&UpscalerGroupMaskF1)==fsr3,"second view did not land on MAIN's own upscaler");
    Check((mirrored&~UpscalerGroupMaskF1)==(vrcamBare&~UpscalerGroupMaskF1),
        "mirroring the upscaler changed an unrelated graph feature");
    // Each upscaler in turn, including DLSS: the new path must not regress the NVIDIA case.
    for(uint64_t group:{DlssGroupFlagF1,fsr2,fsr3,xess}) {
        Check(MirrorUpscalerGroups(vrcamBare,group)==(vrcamBare|group),"upscaler group not applied");
        Check(!cropsTheView(MirrorUpscalerGroups(vrcamBare,group)),"crop branch still reachable");
        // A view already holding the WRONG upscaler is corrected, not OR-ed into holding two.
        for(uint64_t stale:{DlssGroupFlagF1,fsr2,fsr3,xess}) {
            const auto fixed=MirrorUpscalerGroups(vrcamBare|stale,group);
            Check((fixed&UpscalerGroupMaskF1)==group,"stale upscaler group survived the mirror");
        }
        // Idempotent, and a second pass right before the builder cannot undo the first.
        Check(MirrorUpscalerGroups(MirrorUpscalerGroups(vrcamBare,group),group)==
              MirrorUpscalerGroups(vrcamBare,group),"second force site disagreed with the first");
        Check(MirrorUpscalerGroups(MirrorUpscalerGroups(vrcamBare,group),0)==
              MirrorUpscalerGroups(vrcamBare,group),"a later groupless frame cleared a forced group");
    }
    // The mirror and the VRS filter are independent: neither reinstates what the other removed.
    Check(SupportedShadingRateFlags(MirrorUpscalerGroups(forcedRtt,fsr3),0)==
          (MirrorUpscalerGroups(forcedRtt,fsr3)&~ShadingRateImageFlagF1),
        "mirroring the upscaler smuggled an unsupported shading-rate image back in");
    for(unsigned bit=0;bit<64;++bit) {
        const uint64_t flags=1ull<<bit;
        const bool isUpscaler=(flags&UpscalerGroupMaskF1)!=0;
        Check((MirrorUpscalerGroups(flags,fsr3)&~UpscalerGroupMaskF1)==(flags&~UpscalerGroupMaskF1),
            "the mirror touched a bit outside the upscaler group mask");
        Check(MirrorUpscalerGroups(flags,0)==flags,"a groupless MAIN rewrote the view's flags");
        if(!isUpscaler)Check(MirrorUpscalerGroups(flags,fsr3)==(flags|fsr3),"non-upscaler bit lost");
    }
    std::cout<<"PASS RT on/off/re-enable, producer identity, coherent flag pair, recorded AMD "
               "divide-by-zero graph, supported VRS and MAIN-upscaler mirroring for "
               "DLSS/FSR2/FSR3/XeSS\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
