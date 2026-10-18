#pragma once
// WHAT A RUNTIME WILL NOT TELL YOU WHEN IT REJECTS YOUR FRAME.
//
// xrEndFrame returns XR_ERROR_POSE_INVALID (-39) for the WHOLE submission if any layer it
// carries holds a pose whose orientation is not a unit quaternion, or whose numbers are not
// finite. It does not say which layer, which eye, or which of the seven floats was wrong, and
// the port had no logging of the submitted pose at all -- so a session that failed 18,744
// times in a row (AMD/Index/Proton, 2026-10-16) could not name the bad field even once.
//
// Worse, the failure is ABSORBING. The submit path falls through to a layerCount-0 xrEndFrame,
// which is a legal empty frame, so the runtime holds its last composition and the session
// stays alive looking healthy from the outside: 90 cycles a second, zero pictures. The headset
// shows one frozen frame reprojected into a flat panel hanging in the void, forever.
//
// Hence this header. It is deliberately free of <openxr/openxr.h> so the arithmetic can be
// built and tested on a machine with no OpenXR SDK and no MSVC -- see
// tools/render_parity_tests/posevalidity.cpp, which is the only part of this fix that has
// actually been RUN. The frame loop converts XrPosef to Rigid at the call site; that is seven
// float copies and keeps the testable part honest.
#include <cmath>
#include <cstdint>

