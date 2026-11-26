#pragma once

#include "Live176.h"

namespace ua176 {

class Oversampled176 {
public:
    static constexpr int latency = 168;
    static bool supportsOversampling(double rate) noexcept;
    bool prepare(double rate, int factor, const LiveControls&);
    void resetResampling() noexcept;
    bool setControls(const LiveControls&);
    bool step(double input, double& output, double& cv) noexcept;

private:
    struct Halfband {
        std::array<double, 256> history{};
        unsigned position = 0;
        std::array<double, 2> up(double input) noexcept;
        double down(double a, double b) noexcept;
    };

    Live176 circuit;
    Halfband upOuter, upInner, downOuter, downInner;
    std::array<double, latency> delay{};
    std::array<double, 128> source{};
    unsigned position = 0, sourcePosition = 0;
    const double* correction = nullptr;
    int mode = 0, extraDelay = 0;
    bool ready = false;
};

}
