#pragma once
#include <cstdint>

namespace cvr::xr {
// A failed submission never creates a wait for a value that was not signaled.
// The owner serializes calls on its XR submission thread.
class QueueHandoff {
public:
    bool Failed() const { return failed; }
    void Fail() { failed = true; }
    template<class Signal, class Wait>
    bool Acquire(Signal&& signal, Wait&& wait) {
        if(failed)return false;
        const auto value=++acquired;
        if(!signal(value) || !wait(value)){failed=true;return false;}
        return true;
    }
    template<class Signal>
    bool Submitted(Signal&& signal) {
        if(failed)return false;
        if(!signal()){failed=true;return false;}
        return true;
    }
    template<class Wait, class ReleaseCall>
    bool Release(bool written, Wait&& wait, ReleaseCall&& release) {
        if(failed)return false;
        if((written && !wait()) || !release()){failed=true;return false;}
        return true;
    }
private:
    uint64_t acquired=0;
    bool failed=false;
};
}
