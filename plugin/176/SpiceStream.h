#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>

namespace ua176 {
struct Controls {
    float input = 10, output = 3.16227766f;
    float inputVernier = 0, outputVernier = 0;
    float tap = .29f, biasResistance = 6800;
    float attack = .5f, release = .8f, limiting = 1;
    bool doubleTiming = false;
};

class SpiceStream {
public:
    SpiceStream();
    ~SpiceStream();
    SpiceStream(const SpiceStream&) = delete;
    SpiceStream& operator=(const SpiceStream&) = delete;
    bool start(const std::filesystem::path& library, double rate);
    void stop();
    bool submit(float input, const Controls& controls) noexcept;
    bool read(std::uint64_t index, float& value, float& cv) noexcept;
    bool ready() const noexcept;
    bool failed() const noexcept;
    std::uint64_t framesProduced() const noexcept;
    static constexpr int latency = 8192;
    static constexpr double fullScalePeak = 8.70596066; // +18 dBu sine.
private:
    struct Impl;
    std::unique_ptr<Impl> state;
};
}
