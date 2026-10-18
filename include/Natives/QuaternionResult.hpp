#pragma once

namespace cvr::natives {

// CET's scratch return storage is max_align_t-aligned (8 on Windows), while
// RED4ext::Quaternion requests 16-byte alignment. Do not dereference that SDK
// type here: clang-cl can combine member assignments into a faulting MOVAPS.
// The RTTI payload remains four floats in i, j, k, r order. Volatile scalar
// stores also keep future compiler changes from assuming SIMD alignment.
inline void StoreQuaternionResult(void* result, float i, float j, float k, float r) noexcept {
    auto* out = static_cast<volatile float*>(result);
    out[0] = i;
    out[1] = j;
    out[2] = k;
    out[3] = r;
}

} // namespace cvr::natives
