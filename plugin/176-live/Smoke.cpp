#include "Live176.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

int main(int argc, char**)
{
    using Clock = std::chrono::steady_clock;
    constexpr int rate = 44100, block = 512, warm = 64, blocks = 384;
    constexpr double deadline = 1000. * block / rate;
    constexpr double sourcePeak = 3.474135043921514;
    constexpr double tau = 6.283185307179586;
    constexpr std::array<float, 4> taps{.12f, .29f, .58f, 1.f};
    constexpr std::array<float, 4> bias{1000, 6800, 20000, 47000};
    constexpr std::array<int, 4> ratios{2, 4, 8, 12};
    const int channels = argc > 1 ? 2 : 1;
    std::array<ua176::Live176, 2> simulators;
    bool missedDeadline = false;
    std::array<double, block> tone{};
    std::vector<double> times;
    times.reserve(blocks);
    for (int setting = 0; setting < 6; ++setting) {
        const int ratioIndex = setting == 5 ? 3 : setting % 4;
        const int ratio = ratios[ratioIndex];
        const int frequency = setting == 5 ? 10000 : 1000;
        ua176::Controls knobs;
        knobs.output = 1;
        knobs.tap = taps[ratioIndex];
        knobs.biasResistance = bias[ratioIndex];
        knobs.limiting = setting == 4 ? 0 : 1;
        for (auto& simulator : simulators)
            if (!simulator.prepare(rate, knobs)) {
                std::fprintf(stderr, "Initialization failed at %d:1, limiting %.0f\n", ratio, knobs.limiting);
                return 2;
            }

        times.clear();
        double squares = 0, controlSum = 0, bPlus = 0, reg = 0;
        int captured = 0;
        for (int b = 0; b < blocks + warm; ++b) {
            for (int i = 0; i < block; ++i)
                tone[i] = sourcePeak * std::sin(tau * frequency * (b * block + i) / rate);
            const auto start = Clock::now();
            for (int channel = 0; channel < channels; ++channel) {
                auto& simulator = simulators[channel];
                for (int i = 0; i < block; ++i) {
                    double value = 0, cv = 0;
                    if (!simulator.step(tone[i] * (channel == 0 ? 1 : -.57), value, cv)) {
                        std::fprintf(stderr, "Convergence failed at %d:1, limiting %.0f, channel %d, sample %d\n",
                                     ratio, knobs.limiting, channel, b * block + i);
                        return 3;
                    }
                    if (channel == 0 && b >= blocks + warm - 18) {
                        squares += value * value;
                        controlSum += cv;
                        bPlus += simulator.bPlus();
                        reg += simulator.regulated();
                        ++captured;
                    }
                }
            }
            const double ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
            if (b >= warm)
                times.push_back(ms);
        }

        const double rms = std::sqrt(squares / captured);
        controlSum /= captured;
        bPlus /= captured;
        reg /= captured;
        std::sort(times.begin(), times.end());
        const double p99 = times[static_cast<int>(std::ceil(times.size() * .99)) - 1];
        const auto misses = std::count_if(times.begin(), times.end(), [=](double ms) { return ms >= deadline; });
        double mean = 0;
        for (double ms : times)
            mean += ms / times.size();
        std::printf("%d Hz, %d:1 limiting %.0f, %s 512 @ 44.1k: mean %.3f ms, p99 %.3f ms, max %.3f ms, misses %zu / %d, "
                    "RMS %.6f V, CV %.6f V, B+ %.6f V, REG %.6f V\n",
                    frequency, ratio, knobs.limiting, channels == 1 ? "mono" : "stereo", mean, p99, times.back(), static_cast<std::size_t>(misses),
                    blocks, rms, controlSum, bPlus, reg);
        std::fflush(stdout);
        missedDeadline |= misses != 0;
        if ((setting == 0 && (rms < 6.70 || rms > 7.02 || controlSum < -3.55 || controlSum > -3.4))
            || (setting == 4 && std::abs(controlSum) > .01))
            return 4;
    }
    // Change the pad during playback and measure across its output load.
    auto& simulator = simulators[0];
    ua176::Controls knobs;
    knobs.output = 1;
    if (!simulator.prepare(rate, knobs))
        return 6;
    double openRms = 0;
    int sample = 0;
    for (int attenuation : {0, 2, 10, 20, 30, 40}) {
        knobs.output = std::pow(10.f, attenuation / 20.f);
        if (!simulator.setControls(knobs))
            return 6;
        double power = 0;
        constexpr int window = rate / 5;
        for (int n = 0; n < 3 * window; ++n, ++sample) {
            double value = 0, cv = 0;
            if (!simulator.step(sourcePeak * std::sin(tau * 1000 * sample / rate), value, cv))
                return 6;
            if (n >= 2 * window)
                power += value * value;
        }
        const double rms = std::sqrt(power / window);
        if (attenuation == 0)
            openRms = rms;
        const double measured = 20 * std::log10(openRms / rms);
        std::printf("Output pad: %d dB selected, %.3f dB measured\n", attenuation, measured);
        if (!std::isfinite(measured) || std::abs(measured - attenuation) > .05)
            return 6;
    }
    return missedDeadline ? 5 : 0;
}
