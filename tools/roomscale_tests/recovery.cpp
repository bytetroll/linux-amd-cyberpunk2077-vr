#include "Runtimes/RoomscaleMovement.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace cvr::roomscale;
namespace {
void Check(bool value, const char* message) {
    if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
void Near(float actual, float expected, const char* message) {
    Check(std::isfinite(actual) && std::abs(actual-expected)<.0001f,message);
}
struct Run {
    Movement movement;
    uint64_t now=1000000,sequence=0,origin=1;
    float entity=0;
    bool discontinuity=false;
    Step Tick(float x,bool valid=true,bool allowed=true,bool stale=false,
              float runtimeX=std::numeric_limits<float>::quiet_NaN()) {
        now+=10000;
        Sample pose{{x,0},++sequence,origin,stale?now-Movement::MaxAgeUs-1:now,valid};
        pose.runtimeHead={runtimeX,0};pose.runtimeHeadKnown=std::isfinite(runtimeX);
        auto step=movement.Begin(pose,now,.01f,1,0,allowed,.10f);
        movement.Complete(step);
        entity+=step.world.x;
        discontinuity|=step.reason==Reason::Discontinuity;
        if(origin==1 && !discontinuity)Near(entity+x-movement.Consumed(origin).x,x,"body correction changed the camera path");
        return step;
    }
    void Start(bool rawKnown=false) {
        Tick(0,true,true,false,rawKnown?0:std::numeric_limits<float>::quiet_NaN());
        for(int i=0;i<8;++i)Tick(.15f,true,true,false,rawKnown?.15f:std::numeric_limits<float>::quiet_NaN());
        const float remaining=.15f-movement.Consumed(origin).x;
        Check(remaining>0 && remaining<.10f,"fixture must pause active follow inside the free zone");
    }
};
void BriefRecovery(bool stale) {
    Run run;run.Start();
    const float consumed=run.movement.Consumed(1).x;
    for(int i=0;i<3;++i) {
        const auto step=run.Tick(.15f,!stale?false:true,true,stale);
        Near(step.world.x,0,"tracking loss emitted a movement request");
        Near(run.movement.Consumed(1).x,consumed,"tracking loss consumed pending movement");
    }
    Near(run.Tick(.15f).world.x,0,"recovery must seed before resuming known movement");
    const auto next=run.Tick(.15f);
    Check(next.world.x>0 && next.world.x<.01f,"known follow did not restart smoothly");
    for(int i=0;i<80;++i)run.Tick(.15f);
    Near(run.movement.Consumed(1).x,.15f,"brief tracking loss stranded the accepted follow residual");
}
void Cancellation() {
    for(int reason=0;reason<5;++reason) {
        Run run;run.Start();
        float head=.15f;
        if(reason==0) { for(int i=0;i<30;++i)run.Tick(head,false); }
        if(reason==1)run.Tick(head,true,false);
        if(reason==2)run.movement.Suspend();
        if(reason==3) { ++run.origin;head=0; }
        if(reason==4) { run.Tick(head,false);head+=1; }
        Near(run.Tick(head).world.x,0,"cancelled follow moved on baseline");
        const auto consumed=run.movement.Consumed(run.origin).x;
        for(int i=0;i<80;++i)Near(run.Tick(head).world.x,0,"cancelled follow replayed old movement");
        Near(run.movement.Consumed(run.origin).x,consumed,"cancelled follow altered the correction ledger");
    }
    Run jump;jump.Start();
    Check(jump.Tick(1.15f).reason==Reason::Discontinuity,"large live tracking jump was not rejected");
    for(int i=0;i<80;++i)Near(jump.Tick(1.15f).world.x,0,"discontinuity retained pending movement");
}
void FilteredDiscontinuity() {
    Run run;run.Start(true);
    const auto position=run.entity,view=.15f-run.movement.Consumed(1).x;
    run.Tick(.15f,false);
    // The filtered head has moved only3cm, while raw tracking teleported1m.
    Check(run.Tick(.18f,true,true,false,1.15f).reason==Reason::Discontinuity,
          "raw tracking jump hid behind filtered recovery");
    Near(.18f-run.movement.Consumed(1).x,view,"recovery teleport changed camera span");
    run.Tick(.18f,false);
    // A second rejection while rebasing must not turn the tail into walking.
    for(float x:{.30f,.50f,.70f,.90f,1.15f}) {
        auto step=run.Tick(x,true,true,false,1.15f);
        Check(step.reason==Reason::Discontinuity,"tracking recovery lost the filtered jump tail");
        Near(step.world.x,0,"filtered tracking jump moved the body");
        Near(x-run.movement.Consumed(1).x,view,"filtered jump changed camera span");
    }
    for(int i=0;i<80;++i)Near(run.Tick(1.15f,true,true,false,1.15f).world.x,0,"filtered jump retained pending movement");
    Near(run.entity,position,"tracking jump replayed body follow");
}
}
int main() {
    BriefRecovery(false);
    BriefRecovery(true);
    Cancellation();
    FilteredDiscontinuity();
    std::cout << "roomscale recovery: invalid/stale recovery and cancellation cases passed\n";
}
