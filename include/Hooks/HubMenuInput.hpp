#pragma once
#include <cmath>
#include <cstdint>

namespace cvr::input {
// A dedicated pressure input emits a short native Start press. Keeping Start
// held opens the map, so a resting thumb must never extend the menu pulse.
class HubMenuPress {
public:
    bool Update(bool enabled,bool available,float force,std::uint64_t now) {
        if(!enabled || !available || !std::isfinite(force) || force<0.f || force>1.f || now<m_last) {
            m_armed=m_down=m_pulse=false;m_last=now;return false;
        }
        m_last=now;
        if(m_pulse && now-m_started>=120)m_pulse=false;
        if(force<=.20f) { m_down=false;m_armed=true; }
        else if(force>=.55f && !m_down) {
            m_down=true;
            if(m_armed && !m_pulse) { m_started=now;m_pulse=true; }
            m_armed=false;
        }
        return m_pulse;
    }
private:
    bool m_armed{},m_down{},m_pulse{};
    std::uint64_t m_started{},m_last{};
};
} // namespace cvr::input
