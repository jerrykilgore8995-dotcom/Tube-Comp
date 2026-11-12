#pragma once
// TubeComp circuit-calibration targets. ALL values below are recorded
// 1kHz ngspice .tran measurements (NOT actual vintage hardware).
// -18..+3 dBu: 0.22 second SPICE source-ramped run (not fully B+ settled).
// +7dBu and A-436B +10/+13dBu: 1.6 second supply-settled SPICE run.
// This mixed-settling grid is NOT accepted as universal 5% calibration.
// Source: TubeComp 2026-11-12 SPICE workflow artifacts. Do not edit values
// merely to make a software test pass; regenerate from reference CSV instead.
#include <array>
#include <algorithm>
#include <cmath>

namespace tubecomp::spice {
struct Point {
    float inputDbu, outputDbu, cvMagnitude;
    std::array<float,10> harmonicDb; // K2..K11, dBc of fundamental.
};
template<std::size_t N>
inline Point at(const std::array<Point,N>& grid,float input) noexcept {
    if(input<=grid.front().inputDbu)return grid.front();
    if(input>=grid.back().inputDbu)return grid.back();
    for(std::size_t j=1;j<N;++j){
        if(input<=grid[j].inputDbu){
            const auto& a=grid[j-1];const auto& b=grid[j];
            const float t=(input-a.inputDbu)/(b.inputDbu-a.inputDbu);
            Point v {};
            v.inputDbu=input;
            v.outputDbu=a.outputDbu+(b.outputDbu-a.outputDbu)*t;
            v.cvMagnitude=a.cvMagnitude+(b.cvMagnitude-a.cvMagnitude)*t;
            for(std::size_t k=0;k<10;++k)
                v.harmonicDb[k]=a.harmonicDb[k]+(b.harmonicDb[k]-a.harmonicDb[k])*t;
            return v;
        }
    }
    return grid.back();
}
inline Point blend(Point a,Point b,float t) noexcept {
    t=std::clamp(t,0.f,1.f);
    a.outputDbu+=(b.outputDbu-a.outputDbu)*t;
    a.cvMagnitude+=(b.cvMagnitude-a.cvMagnitude)*t;
    for(std::size_t i=0;i<10;++i)
        a.harmonicDb[i]+=(b.harmonicDb[i]-a.harmonicDb[i])*t;
    return a;
}
static constexpr std::array<Point,8> altec80 {{
{-36.f,1.31f,.59f,{-78.61f,-46.16f,-89.49f,-52.2f,-110.57f,-63.17f,-99.65f,-69.49f,-104.56f,-65.92f}},
{-24.f,8.04f,3.56f,{-78.61f,-46.16f,-89.49f,-52.2f,-110.57f,-63.17f,-99.65f,-69.49f,-104.56f,-65.92f}},
{-18.f,10.444f,5.453f,{-78.61f,-46.16f,-89.49f,-52.2f,-110.57f,-63.17f,-99.65f,-69.49f,-104.56f,-65.92f}},
{-12.f,12.441f,7.643f,{-82.49f,-44.91f,-95.28f,-51.95f,-103.96f,-63.50f,-104.34f,-68.05f,-118.40f,-65.42f}},
{-6.f,14.686f,10.390f,{-84.25f,-42.91f,-94.38f,-51.69f,-99.46f,-65.50f,-117.53f,-65.75f,-104.15f,-65.60f}},
{0.f,17.066f,14.066f,{-81.40f,-39.62f,-91.49f,-51.29f,-100.74f,-69.12f,-115.53f,-63.67f,-100.79f,-66.13f}},
{3.f,18.122f,16.139f,{-79.92f,-37.79f,-88.31f,-50.99f,-98.67f,-71.18f,-101.18f,-63.25f,-109.10f,-66.65f}},
{7.f,19.013f,21.917f,{-90.28f,-41.27f,-90.98f,-51.87f,-90.72f,-60.82f,-91.49f,-78.48f,-90.24f,-66.20f}}
}};
static constexpr std::array<Point,10> altec100 {{
{-36.f,3.25f,.70f,{-80.36f,-45.83f,-92.97f,-52.13f,-97.99f,-63.02f,-112.f,-69.62f,-105.38f,-65.95f}},
{-24.f,8.72f,4.04f,{-80.36f,-45.83f,-92.97f,-52.13f,-97.99f,-63.02f,-112.f,-69.62f,-105.38f,-65.95f}},
{-18.f,11.082f,6.131f,{-80.36f,-45.83f,-92.97f,-52.13f,-97.99f,-63.02f,-112.f,-69.62f,-105.38f,-65.95f}},
{-12.f,13.129f,8.443f,{-83.56f,-44.37f,-96.91f,-51.79f,-99.75f,-63.72f,-111.04f,-67.22f,-109.37f,-65.17f}},
{-6.f,15.470f,11.479f,{-83.29f,-41.96f,-94.54f,-51.51f,-102.83f,-66.26f,-109.86f,-64.85f,-107.18f,-65.43f}},
{0.f,17.762f,15.394f,{-80.67f,-38.43f,-89.75f,-51.12f,-95.66f,-70.5f,-120.54f,-63.35f,-106.85f,-66.53f}},
{3.f,18.736f,17.518f,{-79.4f,-36.69f,-88.21f,-50.65f,-96.88f,-72.32f,-102.18f,-63.17f,-106.69f,-66.98f}},
{7.f,19.512f,23.333f,{-91.21f,-41.0f,-91.36f,-51.26f,-90.22f,-60.78f,-90.97f,-77.65f,-91.66f,-66.11f}},
{10.f,20.237f,25.550f,{-90.85f,-41.67f,-91.70f,-49.65f,-90.25f,-60.47f,-90.64f,-77.15f,-91.27f,-65.85f}},
{13.f,20.919f,27.846f,{-91.17f,-46.35f,-90.11f,-46.63f,-90.83f,-59.31f,-91.89f,-76.89f,-90.10f,-65.72f}}
}};
static constexpr std::array<Point,8> rs80 {{
{-36.f,-2.66f,.48f,{-89.44f,-39.01f,-96.97f,-45.03f,-108.99f,-56.8f,-105.46f,-62.34f,-101.09f,-60.15f}},
{-24.f,4.75f,3.03f,{-89.44f,-39.01f,-96.97f,-45.03f,-108.99f,-56.8f,-105.46f,-62.34f,-101.09f,-60.15f}},
{-18.f,7.537f,4.759f,{-89.44f,-39.01f,-96.97f,-45.03f,-108.99f,-56.8f,-105.46f,-62.34f,-101.09f,-60.15f}},
{-12.f,9.867f,6.731f,{-105.28f,-37.77f,-103.85f,-44.70f,-109.99f,-57.83f,-110.21f,-59.95f,-101.91f,-59.70f}},
{-6.f,12.140f,9.152f,{-107.87f,-36.44f,-124.61f,-44.49f,-111.49f,-59.23f,-114.51f,-58.35f,-118.10f,-59.54f}},
{0.f,14.541f,12.447f,{-102.13f,-34.22f,-106.20f,-44.30f,-108.42f,-61.60f,-121.38f,-57.12f,-108.63f,-59.99f}},
{3.f,15.698f,14.381f,{-96.24f,-32.61f,-102.94f,-44.17f,-102.39f,-63.60f,-108.15f,-56.73f,-102.75f,-60.62f}},
{7.f,17.545f,19.195f,{-89.82f,-32.95f,-90.90f,-43.87f,-91.25f,-62.13f,-90.38f,-56.83f,-91.08f,-59.95f}}
}};
static constexpr std::array<Point,8> rs100 {{
{-36.f,-.72f,.55f,{-92.83f,-38.59f,-111.28f,-44.89f,-99.08f,-57.05f,-115.28f,-61.37f,-101.69f,-59.87f}},
{-24.f,5.45f,3.55f,{-92.83f,-38.59f,-111.28f,-44.89f,-99.08f,-57.05f,-115.28f,-61.37f,-101.69f,-59.87f}},
{-18.f,8.314f,5.370f,{-92.83f,-38.59f,-111.28f,-44.89f,-99.08f,-57.05f,-115.28f,-61.37f,-101.69f,-59.87f}},
{-12.f,10.593f,7.442f,{-101.68f,-37.38f,-104.76f,-44.63f,-106.20f,-58.20f,-104.53f,-59.53f,-110.48f,-59.69f}},
{-6.f,12.907f,10.110f,{-109.60f,-35.87f,-104.18f,-44.44f,-112.f,-59.78f,-106.97f,-58.01f,-103.17f,-59.63f}},
{0.f,15.298f,13.683f,{-98.66f,-33.21f,-102.58f,-44.23f,-114.77f,-62.85f,-111.40f,-56.84f,-110.86f,-60.23f}},
{3.f,16.396f,15.677f,{-94.79f,-31.42f,-100.83f,-43.96f,-109.19f,-65.23f,-106.91f,-56.47f,-108.70f,-61.16f}},
{7.f,18.118f,20.566f,{-90.78f,-32.09f,-90.77f,-43.44f,-90.78f,-63.04f,-90.75f,-56.37f,-90.73f,-59.89f}}
}};
inline Point lookup(bool rs,float inputKnob,float inputDbu) noexcept {
    const float k=std::clamp(inputKnob,0.f,10.f);
    // The actual dual-50k attenuator's near-zero wiper loading varies with
    // frequency. This input control extrapolation remains unverified <80%.
    float level=inputDbu;
    if(k<8.f)
        level+=20.f*std::log10(std::max(.001f,k)/8.f);
    const auto p80=rs?at(rs80,level):at(altec80,level);
    const auto p100=rs?at(rs100,level):at(altec100,level);
    auto result=blend(p80,p100,(k-8.f)/2.f);
    if(level< -36.f)
        result.outputDbu+=level+36.f;
    if(level> (k>=9.f && !rs?13.f:7.f))
        result.outputDbu+=.24f*(level-(k>=9.f && !rs?13.f:7.f));
    result.inputDbu=inputDbu;
    return result;
}
template<std::size_t N>
inline float detectorCvFromOutput(const std::array<Point,N>& a,float outputDbu) noexcept {
    if(outputDbu<=a.front().outputDbu)
        return std::max(0.f,a.front().cvMagnitude+
                          .20f*(outputDbu-a.front().outputDbu));
    for(std::size_t j=1;j<N;++j){
        if(outputDbu<=a[j].outputDbu){
            const float t=std::clamp((outputDbu-a[j-1].outputDbu)/
                   std::max(.01f,a[j].outputDbu-a[j-1].outputDbu),0.f,1.f);
            return a[j-1].cvMagnitude+
                        t*(a[j].cvMagnitude-a[j-1].cvMagnitude);
        }
    }
    const auto& end=a.back();
    return std::clamp(end.cvMagnitude+
                   2.0f*(outputDbu-end.outputDbu),0.f,39.f);
}
inline float feedbackCv(bool rs,float knob,float outputDbu) noexcept {
    // Detector follows the 6CG7 plate outputs before the RS124 output pad.
    // The pad-independent output measurement is mapped back to the SPICE
    // detector response. At non-sinusoidal input this is a proxy.
    const float from80=rs?detectorCvFromOutput(rs80,outputDbu):
                            detectorCvFromOutput(altec80,outputDbu);
    const float from100=rs?detectorCvFromOutput(rs100,outputDbu):
                             detectorCvFromOutput(altec100,outputDbu);
    const float t=std::clamp((knob-8.f)*.5f,0.f,1.f);
    return from80+(from100-from80)*t;
}
inline float attenuationDb(bool rs,float cv) noexcept {
    // Latest monotonic exponential 6BC8 gain-vs-control ngspice .AC results.
    static constexpr float grids[10]={0,3,6,9,12,15,18,22,26,30};
    static constexpr float altec[10]={0,4.9086f,9.2751f,13.3466f,
          16.6711f,19.5014f,22.2703f,26.1775f,30.3779f,34.8054f};
    static constexpr float rs124[10]={0,4.8322f,9.1913f,13.2219f,
          16.4986f,19.3136f,22.0878f,26.0100f,30.2236f,34.6604f};
    const auto* y=rs?rs124:altec;
    cv=std::clamp(cv,0.f,39.f);
    for(std::size_t i=1;i<10;++i)
        if(cv<=grids[i]){
            const float t=(cv-grids[i-1])/(grids[i]-grids[i-1]);
            return y[i-1]+t*(y[i]-y[i-1]);
        }
    return y[9]+(cv-30.f)*1.1f;
}

inline float linearGainDb(bool rs,float inputKnob) noexcept {
    const float base=rs?34.24f:38.35f;
    return base+20.f*std::log10(std::max(.001f,inputKnob)/8.f);
}
} // namespace tubecomp::spice
