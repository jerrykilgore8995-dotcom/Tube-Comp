#include "Live176.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <immintrin.h>

namespace ua176 {
namespace {

template<class T> double square(T x) { return x * x; }

template<int count>
double dot(const double* a, const double* b) noexcept
{
    __m256d sum = _mm256_setzero_pd();
    int j = 0;
    for (; j + 4 <= count; j += 4)
        sum = _mm256_fmadd_pd(_mm256_loadu_pd(a + j), _mm256_loadu_pd(b + j), sum);
    const __m128d half = _mm_add_pd(_mm256_castpd256_pd128(sum), _mm256_extractf128_pd(sum, 1));
    double result = _mm_cvtsd_f64(_mm_add_sd(half, _mm_unpackhi_pd(half, half)));
    for (; j < count; ++j)
        result += a[j] * b[j];
    return result;
}

struct DiodeTable {
    static constexpr int count = 481;
    std::array<double, count> voltage{}, current{};
    DiodeTable()
    {
        for (int j = 0; j < count; ++j) {
            const double i = std::pow(10., -14. + 14. * j / (count - 1));
            const double v = .081471 * std::log1p(i / 66e-12);
            current[j] = i + (v - .867) / 12.7e9;
            voltage[j] = v - .867 + std::pow(i / .0032 + 1e-9, .775) - std::pow(1e-9, .775);
        }
    }
};
const DiodeTable diodeTable;

void detector(double v, double& current, double& derivative)
{
    const auto& table = diodeTable;
    if (v < table.voltage.front()) {
        const double exponent = std::exp((v + .867) / .081471);
        current = 66e-12 * (exponent - 1) + v / 12.7e9;
        derivative = 66e-12 * exponent / .081471 + 1 / 12.7e9;
        return;
    }
    const auto upper = std::upper_bound(table.voltage.begin(), table.voltage.end(), v);
    const auto j = std::min<std::size_t>(upper - table.voltage.begin(), DiodeTable::count - 1);
    derivative = (table.current[j] - table.current[j - 1]) /
                 (table.voltage[j] - table.voltage[j - 1]);
    current = table.current[j - 1] + (v - table.voltage[j - 1]) * derivative;
}

template<bool derivative>
void plateLaw(double vp, double vg, double offset, double gridScale, double knee,
              double kvb, double gridOffset, double gain, double exponent, double* out)
{
    if (vp <= 0) {
        out[0] = 0;
        if constexpr (derivative) out[1] = 0;
        return;
    }
    const double inverse = 1 / std::sqrt(kvb + vp * vp);
    const double grid = gridScale * (vg + gridOffset);
    const double drive = offset + inverse * grid;
    const double exponential = drive > 50 ? 0 : std::exp(drive);
    const double soft = drive > 50 ? drive : std::log1p(exponential);
    const double energy = vp * soft / knee;
    if constexpr (derivative) {
        if (energy < 1e-50) {
            out[0] = out[1] = 0;
            return;
        }
        const double slope = drive > 50 ? 1 : exponential / (1 + exponential);
        const double common = gain * exponent * std::pow(energy, exponent - 1) / knee;
        out[0] = common * (soft - vp * vp * inverse * inverse * inverse * grid * slope);
        out[1] = common * vp * inverse * gridScale * slope;
    } else {
        out[0] = gain * std::pow(energy, exponent);
    }
}

#include "Live176Deck.h"

constexpr int deviceVoltages = portCount - 5;
constexpr int observations = stateCount + portCount;
constexpr int filterInput = deviceCount + 2;
constexpr int historyOffset = filterInput + 1;
constexpr int columns = historyOffset + stateCount;

ElectricalControls electrical(const LiveControls& c, double hpGain, bool filtered)
{
    ElectricalControls p{};
    auto pad = [](double k, double& arm, double& shunt) {
        arm = 1 / std::max(.01, 300 * (k - 1) / (k + 1));
        shunt = std::max(1e-9, k * k - 1) / (1200 * k);
    };
    pad(c.input, p.INPUTArm, p.INPUTShunt);
    pad(c.output, p.OUTPUTArm, p.OUTPUTShunt);
    p.hpFeed = .5 * c.tap * hpGain;
    p.hpMemory = filtered ? .5 * c.tap * (1 - hpGain) : 0;
    p.doubleOn = c.doubleTiming ? 1 / 200e3 : 0;
    p.doubleOff = c.doubleTiming ? 0 : 1 / 200e3;
    p.limitOn = c.limiting > .5 ? 1000. : 1e-12;
    p.limitOff = c.limiting > .5 ? 1e-12 : 1000.;
    p.tap = c.tap;
    p.ratioBias = 1 / static_cast<double>(c.biasResistance);
    p.attack = 1 / std::max(.001, 25e3 * (1 - c.attack));
    p.releaseUpper = 1 / std::max(.001, 5e6 * (1 - c.release));
    p.releaseLower = 1 / std::max(.001, 5e6 * c.release);
    return p;
}

bool same(const LiveControls& a, const LiveControls& b)
{
    return a.input == b.input && a.output == b.output && a.sidechainHz == b.sidechainHz && a.tap == b.tap && a.biasResistance == b.biasResistance &&
        a.attack == b.attack && a.release == b.release && a.limiting == b.limiting && a.doubleTiming == b.doubleTiming;
}

// Factor the linear circuit once for a control setting. Each RHS column
// describes one device current, state history, or source.
bool eliminate(double* a, double* rhs, int size, int width) noexcept
{
    for (int p = 0; p < size; ++p) {
        int pivot = p;
        for (int row = p + 1; row < size; ++row)
            if (std::abs(a[row * size + p]) > std::abs(a[pivot * size + p]))
                pivot = row;
        auto* const top = a + p * size;
        auto* const topRhs = rhs + p * width;
        if (pivot != p) {
            std::swap_ranges(top + p, top + size, a + pivot * size + p);
            std::swap_ranges(topRhs, topRhs + width, rhs + pivot * width);
        }
        if (!(std::abs(top[p]) > 1e-18))
            return false;
        const double reciprocal = 1 / top[p];
        for (int row = p + 1; row < size; ++row) {
            auto* const target = a + row * size;
            if (target[p] == 0)
                continue;
            const double scale = target[p] * reciprocal;
            for (int x = p + 1; x < size; ++x)
                target[x] -= scale * top[x];
            auto* const targetRhs = rhs + row * width;
            for (int x = 0; x < width; ++x)
                targetRhs[x] -= scale * topRhs[x];
        }
    }
    for (int p = size - 1; p >= 0; --p) {
        auto* const target = rhs + p * width;
        const double reciprocal = 1 / a[p * size + p];
        for (int row = p + 1; row < size; ++row) {
            const double factor = a[p * size + row];
            const auto* const source = rhs + row * width;
            for (int x = 0; x < width; ++x)
                target[x] -= factor * source[x];
        }
        for (int x = 0; x < width; ++x)
            target[x] *= reciprocal;
    }
    return true;
}

}

struct Live176::Circuit {
    double rate = 44100;
    LiveControls controls{};
    bool configured = false;
    std::array<double, nodeCount * nodeCount> a{};
    std::array<double, nodeCount * stateCount> d{};
    std::array<double, nodeCount * historyOffset> b{};
    std::array<double, nodeCount * columns> rhs{};
    std::array<double, portCount * nodeCount> z{};
    std::array<double, observations * columns> response{};
    std::array<double, observations * columns> candidate{};
    std::array<double, columns> feed{};
    std::array<double, stateCount> previous{}, older{};
    std::array<double, portCount> portBase{}, voltages{};
    std::array<double, deviceCount> deviceCurrent{}, oldDeviceCurrent{}, evaluation{}, residual{};
    std::array<double, deviceCount * deviceCount> jacobian{};
    std::array<int, deviceCount> pivots{}, active{}, inactive{};
    std::array<double, deviceCount * deviceCount> cross{};
    std::array<double, deviceCount> delta{};
    int activeCount = 0, inactiveCount = 0;
    bool factored = false;
    std::array<double, deviceVoltages> slopes{};
    double rail = 0, regulator = 0;
    double hpGain = 0, hpState = 0;

