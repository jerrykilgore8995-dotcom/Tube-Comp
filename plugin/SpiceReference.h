#pragma once
// TubeComp reference values are independent fully-settled 1.6s ngspice
// measurements at 1kHz (NOT vintage hardware). This is still a sparse
// 80/100% input-control grid, NOT verified 8%-all-controls calibration.
// Source: TubeComp 2026-11-12 SPICE workflow artifacts. Do not edit values
// merely to make a software test pass; regenerate from reference CSV instead.
#include <array>
#include <algorithm>
#include <cmath>

namespace tubecomp::spice {
struct Point {
    float inputDbu, outputDbu, cvMagnitude, thdPercent;
    std::array<float,19> harmonicDb; // K2..K20 from settled ngspice reference.
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
            v.thdPercent=a.thdPercent+(b.thdPercent-a.thdPercent)*t;
            for(std::size_t k=0;k<19;++k)
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
    a.thdPercent+=(b.thdPercent-a.thdPercent)*t;
    for(std::size_t i=0;i<19;++i)
        a.harmonicDb[i]+=(b.harmonicDb[i]-a.harmonicDb[i])*t;
    return a;
}
// Provenance-locked 36-case, 1.6-second measured SPICE harmonic data.
#include "SpiceReferenceMeasured.inc"
// Real 0..100% dual-section 50k input attenuator curves at three levels.
#include "SpiceInputGridMeasured.inc"
inline Point measuredInputAt(bool rs,int index,float inputDbu) noexcept {
    const auto& bank=rs?measuredRSInput:measuredAltecInput;
    const auto& points=bank[static_cast<size_t>(std::clamp(index,0,10))];
    Point result=at(points,inputDbu);
    // Below/above the measured -18..+7dBu range use explicit EXTRAPOLATION,
    // never describe these out-of-grid levels as measured SPICE.
    if(inputDbu<-18.f){
        result.outputDbu+=inputDbu+18.f;
        result.cvMagnitude=std::max(0.f,result.cvMagnitude+
                                    .20f*(inputDbu+18.f));
    }
    if(inputDbu>7.f){
        const float d=std::max(1.f,points[2].inputDbu-points[1].inputDbu);
        result.outputDbu+=(inputDbu-7.f)*
                 (points[2].outputDbu-points[1].outputDbu)/d;
        result.cvMagnitude+=(inputDbu-7.f)*
                 (points[2].cvMagnitude-points[1].cvMagnitude)/d;
    }
    result.cvMagnitude=std::max(0.f,result.cvMagnitude);
    result.inputDbu=inputDbu;
    return result;
}
inline Point lookup(bool rs,float inputKnob,float inputDbu) noexcept {
    const float k=std::clamp(inputKnob,0.f,10.f);
    // Measured independent ngspice 11-position 50k dual-pot loading.
    // Below 80% use actual 0..70% SPICE, not a 20log(k/8) estimate.
    if(k<8.f){
        const int index=std::clamp(int(k),0,7);
        const Point lower=measuredInputAt(rs,index,inputDbu);
        // Use the existing 80% broad-level calibration as the endpoint to
        // avoid a sudden gain discontinuity at 80% of the physical dial.
        const Point upper=(index==7)?
            (rs?at(rs80,inputDbu):at(altec80,inputDbu)):
            measuredInputAt(rs,index+1,inputDbu);
        const float frac=std::clamp(k-float(index),0.f,1.f);
        auto result=blend(lower,upper,frac);
        result.inputDbu=inputDbu;
        return result;
    }
    const float level=inputDbu;
    const auto p80=rs?at(rs80,level):at(altec80,level);
    const auto p100=rs?at(rs100,level):at(altec100,level);
    auto result=blend(p80,p100,(k-8.f)/2.f);
    // Pot midpoint at 90% is not generally the arithmetic average of the
    // 80/100% harmonic spectra. Independently measured ngspice midpoint
    // corrects the weak K9/K19 residual without moving either calibrated
    // 80/100% endpoint. The correction is zero at each endpoint.
    if(k>8.f && k<10.f){
        const Point actual90=measuredInputAt(rs,9,inputDbu);
        const Point predicted90=blend(
              rs?at(rs80,level):at(altec80,level),
              rs?at(rs100,level):at(altec100,level),.5f);
        const float weight=std::clamp(1.f-std::fabs(k-9.f),0.f,1.f);
        for(size_t i=0;i<19;++i)
            result.harmonicDb[i]+=(actual90.harmonicDb[i]-
                                      predicted90.harmonicDb[i])*weight;
    }
    if(level< -24.f) {
        result.outputDbu+=level+24.f;
        result.cvMagnitude=std::max(0.f,result.cvMagnitude+.18f*(level+24.f));
    }
    if(level>13.f){
        result.outputDbu+=.24f*(level-13.f);
        result.cvMagnitude+=.72f*(level-13.f);
    }
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
    if(knob<8.f){
        const auto& bank=rs?measuredRSInput:measuredAltecInput;
        const int index=std::clamp(int(knob),0,7);
        const float a=detectorCvFromOutput(
                                  bank[static_cast<size_t>(index)],outputDbu);
        const float b=(index==7)?
                  (rs?detectorCvFromOutput(rs80,outputDbu):
                      detectorCvFromOutput(altec80,outputDbu)):
                   detectorCvFromOutput(
                       bank[static_cast<size_t>(index+1)],outputDbu);
        return std::max(0.f,a+(b-a)*std::clamp(knob-index,0.f,1.f));
    }
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
