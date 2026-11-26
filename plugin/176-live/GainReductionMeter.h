#pragma once

#include "Oversampled176.h"
#include <atomic>
#include <limits>
#include <thread>

namespace ua176 {

// Runs the uncompressed comparison circuit outside the audio callback.
// submit() and disconnect() are called by the audio thread only.
class GainReductionMeter {
public:
    static constexpr int chunkSize = 512;
    static constexpr float unavailable = -1.f;

    ~GainReductionMeter();
    void start();
    void stop();
    bool isEnabled() const noexcept { return enabled.load(std::memory_order_relaxed); }
    float read() const noexcept;
    void disconnect() noexcept;
    void submit(double rate, int factor, const LiveControls&, const double* input,
                const double* wet, int size) noexcept;

private:
    void run();
    static constexpr unsigned capacity = 16;
    struct Chunk {
        std::array<double, chunkSize> input, wet;
        LiveControls controls;
        double rate;
        int factor, size;
        unsigned generation;
    };
    // One producer and one consumer. The generation also invalidates stale output.
    std::array<Chunk, capacity> queue;
    std::atomic<unsigned> writeIndex{0}, readIndex{0}, generation{0}, reportedGeneration{1};
    std::atomic<bool> enabled{false};
    std::atomic<float> reduction{unavailable};
    bool connected = false;
    std::thread worker;
};
}