namespace cvr::pose {

// Seven floats in the same order and meaning as XrPosef, with the identity as the default --
// NOT the all-zero orientation that a `XrPosef x{}` gives you, which is itself one of the
// invalid poses this header exists to catch.
struct Rigid {
    float ox{0.0f}, oy{0.0f}, oz{0.0f}, ow{1.0f};
    float px{0.0f}, py{0.0f}, pz{0.0f};
};

enum class Verdict : uint32_t {
    Ok = 0,
    NotFinite,      // a NaN or an infinity among the seven numbers
    ZeroQuat,       // orientation is all zeros -- a default-constructed XrPosef that was never filled
    NotUnit,        // finite and non-zero, but outside the runtime's normalisation tolerance
    PositionWild,   // finite, but a distance from the reference space no tracking volume produces
};

inline const char* VerdictName(Verdict v) {
    switch (v) {
        case Verdict::Ok:           return "ok";
        case Verdict::NotFinite:    return "not-finite";
        case Verdict::ZeroQuat:     return "zero-quaternion";
        case Verdict::NotUnit:      return "not-unit";
        case Verdict::PositionWild: return "position-wild";
    }
    return "unknown";
}

// OpenXR requires |q| == 1. Runtimes accept a little drift, and quaternions that have been
// through a few compositions legitimately sit a few ULP off, so the gate is loose enough not
// to reject arithmetic that is merely tired. Anything outside it is a bug, not rounding.
inline constexpr float kUnitTolerance = 1.0e-3f;
// Below this the orientation carries no direction at all and renormalising would divide by
// something meaningless. In practice this is the all-zero quaternion.
inline constexpr float kZeroQuatEpsilon = 1.0e-6f;
// A head does not get a kilometre from its reference space. This is not a plausibility check
// on the player, it catches a pose that has been composed against a garbage origin -- which is
// how a recenter leaks into the layer and puts the camera somewhere the scene is not.
inline constexpr float kMaxPositionM = 1000.0f;

// Written out rather than using std::isfinite directly: the plugin is built with optimisations
// that may imply -ffast-math-like assumptions, under which isfinite can be folded to `true`.
// A self-comparison and a magnitude bound survive that.
inline bool Finite(float v) {
    return (v == v) && (v < 3.0e38f) && (v > -3.0e38f);
}

inline float QuatNorm(const Rigid& p) {
    return std::sqrt(p.ox * p.ox + p.oy * p.oy + p.oz * p.oz + p.ow * p.ow);
}

inline Verdict Classify(const Rigid& p) {
    if (!Finite(p.ox) || !Finite(p.oy) || !Finite(p.oz) || !Finite(p.ow) ||
        !Finite(p.px) || !Finite(p.py) || !Finite(p.pz)) {
        return Verdict::NotFinite;
    }
    const float n = QuatNorm(p);
    if (n < kZeroQuatEpsilon)                      return Verdict::ZeroQuat;
    if (std::fabs(n - 1.0f) > kUnitTolerance)      return Verdict::NotUnit;
    if (std::fabs(p.px) > kMaxPositionM ||
        std::fabs(p.py) > kMaxPositionM ||
        std::fabs(p.pz) > kMaxPositionM)           return Verdict::PositionWild;
    return Verdict::Ok;
}

inline bool Valid(const Rigid& p) { return Classify(p) == Verdict::Ok; }

// Rescue a pose that is only DRIFTED. Returns false for one that is broken in a way
// renormalising cannot fix -- not finite, no direction at all, or positioned off the map --
// because scaling a NaN gives a NaN and scaling the zero quaternion gives nothing at all.
//
// Deliberately narrow: this repairs arithmetic wear, it does not invent an orientation. A pose
// it refuses is the caller's problem, and Gate::Admit below is what has the answer for that.
inline bool Renormalize(Rigid& p) {
    const Verdict v = Classify(p);
    if (v == Verdict::Ok)       return true;
    if (v != Verdict::NotUnit)  return false;
    const float inv = 1.0f / QuatNorm(p);
    p.ox *= inv; p.oy *= inv; p.oz *= inv; p.ow *= inv;
    return Valid(p);
}

// THE THING THAT STOPS ONE BAD FRAME ENDING A SESSION.
//
// Per eye. Every pose on its way to a composition layer goes through Admit, which either
// passes it, repairs it, substitutes the last pose that was good, or -- only when it has never
// seen a good one -- refuses, and the caller must then skip the layer rather than submit
// something the runtime will reject.
//
// Substituting a stale pose is not free: the compositor reprojects against a pose that is one
// or more frames old, which on a head turn is visible as a lurch. That is the right trade. A
// lurch costs one frame; submitting the bad pose costs the session, because xrEndFrame
// rejects the whole submission and the runtime freezes on its last composition.
struct Gate {
    Rigid    lastGood{};
    bool     haveLastGood{false};
    uint64_t passed{0};     // admitted as given
    uint64_t repaired{0};   // renormalised, then admitted
    uint64_t replaced{0};   // unusable, last-good substituted
    uint64_t refused{0};    // unusable and no last-good -- caller must not submit

    // `p` is rewritten in place with what should actually be submitted.
    // `why` reports the verdict of the pose AS SUPPLIED, so the caller can log the real fault
    // rather than the verdict of whatever was substituted for it.
    bool Admit(Rigid& p, Verdict* why = nullptr) {
        const Verdict v = Classify(p);
        if (why) *why = v;
        if (v == Verdict::Ok) {
            lastGood = p; haveLastGood = true; ++passed;  return true;
        }
        if (v == Verdict::NotUnit && Renormalize(p)) {
            lastGood = p; haveLastGood = true; ++repaired; return true;
        }
        if (haveLastGood) {
            p = lastGood; ++replaced; return true;
        }
        ++refused;
        return false;
    }

