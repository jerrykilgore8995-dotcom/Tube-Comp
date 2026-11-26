#include "Oversampled176.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>

namespace {
constexpr int rate = 44100, blockSize = 512, count = 384;
constexpr double tau = 6.283185307179586;

std::complex<double> measure(ua176::Oversampled176& simulator, int frequency, double amplitude,
                             int settle, int bin, bool& valid)
{
    constexpr int capture = rate / 5;
    std::complex<double> sum{};
    for (int sample = 0; sample < settle + capture; ++sample) {
        double output = 0, cv = 0;
        const double input = amplitude * std::sin(tau * frequency * sample / rate);
        valid &= simulator.step(input, output, cv);
        if (sample >= settle)
            sum += output * std::polar(1., -tau * bin * (sample - ua176::Oversampled176::latency) / rate);
    }
    return sum * (2. / capture);
}
}

int main()
{
    bool valid = true;
    bool missed = false;
    using Clock = std::chrono::steady_clock;
    constexpr double source = 3.474135043921514;
    ua176::LiveControls controls;
    controls.output = 10;
    for (int factor : {2, 4}) {
        ua176::Oversampled176 simulator;
        for (bool limiting : {true, false}) {
            controls.limiting = limiting;
            for (int frequency : {1000, 5000, 10000, 0}) {
                if (!simulator.prepare(rate, factor, controls))
                    return 1;
                std::array<double, count> times{};
                std::array<double, blockSize> tone{};
                for (int b = -36; b < count; ++b) {
                    for (int n = 0; n < blockSize; ++n) {
                        const double t = (b * blockSize + n) / static_cast<double>(rate);
                        tone[n] = frequency != 0 ? source * std::sin(tau * frequency * t)
                            : 1.05 * std::sin(tau * 97 * t) + .6 * std::sin(tau * 713 * t)
                              + .35 * std::sin(tau * 5243 * t) + 1.9 * std::exp(-24 * std::fmod(std::abs(t), .18)) * std::sin(tau * 61 * t);
                    }
                    const auto start = Clock::now();
                    for (double in : tone) {
                        double out = 0, cv = 0;
                        valid &= simulator.step(in, out, cv);
                    }
                    if (b >= 0)
                        times[b] = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
                }
                std::sort(times.begin(), times.end());
                const double limit = factor == 2 ? (1000. * blockSize / rate) : 14.;
                const auto misses = std::count_if(times.begin(), times.end(), [limit](double t) { return t >= limit; });
                std::printf("%dx %d Hz limiting %d: p99 %.3f ms, peak %.3f ms, %td blocks >= %.3f ms\n",
                            factor, frequency, limiting ? 1 : 0, times[380], times.back(), misses, limit);
                // Limiting-off timing is diagnostic. Keep the budget for active compression.
                missed |= limiting && times[380] >= limit;
            }
        }

        std::complex<double> midband{};
        int index = 0;
        for (int f : {1000, 20, 30, 10000, 20000}) {
            if (!simulator.prepare(rate, factor, controls))
                return 1;
            const auto value = measure(simulator, f, .0693, rate / 2, f, valid);
            if (f == 1000) {
                midband = value;
                continue;
            }
            // Linearized continuous LiveSPICE deck at the stock operating point.
            constexpr std::array<double, 4> gain{-1.420, -.677, -.175, -.726};
            constexpr std::array<double, 4> phase{44.860, 31.764, -29.112, -60.517};
            const auto relative = value / midband;
            const auto db = 20 * std::log10(std::abs(relative));
            const auto deg = std::arg(relative) * 360 / tau;
            std::printf("%dx %d Hz relative to 1 kHz: %+.3f dB, %+.2f degrees\n", factor, f, db, deg);
            valid &= std::abs(db - gain[index]) < .15 && std::abs(deg - phase[index]) < 3;
            ++index;
        }
        // At 10 kHz, the third harmonic folds to 14.1 kHz at native rate.
        controls.limiting = 1;
        controls.input = 20.2f;
        if (!simulator.prepare(rate, factor, controls))
            return 1;
        const auto fundamental = measure(simulator, 10000, source, rate * 2, 10000, valid);
        const auto spur = measure(simulator, 10000, source, rate / 2, 14100, valid);
        const auto alias = 20 * std::log10(std::abs(spur / fundamental));
        std::printf("%dx 10 kHz, 14.1 kHz foldback relative to fundamental: %.2f dB\n", factor, alias);
        valid &= alias < -100;
        controls.input = 10;
    }
    if (!valid)
        return 1;
    return missed ? 5 : 0;
}