    Circuit() { ports(z.data()); }

    bool setup(const LiveControls& next, bool dc)
    {
        a.fill(0); d.fill(0); b.fill(0);
        const bool filtered = !dc && next.sidechainHz > 0;
        const double frequency = std::clamp(static_cast<double>(next.sidechainHz), 0., std::min(350., rate * .45));
        const double g = filtered ? std::tan(3.14159265358979323846 * frequency / rate) : 0;
        const double nextHpGain = g / (1 + g);
        stamp(electrical(next, nextHpGain, filtered), a.data(), d.data(), b.data());
        const double scale = dc ? 0 : rate * .5;
        for (int row = 0; row < nodeCount; ++row) {
            std::copy_n(b.data() + row * historyOffset, historyOffset, rhs.data() + row * columns);
            for (int state = 0; state < stateCount; ++state) {
                const double coefficient = d[row * stateCount + state] * scale;
                rhs[row * columns + historyOffset + state] = coefficient;
                a[row * nodeCount + stateIndices[state]] += 3 * coefficient;
            }
        }
        if (!eliminate(a.data(), rhs.data(), nodeCount, columns))
            return false;
        for (int port = 0; port < portCount; ++port)
            for (int j = 0; j < columns; ++j) {
                double coefficient = 0;
                for (int i = 0; i < nodeCount; ++i)
                    coefficient += z[port * nodeCount + i] * rhs[i * columns + j];
                candidate[port * columns + j] = coefficient;
            }
        for (int state = 0; state < stateCount; ++state)
            std::copy_n(rhs.data() + stateIndices[state] * columns, columns,
                        candidate.data() + (portCount + state) * columns);
        response.swap(candidate);
        hpGain = nextHpGain;
        if (next.sidechainHz <= 0)
            hpState = 0;
        factored = false;
        oldDeviceCurrent = deviceCurrent;
        controls = next;
        return true;
    }

