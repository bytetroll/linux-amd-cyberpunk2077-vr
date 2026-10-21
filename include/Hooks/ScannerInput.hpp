#pragma once
#include <cmath>
#include <cstdint>

namespace cvr::input {
struct ScannerGripResult { bool active{}, claimed{}; };
struct ScannerDpadResult { bool active{}, consumeUp{}; };

class DpadScannerToggle {
    bool active{},armed{};
public:
    ScannerDpadResult Update(bool enabled,bool gameplay,bool allowed,bool available,bool up) {
        const bool consume=enabled && gameplay;
        if(!enabled || !allowed || !available) {
            active=armed=false;return {false,consume};
        }
        if(!up)armed=true;
        else if(armed){active=!active;armed=false;}
        return {active,consume};
    }
};
inline std::uint16_t ScannerDpadButtons(std::uint16_t vrButtons,bool consumeUp) {
    return consumeUp ? std::uint16_t(vrButtons & ~std::uint16_t{1}) : vrButtons;
}

class DualGripScanner {
    bool active{}, claimed{}, armed{}, left{}, right{}, haveMode{}, previousToggle{};
public:
    ScannerGripResult Update(bool allowed, bool available, bool busy,
                             float leftValue, float rightValue, bool toggle) {
        if (!available || !std::isfinite(leftValue) || !std::isfinite(rightValue) ||
            leftValue<0.f || leftValue>1.f || rightValue<0.f || rightValue>1.f) {
            active = armed = false;
            return {false, claimed};
        }
        left = left ? leftValue > .4f : leftValue >= .7f;
        right = right ? rightValue > .4f : rightValue >= .7f;
        const bool released = !left && !right;
        if (!haveMode || previousToggle != toggle) {
            active = armed = false;
            haveMode = true; previousToggle = toggle;
        }
        if (!allowed) {
            active = armed = false;
            if (released) claimed = false;
            return {false, claimed};
        }
        if (released) {
            armed = true;
            if (!toggle) active = false;
            if (!active) claimed = false;
        }
        if (busy && !active) armed = false;
        if (armed && left && right) {
            active = toggle ? !active : true;
            claimed = true;
            armed = false;
        }
        return {active, claimed};
    }
};

// One arrow-key step per deliberate stick deflection or D-pad press. The
// native D-pad also controls scanner zoom, so navigation uses arrow keys.
class ScannerNavigation {
    bool armed{}, upWas{}, downWas{};
public:
    int Update(bool enabled, float stick, bool up, bool down, float fire=.9f, float rearm=.5f) {
        if (!(fire > .05f && fire <= 1.f)) fire=.9f;
        if (!(rearm >= 0.f && rearm < fire)) rearm=fire*.55f;
        const int digital=(up && !upWas && !down) ? 1 : (down && !downWas && !up) ? -1 : 0;
        upWas=up; downWas=down;
        if (!enabled || !std::isfinite(stick)) { armed=false; return 0; }
        if (std::fabs(stick) < rearm) armed=true;
        if (digital) {armed=false;return digital;}
        if (up || down) {armed=false;return 0;}
        if (armed && std::fabs(stick) > fire) {
            armed=false;
            return stick > 0.f ? 1 : -1;
        }
        return 0;
    }
};

// Gameplay and script grip consumers yield while the scanner owns the chord.
bool ScannerOwnsGrips();
} // namespace cvr::input
