#include "Hooks/NativeMenuInput.hpp"
#include "Hooks/InputPacketSequence.hpp"

#include <iostream>
#include <limits>
#include <stdexcept>

static void Check(bool ok, const char* why) {
    if (!ok) throw std::runtime_error(why);
}

int main() try {
    using namespace cvr::input;
    NativeMenuTrigger input;
    for (float value : {0.f, .5f, 1.f, 0.f}) {
        const auto r = input.Update(false, value);
        Check(!r.confirm && !r.consumeTrigger, "ordinary gameplay trigger changed");
    }
    auto r = input.Update(true, 1.f);
    Check(!r.confirm && r.consumeTrigger, "held trigger selected on menu entry");
    Check(!input.Update(true, .4f).confirm, "partial release armed menu trigger");
    Check(!input.Update(true, .3f).confirm, "release emitted a press");
    Check(!input.Update(true, .64f).confirm, "press threshold too low");
    Check(input.Update(true, .65f).confirm, "menu trigger did not confirm");
    Check(input.Update(true, .5f).confirm, "trigger jitter broke a held click");
    Check(!input.Update(true, .35f).confirm, "trigger did not release below hysteresis threshold");
    Check(!input.Update(true, .5f).confirm, "trigger jitter repeated a click");
    Check(input.Update(true, 1.f).confirm, "second deliberate press was lost");
    r = input.Update(false, 1.f);
    Check(!r.confirm && r.consumeTrigger, "menu exit leaked a held click into fire");
    Check(input.Update(false, .4f).consumeTrigger, "exit suppression released too early");
    Check(!input.Update(false, .3f).consumeTrigger, "released trigger remained suppressed");
    Check(!input.Update(false, 1.f).consumeTrigger, "next gameplay trigger was suppressed");

    input.Update(true, 0.f);
    Check(input.Update(true, 1.f).confirm, "pre-gap menu confirm failed");
    r = input.Update(false, 0.f, false);
    Check(!r.confirm && r.consumeTrigger, "unavailable action supplied a trigger release");
    r = input.Update(false, 1.f);
    Check(!r.confirm && r.consumeTrigger, "missing snapshot fabricated release and leaked fire");
    Check(!input.Update(false, 0.f).consumeTrigger, "valid release after gap did not unblock");
    Check(!input.Update(false, 1.f).consumeTrigger, "post-gap gameplay trigger remained blocked");

    input = {};
    input.Update(true, 0.f);
    input.Update(true, 1.f);
    // Losing menu ownership to the VR overlay follows the same inactive path.
    input.Update(false, 1.f);
    Check(!input.Update(true, 1.f).confirm, "closing overlay caused an unarmed native click");
    input.Update(true, 0.f);
    Check(input.Update(true, 1.f).confirm, "overlay release did not rearm menu");
    for (float invalid : {std::numeric_limits<float>::quiet_NaN(),
                          std::numeric_limits<float>::infinity(), -1.f, 2.f}) {
        r = input.Update(true, invalid);
        Check(!r.confirm && r.consumeTrigger, "invalid trigger generated a click");
        Check(!input.Update(true, 1.f).confirm, "invalid trigger did not require release");
        input.Update(true, 0.f);
        Check(input.Update(true, 1.f).confirm, "valid trigger did not recover");
    }

    for (uint16_t bits : {uint16_t(0), uint16_t(0x1000), uint16_t(0x2000), uint16_t(0xffff)}) {
        Check(NativeMenuButtons(bits, {}) == bits, "original buttons were changed");
        Check(NativeMenuButtons(bits, {true, true}) == (bits | 0x1000), "confirm changed another button");
    }
    // Feed real output through the same packet sequencer as the XInput hook:
    // held A stays stable, releasing/pressing advances the packet every time.
    input = {};
    InputPacketSequence<uint16_t> packets;
    const auto idle = packets.Publish(NativeMenuButtons(0, input.Update(true, 0.f)));
    const auto down = packets.Publish(NativeMenuButtons(0, input.Update(true, 1.f)));
    Check(down != idle, "menu press did not advance XInput packet");
    Check(packets.Publish(NativeMenuButtons(0, input.Update(true, 1.f))) == down,
          "held menu press changed packet");
    const auto up = packets.Publish(NativeMenuButtons(0, input.Update(true, 0.f)));
    Check(up != down, "menu release did not advance XInput packet");
    Check(packets.Publish(NativeMenuButtons(0, input.Update(true, 1.f))) != up,
          "second menu press did not advance XInput packet");
    std::cout << "native menu input: PASS\n";
    return 0;
} catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
}
