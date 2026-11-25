#pragma once
#include "../176/SpiceStream.h"
#include <memory>

namespace ua176 {

class Live176 {
public:
    Live176();
    ~Live176();
    bool prepare(double rate, const Controls&);
    bool setControls(const Controls&);
    bool step(double sourceVolts, double& outputVolts, double& cv) noexcept;
    double bPlus() const noexcept;
    double regulated() const noexcept;
private:
    struct Circuit;
    std::unique_ptr<Circuit> circuit;
};

}
