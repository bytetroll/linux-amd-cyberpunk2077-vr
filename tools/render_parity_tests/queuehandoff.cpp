#include "Runtimes/QueueHandoff.hpp"
#include <stdexcept>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using cvr::xr::QueueHandoff;
void Check(bool condition){if(!condition)throw std::runtime_error("queue handoff expectation failed");}
int main() {
    using Events=std::vector<std::string>;
    unsigned cases=0;
    {
        QueueHandoff h;Events events;uint64_t signaled=0;
        auto signal=[&](uint64_t v){signaled=v;events.push_back("runtime signal "+std::to_string(v));return true;};
        auto wait=[&](uint64_t v){Check(v==signaled);events.push_back("game wait "+std::to_string(v));return true;};
        Check(h.Acquire(signal,wait));Check(h.Acquire(signal,wait));
        Check((events==Events{"runtime signal 1","game wait 1","runtime signal 2","game wait 2"}));++cases;
        events.push_back("write eyes");
        Check(h.Submitted([&]{events.push_back("game signal eyes");return true;}));
        // Both eye releases reuse the exact producer completion, despite newer game work.
        events.push_back("unrelated game frame");
        for(unsigned eye=0;eye<2;++eye)Check(h.Release(true,
            [&]{events.push_back("runtime wait eyes");return true;},
            [&]{events.push_back("release eye");return true;}));
        Check((Events(events.begin()+4,events.end())==Events{"write eyes","game signal eyes","unrelated game frame",
            "runtime wait eyes","release eye","runtime wait eyes","release eye"}));++cases;
    }
    {
        QueueHandoff h;unsigned waits=0,releases=0;
        Check(h.Release(false,[&]{++waits;return false;},[&]{++releases;return true;}));
        Check(waits==0 && releases==1);++cases;
    }
    for(bool signalFails:{true,false}) {
        QueueHandoff h;unsigned waits=0,submissions=0,releases=0;
        Check(!h.Acquire([&](uint64_t){return !signalFails;},[&](uint64_t){++waits;return false;}));
        Check(h.Failed() && waits==unsigned(!signalFails));
        Check(!h.Submitted([&]{++submissions;return true;}));
        Check(!h.Release(true,[&]{++waits;return true;},[&]{++releases;return true;}));
        Check(submissions==0 && releases==0 && waits==unsigned(!signalFails));++cases;
    }
    {
        QueueHandoff h;unsigned waits=0,releases=0;
        Check(!h.Submitted([]{return false;}));
        Check(!h.Release(true,[&]{++waits;return true;},[&]{++releases;return true;}));
        Check(h.Failed() && !waits && !releases);++cases;
    }
    {
        QueueHandoff h;unsigned releases=0;
        Check(h.Submitted([]{return true;}));
        Check(!h.Release(true,[]{return false;},[&]{++releases;return true;}));
        Check(h.Failed() && !releases);++cases;
    }
    {
        QueueHandoff h;unsigned signals=0;
        Check(!h.Release(false,[]{return true;},[]{return false;}));
        Check(!h.Acquire([&](uint64_t){++signals;return true;},[](uint64_t){return true;}));
        Check(h.Failed() && !signals);++cases;
    }
    {
        QueueHandoff h;h.Fail();unsigned calls=0;
        Check(!h.Release(false,[]{return true;},[&]{++calls;return true;}));
        Check(!calls);++cases;
    }
    std::cout<<"PASS "<<cases<<" queue handoff cases\n";
}
