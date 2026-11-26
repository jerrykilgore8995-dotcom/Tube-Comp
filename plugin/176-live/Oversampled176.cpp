#include "Oversampled176.h"

#include <algorithm>

namespace ua176 {
namespace {
#include "OversampleData.h"

constexpr std::array<double, 6> rates{44100, 48000, 88200, 96000, 176400, 192000};
constexpr int halfbandLatency = 96;
constexpr int sourceLatency = 24;
}

std::array<double, 2> Oversampled176::Halfband::up(double input) noexcept
{
    const auto n = position++;
    history[n & 255] = input;
    double odd = 0;
    for (unsigned k = 0; k < 48; ++k)
        odd += halfband[2*k+1] * (history[(n-k) & 255] + history[(n-(95-k)) & 255]);
    return {history[(n-48) & 255], 2 * odd};
}

double Oversampled176::Halfband::down(double a, double b) noexcept
{
    const auto n = position;
    history[n & 255] = a;
    double output = .5 * history[(n-96) & 255];
    for (unsigned k = 1; k < 96; k += 2)
        output += halfband[k] * (history[(n-k) & 255] + history[(n-(192-k)) & 255]);
    history[(n+1) & 255] = b;
    position += 2;
    return output;
}

bool Oversampled176::supportsOversampling(double rate) noexcept
{
    return std::find(rates.begin(), rates.end(), rate) != rates.end();
}

void Oversampled176::resetResampling() noexcept
{
    upOuter = upInner = downOuter = downInner = {};
    delay = {};
    source = {};
    position = sourcePosition = 0;
}

bool Oversampled176::prepare(double rate, int factor, const LiveControls& controls)
{
    const auto rateIndex = std::find(rates.begin(), rates.end(), rate);
    mode = rateIndex != rates.end() && (factor == 2 || factor == 4) ? factor : 1;
    resetResampling();
    correction = mode == 1 ? nullptr
        : sourceCorrection[rateIndex - rates.begin()][mode == 4 ? 1 : 0];
    const int pathDelay = mode == 1 ? Live176::latency(rate)
        : halfbandLatency + sourceLatency + (mode == 4 ? halfbandLatency / 2 : 0);
    extraDelay = latency - pathDelay;
    ready = circuit.prepare(rate * mode, controls);
    return ready;
}

bool Oversampled176::setControls(const LiveControls& controls)
{
    return ready && circuit.setControls(controls);
}

bool Oversampled176::step(double input, double& output, double& cv) noexcept
{
    if (!ready)
        return false;
    bool solved = true;
    const auto solve = [&](double x) {
        double y = 0;
        solved = circuit.step(x, y, cv) && solved;
        return y;
    };
    double wet = 0;
    if (mode == 1) {
        wet = solve(input);
    } else {
        // Correct the held-source integration error before the nonlinear solve.
        const auto n = sourcePosition++;
        source[n & 127] = input;
        double alignedInput = 0;
        for (unsigned k = 0; k < 49; ++k)
            alignedInput += correction[k] * source[(n-k) & 127];
        const auto pair = upOuter.up(alignedInput);
        const auto inner = [&](double x) {
            if (mode == 2)
                return solve(x);
            const auto sub = upInner.up(x);
            const double a = solve(sub[0]);
            const double b = solve(sub[1]);
            return downInner.down(a, b);
        };
        const double a = inner(pair[0]);
        const double b = inner(pair[1]);
        wet = downOuter.down(a, b);
    }

    output = wet;
    if (extraDelay != 0) {
        output = delay[position];
        delay[position] = wet;
        position = (position + 1) % static_cast<unsigned>(extraDelay);
    }
    return solved;
}

}
