#pragma once

// Exception containment for the detour thunks.
//
// The ten entry points in src/Hooks/*.s sit between the engine and these
// callbacks, preserving the engine's volatile registers and flags across each
// call. On the clang-cl build those thunks carry NO Windows unwind record: no
// assembler available on Linux will emit .pdata/.xdata for hand-written
// assembly (for PE targets GCC emits .seh_* from the compiler, and the
// assembler only consumes it), so .cfi_* in the .s files are advisory and the
// .eh_frame they produce is ignored by lld-link. The MASM/.asm path built by
// MSVC does get real unwind records, via PROC FRAME.
//
// That asymmetry is only safe if nothing can unwind through a thunk. So every
// callback below is wrapped: an exception is swallowed and counted rather than
// left to walk a frame the runtime cannot describe. The MASM build carries the
// unwind records as well, but a detour that has to hand the engine back its
// exact register and flag state cannot meaningfully tolerate an unwind through
// it either -- making the invariant explicit in code is the honest version.
//
// The fallback each callback returns on the exception path is always the one
// that leaves the engine's original behaviour untouched.
//
// tools/check_thunk_parity.py enforces that the .asm and .s spellings of these
// thunks stay in sync; see docs/clang-cl-linux-build.md.

#include <atomic>
#include <cstdint>

extern void Log(const char* fmt, ...);

namespace cvr::thunk {

// Total exceptions swallowed at a thunk boundary since load. Non-zero means a
// callback is throwing, which is a bug in the callback and not in the engine.
inline std::atomic<uint32_t>& ExceptionCount() noexcept
{
    static std::atomic<uint32_t> count{0};
    return count;
}

inline void ReportSwallowedException(const char* name) noexcept
{
    ExceptionCount().fetch_add(1, std::memory_order_relaxed);
    Log("[thunk] %s threw; contained at the detour boundary\n", name);
}

} // namespace cvr::thunk