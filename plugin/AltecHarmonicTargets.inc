#pragma once
namespace tubecomp {
// Generated calibration table is included by the build workflow.
#include "AltecHarmonicData.inc"
inline float altecHarmonicRatio(float knob,int k,float source,float original) noexcept {
    int a=std::clamp(int(knob),0,9);
    float t=std::clamp(knob-a,0.f,1.f);
    // Existing waveshaper has no frequency selector. Use approved 1kHz
    // reference for broadband operation rather than changing signal processing.
    const float v=altecData[1][a][k-2]+t*(altecData[1][a+1][k-2]-altecData[1][a][k-2]);
    return v;
}
}
