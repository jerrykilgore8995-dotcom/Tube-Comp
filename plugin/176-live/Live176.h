#pragma once
#include <array>
#include <memory>

namespace ua176 {

struct RatioSetting {
    float tap;
    float biasResistance;
};

// Effective T3 tap fractions and schematic 6AL5 bias returns in front-panel order.
inline constexpr std::array<RatioSetting, 4> ratioSettings{{
    {.235f, 1000}, {.44f, 6800}, {.65f, 20000}, {1.f, 47000}
}};

struct LiveControls {
    float input = 10, output = 3.16227766f;
    float tap = ratioSettings[1].tap, biasResistance = ratioSettings[1].biasResistance;
    float attack = .5f, release = .8f, limiting = 1;
    float sidechainHz = 0;
    bool doubleTiming = false;
    bool slowAttack = false;
    bool suppressCvRipple = true;
};

inline float dryMix(float wet, float dry, float amount) noexcept
{
    if (amount <= 0) return wet;
    // The modeled wet path inverts in the passband.
    dry = -dry;
    if (amount >= 1) return dry;
    return wet + amount * (dry - wet);
}

class Live176 {
public:
    static constexpr int phaseLatency = 32;
    static int latency(double rate) noexcept { return rate == 44100 ? phaseLatency : 0; }
    Live176();
    ~Live176();
    bool prepare(double rate, const LiveControls&);
    bool setControls(const LiveControls&);
    bool step(double sourceVolts, double& outputVolts, double& cv) noexcept;
    double bPlus() const noexcept;
    double regulated() const noexcept;
    double interstage() const noexcept;
#ifdef UA176_PHASE_REFERENCE
    static bool exportPhaseReference(const char*, double bias);
#endif
private:
    struct Circuit;
    std::unique_ptr<Circuit> circuit;
};

}
