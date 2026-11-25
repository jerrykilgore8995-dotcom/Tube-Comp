#include "SpiceStream.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

int main(int argc, char** argv)
{
    if (argc != 2) return 1;
    using Clock = std::chrono::steady_clock;
    ua176::SpiceStream stream, bypassed;
    const auto start = Clock::now();
    const auto library = std::filesystem::u8path(argv[1]);
    if (!stream.start(library, 44100) || !bypassed.start(library, 44100)) return 2;
    const ua176::Controls controls;
    constexpr int submitted = 16000;
    constexpr int measured = 15000;
    auto off = controls;
    off.limiting = 0;
    for (int i = 0; i < submitted; ++i) {
        const auto tone = .1995262f * static_cast<float>(std::sin(6.283185307 * 1000 * i / 44100.));
        if (!stream.submit(tone, controls) || !bypassed.submit(tone, off)) return 3;
    }
    while ((stream.framesProduced() < measured || bypassed.framesProduced() < measured)
           && !stream.failed() && !bypassed.failed()
           && Clock::now() - start < std::chrono::minutes(2))
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    constexpr int testCount = 3000;
    double sumSquares = 0, sumBias = 0, bypassBias = 0;
    for (int i = measured - testCount; i < measured; ++i) {
        float sample = 0, cv = 0;
        if (!stream.read(static_cast<std::uint64_t>(i), sample, cv)) return 4;
        sumSquares += sample * sample;
        sumBias += cv;
        if (!bypassed.read(static_cast<std::uint64_t>(i), sample, cv)) return 4;
        bypassBias += cv;
    }
    const double seconds = std::chrono::duration<double>(Clock::now() - start).count();
    const double rms = std::sqrt(sumSquares / testCount);
    const double cv = sumBias / testCount;
    std::cout << measured << " solved samples in " << seconds << " s, output RMS "
              << rms << ", 6BC8 CV " << cv << " V, limiting off " << bypassBias / testCount << " V\n";
    return rms > .02 && rms < .8 && cv < -.2 && cv > -5 && std::abs(bypassBias / testCount) < .01 ? 0 : 5;
}
