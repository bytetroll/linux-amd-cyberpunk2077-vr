#pragma once

#include <cmath>
#include <cstdint>

namespace cvr::input {

struct NativeMenuTriggerResult {
    bool confirm{};
    bool consumeTrigger{};
};

// Native menus use gamepad A for click/skip, while the VR trigger normally
// reaches the game as RT. Require a released trigger on entry and retain a
// held press until release on exit, so selecting Continue cannot also fire.
// The owner serializes Update; this policy does not read a runtime or clock.
class NativeMenuTrigger {
public:
    // A missing sample is not a release. Keep the trigger blocked until an
    // actual valid sample shows it released, including after loading/tracking gaps.
    void Suspend() {
        m_menu = m_armed = m_down = false;
        m_blocked = true;
    }

    NativeMenuTriggerResult Update(bool menu, float trigger, bool available = true) {
        if (!available) {
            Suspend();
            return {false, true};
        }
        if (!std::isfinite(trigger) || trigger < 0.f || trigger > 1.f) {
            m_menu = menu;
            m_armed = m_down = false;
            m_blocked = true;
            return {false, true};
        }
        const bool released = trigger <= .30f;
        if (!menu) {
            if (m_menu && !released) m_blocked = true;
            m_menu = m_armed = m_down = false;
            if (released) m_blocked = false;
            return {false, m_blocked};
        }
        if (!m_menu) {
            m_menu = true;
            m_armed = released;
            m_down = false;
        }
        if (released) {
            m_armed = true;
            m_down = m_blocked = false;
        } else if (m_armed) {
            m_down = m_down ? trigger > .35f : trigger >= .65f;
        }
        return {m_down, true};
    }

private:
    bool m_menu{}, m_armed{}, m_down{}, m_blocked{};
};

inline uint16_t NativeMenuButtons(uint16_t buttons, NativeMenuTriggerResult trigger) {
    constexpr uint16_t gamepadA = 0x1000;
    return buttons | (trigger.confirm ? gamepadA : 0);
}

} // namespace cvr::input
