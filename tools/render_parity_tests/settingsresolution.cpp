// Linux x86-64: execute the actual emitted resume against the game's original
// change-predicate bytes. No game process or Windows runtime is required.
#include "Hooks/SettingsResolution.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <sys/mman.h>
#include <unistd.h>

static void Check(bool ok, const char* name) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", name); std::exit(1); }
}
static void Put(std::array<uint8_t, 0xa0>& settings, size_t offset, uint32_t value) {
    std::memcpy(settings.data() + offset, &value, sizeof(value));
}
static uint32_t Get(const std::array<uint8_t, 0xa0>& settings, size_t offset) {
    uint32_t value; std::memcpy(&value, settings.data() + offset, sizeof(value)); return value;
}

int main() {
    // Cyberpunk2077.exe RVA 0x21C584, beginning six bytes before the hook site.
    constexpr uint8_t original[] = {
        0x8b,0x81,0x84,0,0,0, 0x39,0x41,0x18,0x75,0x19,
        0x8b,0x81,0x88,0,0,0,0x39,0x41,0x1c,0x75,0x0e,
        0x8a,0x81,0x91,0,0,0,0x38,0x41,0x28,0x75,0x03,
        0x32,0xc0,0xc3,0xb0,0x01,0xc3
    };
    const auto length = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    auto* code = static_cast<uint8_t*>(mmap(nullptr,length,PROT_READ|PROT_WRITE,
        MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
    Check(code != MAP_FAILED, "allocate executable fixture");
    std::memcpy(code, original, sizeof(original));
    // Enter the real resume with stale prefetched EAX=1024, just as the saved
    // register is restored after a callback changes target width to 1440.
    code[128]=0xb8; const uint32_t stale=1024;
    std::memcpy(code+129,&stale,sizeof(stale));
    Check(cvr::settings_resolution::EmitResume(code+133,code+6),"emit actual resume");
    Check(mprotect(code,length,PROT_READ|PROT_EXEC)==0,"protect executable fixture");
    using Predicate = bool(__attribute__((ms_abi)) *)(const void*);
    const auto native = reinterpret_cast<Predicate>(code);
    const auto patched = reinterpret_cast<Predicate>(code+128);
    std::array<uint8_t,0xa0> settings{};
    auto test=[&](uint32_t aw,uint32_t ah,uint32_t tw,uint32_t th,bool mode,bool expected,const char* name){
        settings.fill(0); Put(settings,0x18,aw); Put(settings,0x1c,ah);
        Put(settings,0x84,tw); Put(settings,0x88,th); settings[0x91]=mode;
        Check(native(settings.data())==expected,"original predicate baseline");
        Check(patched(settings.data())==expected,name);
    };
    test(1024,1280,1440,1600,false,true,"both dimensions changed");
    test(1024,1440,1440,1440,false,true,"width-only branch (old JNE+6 loses mismatch)");
    test(1440,1280,1440,1600,false,true,"height-only mismatch");
    test(1440,1600,1440,1600,false,false,"already applied with stale prefetched width");
    test(1440,1600,1440,1600,true,true,"mode mismatch preserved");
    test(1024,1280,1024,1280,false,false,"unchanged old dimensions");

    settings.fill(0); Put(settings,0x18,1024); Put(settings,0x1c,1280);
    Put(settings,0x84,1024); Put(settings,0x88,1280);
    const auto address=reinterpret_cast<uintptr_t>(settings.data());
    unsigned writes=0;
    cvr::settings_resolution::RequestTarget(address,1440,1600,[&](uintptr_t p,uint32_t v){
        Check(p==address+0x84 || p==address+0x88,"only requested fields written");
        std::memcpy(reinterpret_cast<void*>(p),&v,sizeof(v)); ++writes;
    });
    Check(writes==2 && Get(settings,0x18)==1024 && Get(settings,0x1c)==1280,
        "engine-owned active size retained");
    Check(patched(settings.data()),"request causes engine resize");
    Put(settings,0x18,1440); Put(settings,0x1c,1600);
    Check(!patched(settings.data()),"engine acknowledgement stops resize");

    std::array<uint8_t,cvr::settings_resolution::ResumeSize> invalid{};
    std::array<uint8_t,14> badSite{};
    Check(!cvr::settings_resolution::EmitResume(invalid.data(),badSite.data()),"reject unknown instructions");
    Check(!cvr::settings_resolution::EmitResume(nullptr,code+6),"reject null output");
    munmap(code,length);
    std::puts("PASS: requested-resolution ownership and actual SettingsRes resume instructions");
}
