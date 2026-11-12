#pragma once
// TubeComp real-time 6BC8 / 6CG7 feedback compressor prototype.
// Reference: SPICE-exploratory 436B/RS124 operating-point and output grids.
// CV is a negative-control-voltage MAGNITUDE in volts, NOT gain reduction dB.
// All sound processing is sample-by-sample, with no allocation or locks.
// A 6–8% all-controls SPICE match remains an acceptance requirement and is
// NOT established merely by compiling this approximation.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include "SpiceReference.h"
#include "SpicePhaseFIRMeasured.inc"

namespace tubecomp {
constexpr float pi=3.14159265358979323846f;
inline float clamp(float x,float lo,float hi) noexcept {return std::max(lo,std::min(x,hi));}
inline float db2lin(float x) noexcept {return std::exp2(x/6.020599913f);}
inline float lin2db(float x) noexcept {return 6.020599913f*std::log2(std::max(1.e-16f,x));}
inline float coeff(float ms,float fs) noexcept {
    return 1.f-std::exp(-1000.f/(std::max(.05f,ms)*fs));
}
static constexpr std::array<float,9> rsAttackMs={{
    12.f,22.f,33.f,47.f,68.f,100.f,150.f,220.f,330.f
}};
static constexpr std::array<float,6> rsReleaseMs={{
    180.f,330.f,680.f,1000.f,1800.f,3300.f
}};
inline bool recoveryIsHold(int p) noexcept {return (p&1)!=0;}
inline int recoveryNumber(int p) noexcept {return 6-std::clamp(p,0,10)/2;}
struct Parameters {
    bool rs124=true;
    float input=5.f,output=0.f;
    int attack=3,recovery=6;
    float balance=0.f;
    bool superfuse=false;
    float wet=1.f;
    bool bypass=false;
    float stereoLink=1.f;
    float outputOhms=600.f;
};
struct Meter {float gainReduction=0.f,inputPeak=0.f,outputPeak=0.f;};
struct StageProbe {float input=0.f,muPlate=0.f,gainCell=0.f,outputPlate=0.f,grDb=0.f;};

// Original SPICE input-to-output phase: approximately 2 cascaded 10Hz HP
// stages and -180 degrees of push-pull polarity inversion. Fractional delay
// models the residual ≈0.007–0.008ms winding / leakage group delay.
// This must be cross-checked with AC phase measurements across all CVs.
class HighPass {
public:
    void prepare(float fc,float sr) noexcept {
        alpha=std::exp(-2.f*pi*fc/sr);reset();
    }
    float tick(float x) noexcept {
        float y=alpha*(y1+x-x1);
        x1=x;y1=std::fabs(y)<1.e-25f?0.f:y;
        return y1;
    }
    void reset() noexcept {x1=0.f;y1=0.f;}
private:
    float alpha=.999f,x1=0.f,y1=0.f;
};
class LowPass {
public:
    void prepare(float hz,float sr) noexcept {
        a=1.f-std::exp(-2.f*pi*std::min(hz,.49f*sr)/sr);
        reset();
    }
    void setCoefficient(float value) noexcept {a=value;}
    float coefficient() const noexcept {return a;}
    float tick(float x) noexcept {
        y+=a*(x-y);
        if(std::fabs(y)<1.e-25f)y=0.f;
        return y;
    }
    void reset() noexcept {y=0.f;}
private:
    float a=.9f,y=0.f;
};
// Causal measured-response FIR. Real coefficients are source-generated from
// SPICE .AC complex response, not invented phase shifts. The FIR replaces
// both the previously approximate fractional allpass and the output LP.
class SpicePhaseFilter {
public:
    void reset() noexcept {hist.fill(0.f);pos=0;}
    void configure(bool rs,float cv) noexcept {
        const auto& bank=rs?phasefit::rs:phasefit::altec;
        const auto& voltages=phasefit::cv;
        cv=clamp(cv,0.f,17.f);
        size_t hi=1;
        while(hi<4&&cv>voltages[hi])++hi;
        const size_t lo=hi-1;
        const float t=(cv-voltages[lo])/(voltages[hi]-voltages[lo]);
        for(int k=0;k<phasefit::kLen;++k)
            taps[static_cast<size_t>(k)]=
               bank[lo][static_cast<size_t>(k)]+t*
               (bank[hi][static_cast<size_t>(k)]-
                bank[lo][static_cast<size_t>(k)]);
    }
    float tick(float x) noexcept {
        hist[static_cast<size_t>(pos)]=x;
        float y=0.f;
        int index=pos;
        for(int k=0;k<phasefit::kLen;++k){
            y+=taps[static_cast<size_t>(k)]*
               hist[static_cast<size_t>(index)];
            if(--index<0)index=phasefit::kLen-1;
        }
        if(++pos==phasefit::kLen)pos=0;
        return std::fabs(y)<1.e-24f?0.f:y;
    }
private:
    std::array<float,phasefit::kLen> taps{};
    std::array<float,phasefit::kLen> hist{};
    int pos=0;
};

// Chebyshev waveshaper for the K2..K11 harmonic-bin targets. Coefficients
// interpolate with input operating point, rather than a static K3 preset.
// Chebyshev of a normalized sine yields pure higher-order partials.
// This is NOT equivalent to independently verified 6CG7 plate equations.
// All nonlinear spectral targets must be assessed on the render at 44.1k.
inline float harmonics(float x,float peak,
                       const std::array<float,19>& gains) noexcept {
    peak=std::max(peak,1.e-8f);
    const float z=clamp(x/peak,-1.f,1.f);
    float prev=1.f,cur=z;
    float residue=0.f;
    for(int k=2;k<=20;++k){
        const float next=2.f*z*cur-prev;
        prev=cur;cur=next;
        residue+=gains[static_cast<size_t>(k-2)]*next;
    }
    return x+peak*residue;
}

class Engine final {
public:
    void prepare(float sampleRate,int) noexcept {
        fs=std::max(8000.f,sampleRate);
        inputEnvAlpha=coeff(8.f,fs);
        harmonicEnvAlpha=coeff(90.f,fs);
        outputEnvAlpha=coeff(5.f,fs);
        gainSmoothAlpha=coeff(1.8f,fs);
        wetSmoothAlpha=coeff(7.f,fs);
        outputSmoothAlpha=coeff(7.f,fs);
        for(int c=0;c<2;++c){
            hpf[c][0].prepare(9.7f,fs);
            hpf[c][1].prepare(9.7f,fs);
            phase[c].reset();
            phase[c].configure(params.rs124,0.f);
        }
        reset();
        setParameters(params,true);
    }
    void reset() noexcept {
        sampleCount=0;
        dryDelayIndex=0;
        for(auto& channel:dryDelay)channel.fill(0.f);
        cv={0.f,0.f};inputEnv={0.f,0.f};harmonicEnv={0.f,0.f};outputEnv={0.f,0.f};
        feedbackAudio={0.f,0.f};
        gainDb={30.f,30.f};
        sourceDbu={-36.f,-36.f};
        lastProbe={};lastMeter={};
        wetLevel=1.f;outputLevel=1.f;
        for(int c=0;c<2;++c){
            for(auto& filter:hpf[c])filter.reset();
            phase[c].reset();
        }
    }
    void setParameters(Parameters p,bool immediate=false) noexcept {
        p.input=clamp(p.input,0.f,10.f);
        p.output=clamp(p.output,-10.f,0.f);
        p.attack=std::clamp(p.attack,0,8);
        p.recovery=std::clamp(p.recovery,0,10);
        p.balance=clamp(p.balance,-1.f,1.f);
        p.wet=clamp(p.wet,0.f,1.f);
        p.stereoLink=clamp(p.stereoLink,0.f,1.f);
        params=p;
        attack63=p.rs124?rsAttackMs[static_cast<size_t>(p.attack)]:50.f;
        release63=p.rs124?
            rsReleaseMs[static_cast<size_t>(recoveryNumber(p.recovery)-1)]:1000.f;
        attackAlpha=coeff(attack63*(p.superfuse&&p.rs124?.75f:1.f),fs);
        releaseAlpha=coeff(release63,fs);
        targetOutput=db2lin(p.rs124?p.output:0.f);
        targetWet=p.bypass?0.f:p.wet;
        if(immediate){
            wetLevel=targetWet;outputLevel=targetOutput;
        }
    }
    float attackMs() const noexcept{return attack63;}
    float releaseMs() const noexcept{return release63;}
    bool holdEnabled() const noexcept {
        return params.rs124 && recoveryIsHold(params.recovery);
    }
    void process(float& left,float& right) noexcept {
        ++sampleCount;
        const bool update=(sampleCount%16)==0;
        wetLevel+=(targetWet-wetLevel)*wetSmoothAlpha;
        outputLevel+=(targetOutput-outputLevel)*outputSmoothAlpha;
        const float dry[2]={left,right};
        float alignedDry[2]={};
        for(int c=0;c<2;++c){
            dryDelay[c][static_cast<size_t>(dryDelayIndex)]=dry[c];
            alignedDry[c]=dryDelay[c][static_cast<size_t>(
                         (dryDelayIndex+1)%(phasefit::kLatency+1))];
        }
        if(++dryDelayIndex==phasefit::kLatency+1)dryDelayIndex=0;
        float shaped[2]={};
        float targets[2]={};
        float inPeak[2]={};
        float desiredGainDb[2]={};
        float inAfterHp[2]={};
        // 6BC8 source loading is emulated by a gain proportional to the
        // matched dual 50k input attenuator. Never drive a zero input knob.
        for(int c=0;c<2;++c){
            const float x=dry[c];
            // First coupling pole before tube and one after its output.
            inAfterHp[c]=hpf[c][0].tick(x);
            inputEnv[c]+=inputEnvAlpha*(std::fabs(inAfterHp[c])-inputEnv[c]);
            harmonicEnv[c]+=harmonicEnvAlpha*(std::fabs(inAfterHp[c])-harmonicEnv[c]);
            // mean(abs(sine))=2/pi of peak. Explicit DAW convention:
            // -18 dBFS PEAK corresponds to +4 dBu, so 0 dBFS = +22 dBu.
            inPeak[c]=std::max(1.e-8f,inputEnv[c]*(pi*.5f));
            if(update){
                sourceDbu[c]=22.f+lin2db(inPeak[c]);
                const float beforeOutputPad=std::max(
                      1.e-9f,outputEnv[c]*(pi*.5f));
                const float preOutputDbu=22.f+lin2db(beforeOutputPad);
                const float padDbu=params.rs124?lin2db(.65f):0.f;
                targets[c]=spice::feedbackCv(params.rs124,params.input,
                                           preOutputDbu+padDbu);
                if(params.superfuse&&params.rs124)
                    targets[c]=std::min(39.f,targets[c]*1.12f);
            }
        }
        // The control circuit samples the signal AFTER the 6CG7, before
        // any RS124 output pad. Linked stereo uses stronger feedback envelope.
        if(update){
            const float linked=std::max(targets[0],targets[1]);
            for(int c=0;c<2;++c)
                heldAim[c]=targets[c]*(1.f-params.stereoLink)+
                            linked*params.stereoLink;
        }
        for(int c=0;c<2;++c){
            const float goal=heldAim[c];
            if(calibrationCv>=0.f)cv[c]=calibrationCv;
            else {
                if(goal>cv[c])cv[c]+=(goal-cv[c])*attackAlpha;
                else if(!holdEnabled())cv[c]+=(goal-cv[c])*releaseAlpha;
                cv[c]=clamp(cv[c],0.f,39.f);
            }
            if(update){
                const auto target=spice::lookup(params.rs124,params.input,
                                                sourceDbu[c]);
                const float refCv=target.cvMagnitude;
                // Tube transfer gain vs negative 6BC8 grid bias is measured
                // by ngspice; this is a correction to the reference output.
                // At steady-state CV=reference CV it matches measured gain.
                const float dynamicCorrection=
                    spice::attenuationDb(params.rs124,refCv)-
                    spice::attenuationDb(params.rs124,cv[c]);
                // RS124 SPICE reference already includes its nominal .65
                // post-output pad. Feedback is sampled upstream of the pad.
                const float padCorrection=params.rs124?-lin2db(.65f):0.f;
                desiredGainDb[c]=target.outputDbu-sourceDbu[c]+
                                 dynamicCorrection+padCorrection;
                desiredGain[c]=clamp(desiredGainDb[c],-65.f,65.f);
                harmonicTargets[c]=target;
                for(int k=2;k<=20;++k){
                    float ratio=std::exp2(target.harmonicDb[
                                       static_cast<size_t>(k-2)]/
                                          6.020599913f);
                    if((k%2)==0)
                        ratio*=clamp(1.f+params.balance*
                            (params.rs124?1.3f:1.1f),.05f,3.5f);
                    // Match measured 44.1kHz high-order response: the
                    // digital output low-pass and amplitude tracking
                    // attenuate high K more than the 1kHz SPICE window.
                    // Quadratic pre-emphasis is FIT to the output-bin grid,
                    // not a claim about original 6CG7 plate physics.
                    const float kFraction=float(k)/20.f;
                    ratio*=1.f+.18f*kFraction*kFraction;
                    harmonicLinear[c][static_cast<size_t>(k-2)]=ratio;
                }
                phase[c].configure(params.rs124,cv[c]);
            }
            gainDb[c]+=(desiredGain[c]-gainDb[c])*gainSmoothAlpha;
            // Signal remains bipolar and is never rectified into the audio.
            const float beforePad=inAfterHp[c]*db2lin(gainDb[c]);
            shaped[c]=beforePad;
            // The waveshaper's Chebyshev basis needs a steady peak estimate.
            // A fast 8ms abs-envelope ripples at 2*f and cross-modulates
            // K7-K20; this slower normalizer avoids injecting fake partials.
            const float drivePeak=std::max(1.e-8f,harmonicEnv[c]*
                                 (pi*.5f))*db2lin(gainDb[c]);
            const float nonlinear=harmonics(beforePad,drivePeak,harmonicLinear[c]);
            // Plate polarity inversion is deliberate: phase should match
            // the negative push-pull polarity in the ngspice output.
            const float afterHP=hpf[c][1].tick(nonlinear);
            const float beforeOutputPad=-phase[c].tick(afterHP);
            outputEnv[c]+=outputEnvAlpha*
                 (std::fabs(beforeOutputPad)-outputEnv[c]);
            feedbackAudio[c]=beforeOutputPad;
            const float pad=params.rs124?.65f:1.f;
            const float wet=beforeOutputPad*pad*outputLevel;
            const float result=alignedDry[c]+wetLevel*(wet-alignedDry[c]);
            shaped[c]=std::isfinite(result)?result:0.f;
            if(c==0)lastProbe={inAfterHp[c],beforePad,nonlinear,
                              beforeOutputPad,spice::attenuationDb(
                                    params.rs124,cv[c])};
        }
        left=shaped[0];right=shaped[1];
        lastMeter.gainReduction=std::max(
            spice::attenuationDb(params.rs124,cv[0]),
            spice::attenuationDb(params.rs124,cv[1]));
        lastMeter.inputPeak=std::max(std::fabs(dry[0]),std::fabs(dry[1]));
        lastMeter.outputPeak=std::max(std::fabs(left),std::fabs(right));
    }
    Meter meters() const noexcept{return lastMeter;}
    float controlVolts() const noexcept{return std::max(cv[0],cv[1]);}
    // Offline calibration only: force a static 6BC8 control voltage as in
    // SPICE .AC; not exposed as a user plugin parameter.
    void setCalibrationControlVolts(float cvMagnitude) noexcept {
        calibrationCv=cvMagnitude<0.f?-1.f:clamp(cvMagnitude,0.f,39.f);
    }
    StageProbe stageProbe() const noexcept{return lastProbe;}
private:
    float fs=44100.f;
    float calibrationCv=-1.f;
    Parameters params;
    Meter lastMeter{};
    StageProbe lastProbe{};
    unsigned sampleCount=0;
    int dryDelayIndex=0;
    std::array<std::array<float,phasefit::kLatency+1>,2> dryDelay{};
    float inputEnvAlpha=.01f,harmonicEnvAlpha=.001f,outputEnvAlpha=.01f;
    float gainSmoothAlpha=.01f,wetSmoothAlpha=.01f,outputSmoothAlpha=.01f;
    float attack63=50.f,release63=1000.f,attackAlpha=.001f,releaseAlpha=.001f;
    float targetOutput=1.f,targetWet=1.f,outputLevel=1.f,wetLevel=1.f;
    std::array<float,2> inputEnv{},harmonicEnv{},outputEnv{},feedbackAudio{},cv{},heldAim{};
    std::array<float,2> gainDb{},desiredGain{},sourceDbu{};
    std::array<spice::Point,2> harmonicTargets{};
    std::array<std::array<float,19>,2> harmonicLinear{};
    std::array<std::array<HighPass,2>,2> hpf;
    std::array<SpicePhaseFilter,2> phase;
};
} // namespace tubecomp