    void preparePorts(double audio, double supply)
    {
        feed[0] = supply;
        feed[1] = audio;
        feed[filterInput] = hpState;
        for (int i = 0; i < stateCount; ++i)
            feed[historyOffset + i] = 4 * previous[i] - older[i];
        for (int port = 0; port < deviceVoltages; ++port) {
            const auto* row = response.data() + port * columns;
            portBase[port] = row[0] * feed[0] + row[1] * feed[1]
                + row[filterInput] * hpState
                + dot<stateCount>(row + historyOffset, feed.data() + historyOffset);
        }
    }

    bool factorJacobian() noexcept
    {
        devicesAt<true>(voltages.data(), slopes.data());
        activeCount = inactiveCount = 0;
        for (int i = 0; i < deviceCount; ++i) {
            double slope = 0;
            for (int k = deviceOffsets[i]; k < deviceOffsets[i + 1]; ++k)
                slope += std::abs(slopes[k]);
            // Almost-open junctions and cut-off grids can use a unit
            // Jacobian row. All device residuals still have to converge.
            if (slope > 1e-8)
                active[activeCount++] = i;
            else
                inactive[inactiveCount++] = i;
        }
        for (int i = 0; i < activeCount; ++i) {
            const int device = active[i];
            const auto coefficient = [&](int other) {
                double value = device == other ? 1 : 0;
                for (int k = deviceOffsets[device]; k < deviceOffsets[device + 1]; ++k)
                    value -= slopes[k] * response[k * columns + other + 2];
                return value;
            };
            for (int j = 0; j < activeCount; ++j)
                jacobian[i * deviceCount + j] = coefficient(active[j]);
            for (int j = 0; j < inactiveCount; ++j)
                cross[i * deviceCount + j] = coefficient(inactive[j]);
        }

        for (int p = 0; p < activeCount; ++p) {
            int pivot = p;
            for (int row = p + 1; row < activeCount; ++row)
                if (std::abs(jacobian[row * deviceCount + p]) > std::abs(jacobian[pivot * deviceCount + p]))
                    pivot = row;
            pivots[p] = pivot;
            auto* top = jacobian.data() + p * deviceCount;
            if (pivot != p)
                std::swap_ranges(top, top + activeCount, jacobian.data() + pivot * deviceCount);
            if (!(std::abs(top[p]) > 1e-18))
                return false;
            const double reciprocal = 1 / top[p];
            for (int row = p + 1; row < activeCount; ++row) {
                auto* target = jacobian.data() + row * deviceCount;
                target[p] *= reciprocal;
                for (int j = p + 1; j < activeCount; ++j)
                    target[j] -= target[p] * top[j];
            }
        }
        return true;
    }

    void solveJacobian() noexcept
    {
        for (int i = 0; i < activeCount; ++i) {
            delta[i] = residual[active[i]];
            for (int j = 0; j < inactiveCount; ++j)
                delta[i] -= cross[i * deviceCount + j] * residual[inactive[j]];
        }
        for (int i = 0; i < activeCount; ++i)
            std::swap(delta[i], delta[pivots[i]]);
        for (int i = 1; i < activeCount; ++i) {
            const auto* row = jacobian.data() + i * deviceCount;
            for (int j = 0; j < i; ++j)
                delta[i] -= row[j] * delta[j];
        }
        for (int i = activeCount - 1; i >= 0; --i) {
            const auto* row = jacobian.data() + i * deviceCount;
            for (int j = i + 1; j < activeCount; ++j)
                delta[i] -= row[j] * delta[j];
            delta[i] /= row[i];
            residual[active[i]] = delta[i];
        }
    }

