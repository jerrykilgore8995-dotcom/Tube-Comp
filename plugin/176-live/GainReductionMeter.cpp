#include "GainReductionMeter.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

namespace ua176 {

GainReductionMeter::~GainReductionMeter() { stop(); }

void GainReductionMeter::start()
{
    if (worker.joinable()) return;
    enabled.store(true);
    worker = std::thread([this] { run(); });
}

void GainReductionMeter::stop()
{
    enabled.store(false);
    if (worker.joinable()) worker.join();
    generation.fetch_add(1, std::memory_order_release);
}

float GainReductionMeter::read() const noexcept
{
    const auto resultGeneration = reportedGeneration.load(std::memory_order_acquire);
    const float result = reduction.load(std::memory_order_relaxed);
    return resultGeneration == generation.load(std::memory_order_acquire) ? result : unavailable;
}

void GainReductionMeter::disconnect() noexcept
{
    if (!connected) return;
    generation.fetch_add(1, std::memory_order_release);
    connected = false;
}

void GainReductionMeter::submit(double rate, int factor, const LiveControls& controls,
                                const double* input, const double* wet, int size) noexcept
{
    const auto write = writeIndex.load(std::memory_order_relaxed);
    if (write - readIndex.load(std::memory_order_acquire) >= capacity) {
        disconnect();
        return;
    }
    auto& next = queue[write % capacity];
    next.rate = rate;
    next.factor = factor;
    next.controls = controls;
    next.controls.limiting = 0;
    next.size = size;
    next.generation = generation.load(std::memory_order_relaxed);
    std::copy_n(input, size, next.input.begin());
    std::copy_n(wet, size, next.wet.begin());
    connected = true;
    writeIndex.store(write + 1, std::memory_order_release);
}

void GainReductionMeter::run()
{
    Oversampled176 reference;
    auto activeGeneration = std::numeric_limits<unsigned>::max();
    double rate = 0;
    int factor = 0;
    bool ready = false;
    std::vector<double> referencePower, wetPower;
    double refSum = 0, wetSum = 0;
    int warmup = 0, filled = 0, cursor = 0, hold = 0;
    float held = 0;
    while (enabled.load(std::memory_order_acquire)) {
        const auto read = readIndex.load(std::memory_order_relaxed);
        if (read == writeIndex.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }
        const auto& chunk = queue[read % capacity];
        if (chunk.generation == generation.load(std::memory_order_acquire)) {
            if (!ready || chunk.generation != activeGeneration || chunk.rate != rate || chunk.factor != factor) {
                activeGeneration = chunk.generation;
                rate = chunk.rate;
                factor = chunk.factor;
                ready = reference.prepare(rate, factor, chunk.controls);
                warmup = static_cast<int>(std::lround(rate * .05)) + Oversampled176::latency;
                const auto window = std::max(1, static_cast<int>(std::lround(rate * .01)));
                referencePower.assign(window, 0);
                wetPower.assign(window, 0);
                refSum = wetSum = 0;
                filled = cursor = hold = 0;
                held = 0;
                reduction.store(unavailable, std::memory_order_relaxed);
            }
            ready = ready && reference.setControls(chunk.controls);
            for (int n = 0; ready && n < chunk.size; ++n) {
                double uncompressed = 0, cv = 0;
                ready = reference.step(chunk.input[n], uncompressed, cv);
                if (warmup > 0) {
                    --warmup;
                    continue;
                }
                const double ref = uncompressed * uncompressed, wet = chunk.wet[n] * chunk.wet[n];
                refSum += ref - referencePower[cursor];
                wetSum += wet - wetPower[cursor];
                referencePower[cursor] = ref;
                wetPower[cursor] = wet;
                if (++cursor == static_cast<int>(referencePower.size())) cursor = 0;
                if (filled < static_cast<int>(referencePower.size())) ++filled;
                if (filled < static_cast<int>(referencePower.size())) continue;
                const double floor = 1e-9 * filled;
                const float measured = refSum > floor
                    ? static_cast<float>(std::max(0., 10. * std::log10(refSum / std::max(wetSum, floor)))) : 0.f;
                if (measured >= held) {
                    held = measured;
                    hold = static_cast<int>(std::lround(rate * .25));
                } else if (hold > 0) {
                    --hold;
                } else {
                    held = measured;
                }
            }
            if (ready && filled == static_cast<int>(referencePower.size()))
                reduction.store(held, std::memory_order_relaxed);
            else
                reduction.store(unavailable, std::memory_order_relaxed);
            reportedGeneration.store(activeGeneration, std::memory_order_release);
        }
        readIndex.store(read + 1, std::memory_order_release);
    }
}
}