    // True once the gate has stopped passing poses through untouched, i.e. the pose pipeline
    // upstream is producing garbage and the substitutions are papering over it. The frame loop
    // logs on the transition so the session says WHEN it went wrong, not just that it did.
    bool Degraded() const { return (repaired + replaced + refused) != 0; }
};

// A VALID POSE IS NOT YET A STEREO PAIR, AND THIS IS THE BUG THE GATE ABOVE CANNOT SEE.
//
// Everything above asks one question -- is this pose submittable -- and a quaternion with
// ow == 0 and |(ox,oy,oz)| == 1 answers yes. It is exactly unit, perfectly finite, and
// Classify() returns Ok. It is also, by definition, a rotation of 180 degrees: the half-angle
// form means ow == cos(theta/2), so ow == 0 IS theta == 180. The runtime accepts it and draws
// it, which is the whole problem. On AMD/Index/Proton 2026-10-16 that is what the second eye
// was given, and what the headset showed: the front frame, turned through half a revolution --
// a "back panel" that is the same picture mirrored and upside down, because a 180-degree turn
// is what mirrored-and-upside-down means. One pose was never invalid; it was valid and wrong.
//
// So the pair has to be checked as a pair. Two eyes of one head agree to within the headset's
// cant -- a couple of degrees, 4 on the widest reported panel -- and nothing legitimate puts
// them further apart. The angle between them is the reading that names this fault in one
// number, and 180 is a signature, not a coincidence: it means a dropped ow, not drift.
//
// Separate from Gate on purpose. Gate is per eye and stateful; this is one stateless question
// about two poses, and keeping it so is what lets the test below assert the 180 case directly.

// Real headsets cant their panels outward by a degree or two; the widest reported is 4 degrees
// of frustum asymmetry. Past this the two views are not a stereo pair any more.
inline constexpr float kMaxStereoSplitDeg = 15.0f;
// At or beyond this the pair is not merely disagreeing, it is opposed -- the ow-dropped
// signature. Logged by name so the session says "180" instead of "the eyes disagree".
inline constexpr float kOpposedSplitDeg = 150.0f;

enum class PairVerdict : uint32_t {
    Agree = 0,   // within cant: a real stereo pair
    Diverged,    // too far apart to fuse, but not opposed
    Opposed,     // ~180 degrees: the dropped-ow signature, the "back panel"
};

inline const char* PairVerdictName(PairVerdict v) {
    switch (v) {
        case PairVerdict::Agree:    return "agree";
        case PairVerdict::Diverged: return "diverged";
        case PairVerdict::Opposed:  return "opposed-180";
    }
    return "unknown";
}

// Angle between two orientations, in degrees, folded over the q/-q double cover so that a
// quaternion and its negation -- the same rotation -- read as 0 apart and never as 360.
inline float SplitDegrees(const Rigid& a, const Rigid& b) {
    float dot = a.ox * b.ox + a.oy * b.oy + a.oz * b.oz + a.ow * b.ow;
    if (dot < 0.0f) dot = -dot;
    if (dot > 1.0f) dot = 1.0f;
    return 2.0f * std::acos(dot) * 57.2957795130823f;
}

inline PairVerdict ClassifyPair(const Rigid& first, const Rigid& second, float* splitDeg = nullptr) {
    const float split = SplitDegrees(first, second);
    if (splitDeg) *splitDeg = split;
    if (split >= kOpposedSplitDeg)      return PairVerdict::Opposed;
    if (split >  kMaxStereoSplitDeg)    return PairVerdict::Diverged;
    return PairVerdict::Agree;
}

// Collapse a second eye that has left the pair onto the first eye's ORIENTATION, keeping its
// own position so the IPD baseline survives. Returns true when it had to intervene.
//
// This is a rescue, not a fix: it costs the second eye its share of a canted headset's
// geometry and it hides the upstream fault behind a usable picture. The frame loop logs every
// time it fires precisely so the fault stays visible while the session stays watchable.
inline bool ReconcilePair(const Rigid& first, Rigid& second,
                          PairVerdict* verdict = nullptr, float* splitDeg = nullptr) {
    const PairVerdict v = ClassifyPair(first, second, splitDeg);
    if (verdict) *verdict = v;
    if (v == PairVerdict::Agree) return false;
    second.ox = first.ox; second.oy = first.oy; second.oz = first.oz; second.ow = first.ow;
    return true;
}

}  // namespace cvr::pose
