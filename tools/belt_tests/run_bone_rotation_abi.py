#!/usr/bin/env python3
"""Exercise the actual native reader with aligned and CET-style return storage.

The source bodies come from the checked-in baseline and current source. Only the SDK argument
reader and snapshot globals are stubbed; no game, Wine, or active process is used.
Requires Linux x86-64 and clang++; no Git history or game assets are needed.
"""
from pathlib import Path
import subprocess
import tempfile


def extract(source, original, name):
    start = source.index("void " + original + "(")
    end = source.index("\n}\n", start) + 2
    return source[start:end].replace(original + "(", name + "(", 1)


def main():
    repo = Path(__file__).resolve().parents[2]
    readers = {
        "src/Natives/HandBody.cpp": ["VRBoneModelRot", "VRHandRawRot", "VRPalmModelRot", "VRPlayerEngineRot",
                                     "VRViewWorldRot", "VRSmokeMouthWorldRot"],
        "src/Natives/Natives.cpp": ["GetLeftVRHandRot", "GetRightVRHandRot"],
    }
    functions = []
    names = []
    for path, entries in readers.items():
        old = Path(__file__).with_name("fixtures").joinpath("quaternion_readers_before.cpp").read_text()
        new = (repo / path).read_text()
        for name in entries:
            functions += [extract(old, name, "Original" + name), extract(new, name, "Fixed" + name)]
            names.append(name)
    preamble = r'''
#include <array>
#include <cassert>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include "Natives/QuaternionResult.hpp"
#define RED4EXT_UNUSED_PARAMETER(x) (void)(x)
namespace RED4ext {
struct IScriptable {};
struct CStackFrame { char* code; int idx; };
struct alignas(16) Quaternion { float i, j, k, r; };
__attribute__((noinline)) void GetParameter(CStackFrame* frame, int* idx) { *idx = frame->idx; }
}
volatile int g_VRFKSnapCount = 2;
volatile float g_VRFKSnapRot[2][4] = {{0.1f, -0.2f, 0.3f, 0.9f}, {0, 0, 0, 1}};
volatile float g_VRHandRawRot[2][4] = {{0.1f, -0.2f, 0.3f, 0.9f}, {-0.2f, 0.1f, 0, 0.8f}};
volatile float g_VRPalmModelRotL[4] = {0.2f, 0.3f, 0.4f, 0.8f};
volatile float g_VRPalmModelRotR[4] = {-0.2f, -0.3f, -0.4f, 0.8f};
volatile float CyberpunkVR_PlayerEntityQuat[4] = {0.0f, 0.0f, 0.3f, 0.9f};
volatile float g_VRViewWorldRot[4] = {0.3f, 0.1f, 0.0f, 0.8f};
volatile float g_VRSmokeMouthWorldRot[4] = {-0.3f, -0.1f, 0.0f, 0.8f};
bool g_handsStableValid = true;
void EnsureSharedMemory() {}
void RefreshHandsSnapshot() {}
__attribute__((noinline)) float SharedPose(int i) { return float(i) / 20.0f; }
'''
    fixture = r'''
using Reader = void(*)(RED4ext::IScriptable*, RED4ext::CStackFrame*, RED4ext::Quaternion*, int64_t);
static std::array<float,4> run(Reader reader, int offset, int idx, bool nullOutput = false) {
    alignas(16) std::array<unsigned char, 64> bytes;
    bytes.fill(0xa5);
    char code[2]{};
    RED4ext::CStackFrame frame{code, idx};
    auto* dest = bytes.data() + 16 + offset;
    reader(nullptr, &frame, nullOutput ? nullptr : reinterpret_cast<RED4ext::Quaternion*>(dest), 0);
    assert(frame.code == code + 1);
    std::array<float,4> got{};
    if (!nullOutput) std::memcpy(got.data(), dest, sizeof(got));
    for (unsigned i = 0; i < bytes.size(); ++i)
        if (nullOutput || i < unsigned(16 + offset) || i >= unsigned(32 + offset)) assert(bytes[i] == 0xa5);
    return got;
}
int main() {
    // Disable core files for the deliberately faulting pre-fix child.
    rlimit core{0, 0};
    assert(setrlimit(RLIMIT_CORE, &core) == 0);
    run(OriginalVRBoneModelRot, 0, 0);
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0) { run(OriginalVRBoneModelRot, 8, 0); _exit(0); }
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV);
    unsigned cases = 0;
    const std::pair<Reader,Reader> readers[] = {READER_PAIRS};
    for (const auto& pair : readers) {
        for (int offset : {0, 4, 8, 12}) {
            for (int idx : {-1, 0, 1, 2, 1000000}) {
                assert(run(pair.first, offset, idx) == run(pair.second, 0, idx));
                ++cases;
            }
        }
        run(pair.first, 8, 0, true);
        ++cases;
    }
    g_VRFKSnapCount = 0;
    assert(run(FixedVRBoneModelRot, 8, 0) == run(OriginalVRBoneModelRot, 0, 0));
    ++cases;
    g_handsStableValid = false;
    for (const auto& pair : readers) {
        assert(run(pair.first, 8, 1) == run(pair.second, 0, 1));
        ++cases;
    }
    std::cout << "PASS: pre-fix 8-aligned return faults; all 8 fixed readers pass " << cases
              << " offset/index/null/tracking cases with identical values\n";
}
'''
    with tempfile.TemporaryDirectory(prefix="cpvr-bone-abi-") as directory:
        source = Path(directory) / "bone_rotation_abi.cpp"
        binary = Path(directory) / "bone_rotation_abi"
        pairs = ",".join("{Fixed" + name + ",Original" + name + "}" for name in names)
        source.write_text(preamble + "\n".join(functions) + "\n" + fixture.replace("READER_PAIRS", pairs))
        subprocess.run(["clang++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter",
                        "-I" + str(repo / "include"),
                        str(source), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
