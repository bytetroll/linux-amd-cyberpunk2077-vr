#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace cvr::settings_resolution {
// Only the engine can acknowledge an applied size. Updating its active fields
// here would hide the pending resource resize from its change predicate.
template<class Write>
void RequestTarget(uintptr_t settings, uint32_t width, uint32_t height, Write write) {
    write(settings + 0x84, width);
    write(settings + 0x88, height);
}

constexpr size_t ResumeSize = 29;

// Replay the overwritten change predicate after the callback. The original
// function loaded target width before the hook; reload it after our request.
// Relocate its width-mismatch branch to the original true-return path, then
// leave the height comparison's flags for the untouched following JNE.
inline bool EmitResume(uint8_t* output, const uint8_t* site) {
    constexpr uint8_t first[] = {0x39, 0x41, 0x18, 0x75};
    constexpr uint8_t last[] = {0x8b, 0x81, 0x88, 0, 0, 0, 0x39, 0x41, 0x1c};
    if (!output || !site || std::memcmp(site, first, sizeof(first)) ||
        std::memcmp(site + 5, last, sizeof(last))) return false;
    uint8_t bytes[ResumeSize] = {
        0x8b, 0x81, 0x84, 0, 0, 0, // mov eax,[rcx+84h]
        0x39, 0x41, 0x18,          // cmp [rcx+18h],eax
        0x0f, 0x85, 0, 0, 0, 0,   // jne original width-mismatch destination
        0x8b, 0x81, 0x88, 0, 0, 0, // mov eax,[rcx+88h]
        0x39, 0x41, 0x1c,          // cmp [rcx+1ch],eax
        0xe9, 0, 0, 0, 0          // jmp original height JNE
    };
    const auto widthTarget = reinterpret_cast<intptr_t>(site) + 5 +
        static_cast<int8_t>(site[4]);
    const auto widthDelta = widthTarget - (reinterpret_cast<intptr_t>(output) + 15);
    const auto resumeDelta = reinterpret_cast<intptr_t>(site + 14) -
        (reinterpret_cast<intptr_t>(output) + static_cast<intptr_t>(ResumeSize));
    constexpr auto lo = (std::numeric_limits<int32_t>::min)();
    constexpr auto hi = (std::numeric_limits<int32_t>::max)();
    if (widthDelta < lo || widthDelta > hi || resumeDelta < lo || resumeDelta > hi) return false;
    const auto widthRel = static_cast<int32_t>(widthDelta);
    const auto resumeRel = static_cast<int32_t>(resumeDelta);
    std::memcpy(bytes + 11, &widthRel, sizeof(widthRel));
    std::memcpy(bytes + 25, &resumeRel, sizeof(resumeRel));
    std::memcpy(output, bytes, sizeof(bytes));
    return true;
}
}