    bool newton(int budget, bool predict)
    {
        auto next = deviceCurrent;
        if (predict)
            for (int i = 0; i < deviceCount; ++i)
                next[i] += (controls.limiting > .5 ? .9 : .3) * (deviceCurrent[i] - oldDeviceCurrent[i]);
        double previousError = std::numeric_limits<double>::infinity();
        for (int iteration = 0; iteration < budget; ++iteration) {
            for (int port = 0; port < deviceVoltages; ++port) {
                const auto* row = response.data() + port * columns + 2;
                voltages[port] = portBase[port] + dot<deviceCount>(row, next.data());
            }
            devicesAt<false>(voltages.data(), evaluation.data());
            double error = 0;
            for (int i = 0; i < deviceCount; ++i) {
                residual[i] = evaluation[i] - next[i];
                if (!std::isfinite(residual[i]))
                    return false;
                error = std::max(error, std::abs(residual[i]) / (1e-8 + 3e-5 * std::abs(evaluation[i])));
            }
            if (error <= 1) {
                oldDeviceCurrent = deviceCurrent;
                deviceCurrent = next;
                return true;
            }
            if (!predict || !factored || error > previousError * .20) {
                factored = factorJacobian();
                if (!factored)
                    return false;
            }
            solveJacobian();
            previousError = error;

            double damping = 1;
            if (error > 1e3) {
                for (int i = 0; i < deviceCount; ++i) {
                    const int first = deviceOffsets[i];
                    const int count = deviceOffsets[i + 1] - first;
                    // Limit upward excursions into exponential grid conduction.
                    // In reverse bias a large step does not destabilize the law.
                    if (count != 1 || i >= deviceCount - 2 || evaluation[i] < 1e-8)
                        continue;
                    const auto* row = response.data() + first * columns + 2;
                    const double dv = dot<deviceCount>(row, residual.data());
                    if (dv > .5)
                        damping = std::min(damping, .5 / dv);
                }
            }
            for (int i = 0; i < deviceCount; ++i)
                next[i] += damping * residual[i];
        }
        return false;
    }

    double observe(int row) const noexcept
    {
        const auto* coefficients = response.data() + row * columns;
        return dot<columns>(coefficients, feed.data());
    }

    bool advance(double audio, double supply, int budget, double& output, double& cv) noexcept
    {
        preparePorts(audio, supply);
        if ((!configured || !newton(std::min(budget, controls.limiting > .5 ? 7 : 14), true)) && !newton(budget, false))
            return false;
        std::copy(deviceCurrent.begin(), deviceCurrent.end(), feed.begin() + 2);
        output = observe(deviceVoltages);
        cv = observe(deviceVoltages + 1);
        rail = observe(deviceVoltages + 2);
        regulator = observe(deviceVoltages + 3);
        if (hpGain > 0)
            hpState += 2 * hpGain * (observe(deviceVoltages + 4) - hpState);
        else if (!configured && controls.sidechainHz > 0)
            hpState = observe(deviceVoltages + 4);
        older = previous;
        for (int state = 0; state < stateCount; ++state)
            previous[state] = observe(portCount + state);
        return std::isfinite(output) && std::isfinite(cv);
    }
};

Live176::Live176() : circuit(std::make_unique<Circuit>()) {}
Live176::~Live176() = default;
bool Live176::prepare(double rate, const LiveControls& knobs)
{
    circuit->rate = rate;
    circuit->deviceCurrent.fill(0);
    circuit->previous.fill(0);
    circuit->older.fill(0);
    circuit->configured = false;
    circuit->hpState = 0;
    if (!circuit->setup(knobs, true))
        return false;
    double audio = 0, cv = 0;
    for (int i = 1; i <= 40; ++i)
        if (!circuit->advance(0, i / 40., 60, audio, cv))
            return false;
    circuit->older = circuit->previous;
    circuit->configured = circuit->setup(knobs, false);
    return circuit->configured;
}

bool Live176::setControls(const LiveControls& knobs)
{
    return circuit->configured && (same(circuit->controls, knobs) || circuit->setup(knobs, false));
}

bool Live176::step(double input, double& output, double& cv) noexcept
{
    return circuit->configured && circuit->advance(input, 1, 64, output, cv);
}

double Live176::bPlus() const noexcept { return circuit->rail; }
double Live176::regulated() const noexcept { return circuit->regulator; }

}
