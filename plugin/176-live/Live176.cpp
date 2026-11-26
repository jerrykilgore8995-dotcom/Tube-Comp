#include "Live176.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <limits>
#include <immintrin.h>

namespace ua176 {
namespace {

template<class T> double square(T x) { return x * x; }

template<int count>
double dot(const double* a, const double* b) noexcept
{
    __m256d sum = _mm256_setzero_pd();
    __m256d second = _mm256_setzero_pd();
    int j = 0;
    for (; j + 8 <= count; j += 8) {
        sum = _mm256_fmadd_pd(_mm256_loadu_pd(a + j), _mm256_loadu_pd(b + j), sum);
        second = _mm256_fmadd_pd(_mm256_loadu_pd(a + j + 4), _mm256_loadu_pd(b + j + 4), second);
    }
    sum = _mm256_add_pd(sum, second);
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

constexpr int deviceVoltages = portCount - 6;
constexpr int observations = stateCount + portCount;
static_assert(observations <= nodeCount);
constexpr int filterInput = deviceCount + 2;
constexpr int historyOffset = filterInput + 1;
constexpr int columns = historyOffset + stateCount;
constexpr int linearMapDoublings = 4;
constexpr double sdirkGamma = .2928932188134524756; // 1 - 1/sqrt(2)
constexpr int phaseTaps = 2 * Live176::phaseLatency + 1;
constexpr int detectorStart = deviceCount - 2;
constexpr double referenceTap = .29;
using Complex = std::complex<double>;

#ifndef UA176_PHASE_REFERENCE
#include "PhaseProfiles.h"
#endif

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
    std::array<double, stateCount> previous{};
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
    std::array<double, portCount> phaseVoltage{};
    std::array<double, deviceCount> phaseCurrent{}, detectorSlope{}, detectorResistance{};
    std::array<std::array<double, DiodeTable::count>, 2> detectorIncident{};
#ifndef UA176_PHASE_REFERENCE
    std::array<double, 2 * phaseTaps> phaseHistory{};
    std::array<std::array<Complex, maxPhaseModes>, phaseProfiles.size()> phaseState{};
    std::array<double, 2> phaseHighPass{};
    double phaseBias = 0, phaseBiasBlend = 0;
    int phaseCursor = 0;
#else
    std::FILE* phaseReference = nullptr;
    double referenceBias = 0;
#endif

    Circuit() { ports(z.data()); }

    double reconstructSource(double input) noexcept
    {
#ifndef UA176_PHASE_REFERENCE
        if (!configured || !Live176::latency(rate))
            return input;
        phaseHistory[phaseCursor] = phaseHistory[phaseCursor + phaseTaps] = input * (10. / controls.input);
        const auto* samples = phaseHistory.data() + phaseCursor;
        phaseCursor = (phaseCursor + phaseTaps - 1) % phaseTaps;
        phaseVoltage.fill(0);
        phaseCurrent.fill(0);
        std::size_t first = 0;
        while (first + 2 < phaseProfiles.size() && phaseBias < phaseProfiles[first + 1].bias)
            ++first;
        const auto& lo = phaseProfiles[first];
        const auto& hi = phaseProfiles[first + 1];
        const double fraction = std::clamp((phaseBias - lo.bias) / (hi.bias - lo.bias), 0., 1.);
        for (std::size_t i = 0; i < phaseProfiles.size(); ++i) {
            const auto& profile = phaseProfiles[i];
            auto& state = phaseState[i];
            for (int mode = 0; mode < profile.modes; ++mode)
                state[mode] = profile.poles[mode] * state[mode] + samples[Live176::phaseLatency];
            const double weight = i == first ? 1 - fraction : i == first + 1 ? fraction : 0;
            if (weight == 0)
                continue;
            std::array<double, portCount> voltage{};
            for (int port = 0; port < portCount; ++port)
                voltage[port] = dot<phaseTaps>(profile.impulse[port], samples);
            for (int mode = 0; mode < profile.modes; ++mode)
                for (int port = 0; port < portCount; ++port)
                    voltage[port] += std::real(profile.weights[mode][port] * state[mode]);
            for (int port = 0; port < portCount; ++port)
                phaseVoltage[port] += weight * voltage[port];
            // Subtract the reconstructed linear device current. Newton
            // solves the residual current against the total port voltage.
            for (int device = 0; device < deviceCount; ++device)
                for (int port = deviceOffsets[device]; port < deviceOffsets[device + 1]; ++port)
                    phaseCurrent[device] += weight * profile.slopes[port] * voltage[port];
        }
        phaseVoltage[deviceVoltages] *= 10. / controls.output;
        for (int i = 0; i < 2; ++i) {
            auto& voltage = phaseVoltage[deviceOffsets[detectorStart + i]];
            voltage *= controls.tap / referenceTap;
            if (hpGain > 0) {
                const double step = hpGain * (voltage - phaseHighPass[i]);
                voltage -= phaseHighPass[i] + step;
                phaseHighPass[i] += 2 * step;
            } else {
                phaseHighPass[i] = voltage;
            }
        }
        return 0;
#else
        return input;
#endif
    }

    unsigned implicitDetectors = 0;

    void prepareDetectorPorts() noexcept
    {
        implicitDetectors = 0;
        for (int device = detectorStart; device < deviceCount; ++device) {
            const int port = deviceOffsets[device];
            const double resistance = -response[port * columns + device + 2];
            detectorResistance[device] = resistance;
            if (resistance <= 0)
                continue;
            implicitDetectors |= 1u << device;
            auto& incident = detectorIncident[device - detectorStart];
            for (int i = 0; i < DiodeTable::count; ++i)
                incident[i] = diodeTable.voltage[i] + resistance * diodeTable.current[i];
        }
    }

#ifdef UA176_PHASE_REFERENCE
    void dumpLinearCircuit() noexcept
    {
        devicesAt<true>(voltages.data(), slopes.data());
        const int sizes[]{nodeCount, stateCount, deviceCount, deviceVoltages, portCount};
        std::fwrite(sizes, sizeof(int), 5, phaseReference);
        std::fwrite(stateIndices, sizeof(int), stateCount, phaseReference);
        std::fwrite(deviceOffsets, sizeof(int), deviceCount + 1, phaseReference);
        for (const auto& block : {std::pair{a.data(), a.size()}, std::pair{d.data(), d.size()},
                                  std::pair{b.data(), b.size()}, std::pair{z.data(), z.size()},
                                  std::pair{slopes.data(), slopes.size()}})
            std::fwrite(block.first, sizeof(double), block.second, phaseReference);
    }
#endif

    // Compose an affine map F with itself, optionally keeping a weighted
    // contribution from F. Each composed state map uses the same held sources.
    void composeLinearMap(double weight) noexcept
    {
        for (int observation = 0; observation < observations; ++observation) {
            const auto* row = candidate.data() + observation * columns;
            auto* composed = rhs.data() + observation * columns;
            std::copy_n(row, historyOffset, composed);
            for (int column = historyOffset; column < columns; ++column)
                composed[column] = (1 - weight) * row[column];
            for (int state = 0; state < stateCount; ++state) {
                const auto* transition = candidate.data() + (portCount + state) * columns;
                const double coefficient = weight * row[historyOffset + state];
                for (int column = 0; column < columns; ++column)
                    composed[column] += coefficient * transition[column];
            }
        }
        std::copy_n(rhs.data(), candidate.size(), candidate.data());
    }

    bool setup(const LiveControls& next, bool dc)
    {
        a.fill(0); d.fill(0); b.fill(0);
        const bool filtered = !dc && next.sidechainHz > 0;
        const double frequency = std::clamp(static_cast<double>(next.sidechainHz), 0., std::min(350., rate * .45));
        const double g = filtered ? std::tan(3.14159265358979323846 * frequency / rate) : 0;
        const double nextHpGain = g / (1 + g);
        stamp(electrical(next, nextHpGain, filtered), a.data(), d.data(), b.data());
#ifdef UA176_PHASE_REFERENCE
        if (phaseReference) {
            if (dc) {
                a[controlEquation * nodeCount + controlNode] += 1e5;
                b[controlEquation * historyOffset] += 1e5 * referenceBias;
            } else {
                dumpLinearCircuit();
            }
        }
#endif
        const double scale = dc ? 0 : std::ldexp(rate, linearMapDoublings) / sdirkGamma;
        for (int row = 0; row < nodeCount; ++row) {
            std::copy_n(b.data() + row * historyOffset, historyOffset, rhs.data() + row * columns);
            for (int state = 0; state < stateCount; ++state) {
                const double coefficient = d[row * stateCount + state] * scale;
                rhs[row * columns + historyOffset + state] = coefficient;
                a[row * nodeCount + stateIndices[state]] += coefficient;
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
        if (!dc) {
            // The stiffly accurate, two-stage SDIRK map is
            // F(x + (1 - gamma)/gamma * (F(x) - x)). Square it to span one
            // host sample. These are matrix operations during setup only.
            // At run time the source and implicit device currents are held
            // over the interval and the nonlinear solve runs at host rate.
            composeLinearMap((1 - sdirkGamma) / sdirkGamma);
            for (int pass = 0; pass < linearMapDoublings; ++pass)
                composeLinearMap(1);
        }
        if (!std::all_of(candidate.begin(), candidate.end(), [](double value) { return std::isfinite(value); }))
            return false;
        response.swap(candidate);
        prepareDetectorPorts();
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
        std::copy(previous.begin(), previous.end(), feed.begin() + historyOffset);
        for (int port = 0; port < deviceVoltages; ++port) {
            const auto* row = response.data() + port * columns;
            portBase[port] = phaseVoltage[port] + row[0] * feed[0] + row[1] * feed[1]
                + row[filterInput] * hpState
                + dot<stateCount>(row + historyOffset, feed.data() + historyOffset);
        }
    }

    bool factorJacobian() noexcept
    {
        devicesAt<true>(voltages.data(), slopes.data(), implicitDetectors);
        for (int device = detectorStart; device < deviceCount; ++device)
            if (detectorResistance[device] > 0)
                slopes[deviceOffsets[device]] = detectorSlope[device];
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
                if (device == other && detectorResistance[device] > 0)
                    return 1.0;
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

    void evaluateCurrents(const std::array<double, deviceCount>& currents) noexcept
    {
        for (int port = 0; port < deviceVoltages; ++port) {
            const auto* row = response.data() + port * columns + 2;
            voltages[port] = portBase[port] + dot<deviceCount>(row, currents.data());
        }
        devicesAt<false>(voltages.data(), evaluation.data(), implicitDetectors);
        for (int device = detectorStart; device < deviceCount; ++device) {
            const double resistance = detectorResistance[device];
            if (resistance <= 0)
                continue;
            const auto& incident = detectorIncident[device - detectorStart];
            const double drive = voltages[deviceOffsets[device]] + resistance * (currents[device] + phaseCurrent[device]);
            // Implicit one-port solve using the same piecewise-linear
            // diode law, with each breakpoint expressed as V + R*I.
            if (drive < incident.front()) {
                double current = diodeTable.current.front() + (drive - incident.front()) / (resistance + 12.7e9);
                double exact = 0, slope = 0;
                detector(drive - resistance * current, exact, slope);
                detectorSlope[device] = slope / (1 + resistance * slope);
                evaluation[device] = current + (exact - current) / (1 + resistance * slope);
            } else {
                const auto upper = std::upper_bound(incident.begin(), incident.end(), drive);
                const auto i = std::min<std::size_t>(upper - incident.begin(), DiodeTable::count - 1);
                detectorSlope[device] = (diodeTable.current[i] - diodeTable.current[i-1]) / (incident[i] - incident[i-1]);
                evaluation[device] = diodeTable.current[i-1] + detectorSlope[device] * (drive - incident[i-1]);
            }
        }
        for (int device = 0; device < deviceCount; ++device)
            evaluation[device] -= phaseCurrent[device];
    }

    double backtrack(const std::array<double, deviceCount>& currents, double damping, double error) noexcept
    {
        std::array<double, deviceCount> trial{}, scales{};
        for (int i = 0; i < deviceCount; ++i)
            scales[i] = 1e-8 + 3e-5 * std::abs(evaluation[i]);
        while (damping >= .001) {
            for (int i = 0; i < deviceCount; ++i)
                trial[i] = currents[i] + damping * residual[i];
            evaluateCurrents(trial);
            double trialError = 0;
            for (int i = 0; i < deviceCount; ++i) {
                const double difference = std::abs(evaluation[i] - trial[i]) / scales[i];
                trialError = std::max(trialError, std::isfinite(difference)
                    ? difference : std::numeric_limits<double>::infinity());
            }
            if (trialError < error)
                break;
            damping *= .5;
        }
        return damping;
    }

    bool newton(int budget, bool predict, bool useLineSearch = false)
    {
        auto next = deviceCurrent;
        if (predict)
            for (int i = 0; i < deviceCount; ++i)
                next[i] += (controls.limiting > .5 ? .9 : .3) * (deviceCurrent[i] - oldDeviceCurrent[i]);
        double previousError = std::numeric_limits<double>::infinity();
        for (int iteration = 0; iteration < budget; ++iteration) {
            evaluateCurrents(next);
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
            // Use a residual line search on the full-Newton retry. Trial
            // currents never advance the electrical state or the audio clock.
            if (useLineSearch)
                damping = backtrack(next, damping, error);
            for (int i = 0; i < deviceCount; ++i)
                next[i] += damping * residual[i];
        }
        return false;
    }

    double observe(int row) const noexcept
    {
        const auto* coefficients = response.data() + row * columns;
        return dot<columns>(coefficients, feed.data()) + (row < portCount ? phaseVoltage[row] : 0);
    }

    bool advance(double audio, double supply, int budget, double& output, double& cv) noexcept
    {
        preparePorts(reconstructSource(audio), supply);
        if ((!configured || !newton(std::min(budget, controls.limiting > .5 ? 7 : 14), true))
            && !newton(budget, false)
            && (!configured || !newton(budget, false, true)))
            return false;
        std::copy(deviceCurrent.begin(), deviceCurrent.end(), feed.begin() + 2);
        output = observe(deviceVoltages);
        cv = observe(deviceVoltages + 1);
#ifndef UA176_PHASE_REFERENCE
        phaseBias += phaseBiasBlend * (cv - phaseBias);
#endif
        rail = observe(deviceVoltages + 2);
        regulator = observe(deviceVoltages + 3);
        if (hpGain > 0)
            hpState += 2 * hpGain * (observe(deviceVoltages + 4) - phaseVoltage[deviceVoltages + 4] - hpState);
        else if (!configured && controls.sidechainHz > 0)
            hpState = observe(deviceVoltages + 4);
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
    circuit->configured = false;
    circuit->hpState = 0;
    circuit->phaseVoltage.fill(0);
    circuit->phaseCurrent.fill(0);
#ifndef UA176_PHASE_REFERENCE
    circuit->phaseHistory.fill(0);
    circuit->phaseState = {};
    circuit->phaseHighPass.fill(0);
    circuit->phaseCursor = 0;
    circuit->phaseBias = 0;
    circuit->phaseBiasBlend = 1 - std::exp(-1 / (.0065 * rate));
#endif
    if (!circuit->setup(knobs, true))
        return false;
    double audio = 0, cv = 0;
    for (int i = 1; i <= 40; ++i)
        if (!circuit->advance(0, i / 40., 60, audio, cv))
            return false;
#ifndef UA176_PHASE_REFERENCE
    circuit->phaseBias = cv;
#endif
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
double Live176::interstage() const noexcept { return circuit->observe(deviceVoltages + 5); }

#ifdef UA176_PHASE_REFERENCE
bool Live176::exportPhaseReference(const char* path, double bias)
{
    Live176 live;
    auto* file = std::fopen(path, "wb");
    if (!file)
        return false;
    live.circuit->phaseReference = file;
    live.circuit->referenceBias = bias;
    LiveControls controls;
    controls.tap = static_cast<float>(referenceTap);
    controls.biasResistance = 6800;
    controls.limiting = bias != 0;
    controls.output = 10;
    const bool ready = live.prepare(44100, controls);
    return std::fclose(file) == 0 && ready;
}
#endif

}
