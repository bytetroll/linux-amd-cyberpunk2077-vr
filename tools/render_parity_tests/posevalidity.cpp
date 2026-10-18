#include "Utils/PoseValidity.hpp"
#include <iostream>
#include <stdexcept>
#include <limits>
#include <cmath>

void Check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main()try {
    using namespace cvr::pose;
    const float nan=std::numeric_limits<float>::quiet_NaN();
    const float inf=std::numeric_limits<float>::infinity();

    // The identity is what a correctly defaulted pose looks like, and it must pass untouched.
    Check(Valid(Rigid{}),"the default Rigid is not a submittable pose");
    Check(Classify(Rigid{})==Verdict::Ok,"identity misclassified");

    // THE REGRESSION ANCHOR. `XrPosef x{}` zero-initialises the quaternion, which is NOT the
    // identity -- it has no norm at all. That is the shape the frame loop declares its pose
    // arrays in (monoPoses[2]{}), so a capture that never fills them submits this.
    const Rigid zeroQuat{0,0,0,0, 0,0,0};
    Check(Classify(zeroQuat)==Verdict::ZeroQuat,"zero quaternion accepted as a pose");
    Check(!Valid(zeroQuat),"zero quaternion passed validity");
    Rigid rescueZero=zeroQuat;
    Check(!Renormalize(rescueZero),"zero quaternion was 'renormalised' into a direction");

    // NaN and infinity, in every one of the seven slots, orientation and position alike.
    for(int slot=0;slot<7;++slot) {
        for(float bad:{nan,inf,-inf}) {
            Rigid p{};
            float* f=&p.ox;   // ox,oy,oz,ow,px,py,pz are laid out in this order
            f[slot]=bad;
            Check(Classify(p)==Verdict::NotFinite,"a non-finite component was not caught");
            Rigid r=p;
            Check(!Renormalize(r),"a non-finite pose was 'repaired'");
        }
    }

    // Drift is repaired, not rejected: a quaternion a few ULP off unit is ordinary wear from
    // composition and must not cost a frame.
    Rigid drifted{0.0f,0.70712f,0.0f,0.70712f, 0,0,0};
    Check(Classify(drifted)==Verdict::Ok || Classify(drifted)==Verdict::NotUnit,"drift misread");
    Rigid repaired=drifted;
    Check(Renormalize(repaired),"a merely drifted quaternion could not be renormalised");
    Check(Valid(repaired),"renormalising did not produce a valid pose");
    Check(std::fabs(QuatNorm(repaired)-1.0f)<=kUnitTolerance,"renormalised norm still off unit");

    // A badly non-unit quaternion is still repairable -- scaling is scaling.
    Rigid big{0,0,0,5.0f, 0,0,0};
    Check(Classify(big)==Verdict::NotUnit,"a norm of 5 was not flagged");
    Check(Renormalize(big) && Valid(big),"a large-norm quaternion was not rescued");
    Check(std::fabs(big.ow-1.0f)<1e-6f,"renormalising changed the direction");

    // Position is checked too: a pose composed against a garbage origin is finite but absurd,
    // and submitting it puts the layer somewhere the scene is not.
    Check(Classify(Rigid{0,0,0,1, 5000.0f,0,0})==Verdict::PositionWild,"a 5 km offset was accepted");
    Check(Classify(Rigid{0,0,0,1, 0,0,-999.0f})==Verdict::Ok,"a legal large offset was rejected");

    // ---- the Gate -------------------------------------------------------------------------
    // A good pose passes and is latched.
    Gate g;
    Rigid good{0,0,0,1, 0.0f,1.7f,0.0f};
    Verdict why{};
    Check(g.Admit(good,&why) && why==Verdict::Ok,"a good pose was not admitted");
    Check(g.passed==1 && g.haveLastGood,"a good pose was not latched");
    Check(!g.Degraded(),"the gate reported degradation on a clean pose");

    // A broken one is replaced by it, and the REPORTED verdict is the fault as supplied --
    // not the verdict of the substitute, or the log names the wrong thing.
    Rigid broken=zeroQuat;
    Check(g.Admit(broken,&why),"the gate refused although it held a good pose");
    Check(why==Verdict::ZeroQuat,"the gate reported the substitute's verdict, not the fault");
    Check(broken.ow==1.0f && broken.py==1.7f,"the substituted pose is not the last good one");
    Check(g.replaced==1 && g.Degraded(),"substitution was not counted as degradation");

    // THE SESSION-ENDING CASE, and the whole point of the exercise: the first frame is bad and
    // there is no last-good to fall back on. The gate must REFUSE, so the caller skips the
    // layer instead of handing the runtime a pose that rejects the entire xrEndFrame.
    Gate cold;
    Rigid firstBad{0,0,0,0, nan,0,0};
    Check(!cold.Admit(firstBad,&why),"a cold gate admitted an unusable pose");
    Check(why==Verdict::NotFinite,"cold refusal reported the wrong verdict");
    Check(cold.refused==1 && !cold.haveLastGood,"a refusal was latched as good");

    // Recovery: once a good pose arrives the gate stops substituting.
    Rigid recovered{0,0.3826834f,0,0.9238795f, 0,1.7f,0};
    Check(cold.Admit(recovered,&why) && why==Verdict::Ok,"the gate did not recover");
    Check(cold.passed==1 && cold.haveLastGood,"recovery was not latched");

    // Repair is latched as good too, so the next bad frame falls back to the REPAIRED pose
    // rather than to something older.
    Gate r2;
    Rigid near{0,0.70712f,0,0.70712f, 0,0,0};
    const bool nearWasDrifted=Classify(near)==Verdict::NotUnit;
    Check(r2.Admit(near,&why),"a drifted pose was not admitted");
    if(nearWasDrifted)Check(r2.repaired==1,"a drifted pose was not counted as repaired");
    Check(r2.haveLastGood && Valid(r2.lastGood),"an invalid pose was latched as last-good");

    // The gate never latches anything it would itself reject -- the invariant that keeps a
    // substitution from propagating a fault forward for the rest of the session.
    Gate inv;
    for(float bad:{nan,inf,0.0f}) {
        Rigid p{bad,bad,bad,bad, 0,0,0};
        inv.Admit(p,&why);
        if(inv.haveLastGood)Check(Valid(inv.lastGood),"the gate latched an invalid last-good");
    }

    // ---- THE PAIR. What the per-eye gate above is structurally unable to catch. ----
    const float deg=3.14159265358979f/180.0f;
    auto axisAngle=[&](float ax,float ay,float az,float degrees){
        const float h=degrees*deg*0.5f;
        return Rigid{ax*std::sin(h),ay*std::sin(h),az*std::sin(h),std::cos(h), 0,0,0};
    };

    // THE HEADLINE. A dropped ow is a unit quaternion and a 180-degree rotation at the same
    // time: individually submittable, and the reason the headset showed a second panel behind
    // the player with the picture mirrored and upside down.
    const Rigid identity{};
    const Rigid droppedW{1,0,0,0, 0,0,0};          // 180 about X -- what ow==0 always means
    Check(Classify(droppedW)==Verdict::Ok,"the dropped-ow pose is not individually valid -- "
        "if this fails the premise of the pair check is wrong");
    Check(Valid(droppedW),"dropped-ow rejected by the per-eye gate, which it must not be");
    { Gate g; Rigid p=droppedW; Verdict v{};
      Check(g.Admit(p,&v) && v==Verdict::Ok && g.passed==1,
          "the per-eye gate did not pass the dropped-ow pose -- the pair check is what must "
          "catch it, and this test exists to prove the gate alone cannot"); }
    float split=-1.0f;
    Check(ClassifyPair(identity,droppedW,&split)==PairVerdict::Opposed,
        "a 180-degree second eye was not classified as opposed");
    Check(std::fabs(split-180.0f)<0.01f,"the opposed split did not measure 180 degrees");

    // ow==0 is 180 degrees about ANY axis, not just X.
    for(const Rigid& q:{Rigid{1,0,0,0, 0,0,0},Rigid{0,1,0,0, 0,0,0},Rigid{0,0,1,0, 0,0,0},
                        Rigid{0.7071068f,0,0.7071068f,0, 0,0,0}}) {
        Check(Valid(q),"an ow==0 unit quaternion was not individually valid");
        float s=-1.0f;
        Check(ClassifyPair(identity,q,&s)==PairVerdict::Opposed,"an ow==0 pair was not opposed");
        Check(std::fabs(s-180.0f)<0.05f,"an ow==0 pair did not measure 180 degrees");
    }

    // The double cover: q and -q are the SAME rotation and must read as zero apart, or every
    // frame where the runtime happened to hand back the negated form would log a false fault.
    //
    // Tolerance note, measured not guessed: acos has an infinite derivative at 1, so one ULP
    // of float error in the dot product of two NEARLY IDENTICAL orientations comes out as
    // about 0.04 degrees of angle (worst observed here: 0.0396 at a 30-degree axis-angle).
    // Constructed angles away from zero are accurate to ~1.5e-5 degrees. So "the same
    // rotation" is asserted to 0.05, not tighter -- and 0.04 degrees of noise is nothing
    // against a 15-degree gate, which is why this is a test tolerance and not a bug.
    for(const Rigid& q:{axisAngle(0,1,0,37.0f),axisAngle(1,0,0,120.0f),identity}) {
        const Rigid neg{-q.ox,-q.oy,-q.oz,-q.ow, 0,0,0};
        Check(SplitDegrees(q,neg)<0.05f,"q and -q were not recognised as the same rotation");
        Check(ClassifyPair(q,neg)==PairVerdict::Agree,"q vs -q was reported as a split pair");
    }

    // Real cant passes. The widest reported panel asymmetry is 4 degrees; a gate that flagged
    // that would fire on every canted headset forever.
    for(float cant:{0.0f,1.0f,2.0f,4.0f,10.0f}) {
        Check(ClassifyPair(identity,axisAngle(0,1,0,cant))==PairVerdict::Agree,
            "a physically real headset cant was reported as a broken pair");
    }
    // ...and a genuine divergence does not pass, without being mistaken for the 180 signature.
    for(float bad:{20.0f,45.0f,90.0f,140.0f}) {
        float s=-1.0f;
        Check(ClassifyPair(identity,axisAngle(0,1,0,bad),&s)==PairVerdict::Diverged,
            "a diverged pair was not caught, or was misreported as opposed");
        Check(std::fabs(s-bad)<0.05f,"the measured split did not match the constructed angle");
    }

    // Reconcile takes the orientation and LEAVES THE POSITION, because the position is the IPD
    // baseline -- collapsing that too would put both eyes at one point and flatten the stereo.
    Rigid secondEye=droppedW; secondEye.px=0.032f; secondEye.py=1.6f; secondEye.pz=-0.1f;
    PairVerdict pv{}; float rs=-1.0f;
    Check(ReconcilePair(identity,secondEye,&pv,&rs),"reconcile did not report an intervention");
    Check(pv==PairVerdict::Opposed && std::fabs(rs-180.0f)<0.01f,"reconcile misreported the fault");
    Check(secondEye.ox==identity.ox && secondEye.oy==identity.oy &&
          secondEye.oz==identity.oz && secondEye.ow==identity.ow,
        "reconcile did not adopt the first eye's orientation");
    Check(secondEye.px==0.032f && secondEye.py==1.6f && secondEye.pz==-0.1f,
        "reconcile destroyed the second eye's position -- that is the IPD baseline");
    Check(Valid(secondEye),"reconcile produced a pose that is not submittable");
    Check(!ReconcilePair(identity,secondEye,&pv,&rs) && pv==PairVerdict::Agree,
        "reconcile is not idempotent");

    // An agreeing pair is left exactly alone: no silent rewriting of a good second eye.
    Rigid healthy=axisAngle(0,1,0,3.0f); healthy.px=0.032f;
    const Rigid before=healthy;
    Check(!ReconcilePair(identity,healthy,&pv,&rs),"reconcile intervened on a healthy pair");
    Check(healthy.ox==before.ox && healthy.oy==before.oy && healthy.oz==before.oz &&
          healthy.ow==before.ow && healthy.px==before.px,"reconcile mutated a healthy pair");

    // Symmetry and self-agreement -- the split is a property of the pair, not of the order.
    for(float a:{5.0f,30.0f,170.0f,180.0f}) {
        const Rigid q=axisAngle(0,1,0,a);
        Check(std::fabs(SplitDegrees(identity,q)-SplitDegrees(q,identity))<0.01f,
            "the split angle is not symmetric");
        Check(SplitDegrees(q,q)<0.05f,"a pose did not agree with itself");
    }

    std::cout<<"PASS zero/NaN/inf in all seven slots, unit tolerance, drift repair, wild "
               "position, gate pass/repair/replace/refuse, cold-start refusal, recovery, "
               "the never-latch-invalid invariant, and the stereo pair: dropped-ow is valid "
               "yet 180 apart, q/-q double cover, real cant admitted, divergence caught, "
               "reconcile keeps the IPD baseline\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
