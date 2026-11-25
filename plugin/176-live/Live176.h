#pragma once
#include <memory>

namespace ua176 {

struct LiveControls {
    float input = 10, output = 3.16227766f;
    float tap = .29f, biasResistance = 6800;
    float attack = .5f, release = .8f, limiting = 1;
    float sidechainHz = 0;
    bool doubleTiming = false;
};

inline float dryMix(float wet, float dry, float amount) noexcept
{
    if (amount <= 0) return wet;
    if (amount >= 1) return dry;
    return wet + amount * (dry - wet);
}

class Live176 {
public:
    Live176();
    ~Live176();
    bool prepare(double rate, const LiveControls&);
    bool setControls(const LiveControls&);
    bool step(double sourceVolts, double& outputVolts, double& cv) noexcept;
    double bPlus() const noexcept;
    double regulated() const noexcept;
private:
    struct Circuit;
    std::unique_ptr<Circuit> circuit;
};

}
