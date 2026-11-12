#pragma once
// TUBE COMP realtime DSP — intentionally NOT a SPICE solver.
// 6BC8 remote-cutoff approximation -> rectified 6AL5 feedback control ->
// 6CG7 output amplifier approximation. Factory Altec and reconstructed RS124.
// Coefficients and all nominal stage values are source-labelled and must be
// calibrated against legitimate ngspice/hardware measurements before claiming
// original-device accuracy. No allocations, file I/O, locks or exp() per sample.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace tubecomp {
constexpr float pi = 3.14159265358979323846f;
inline float clamp(float v,float a,float b) noexcept {return std::max(a,std::min(v,b));}
inline float db2lin(float dB) noexcept {return std::pow(10.f,dB/20.f);}
inline float lin2db(float x) noexcept {return 20.f*std::log10(std::max(x,1.e-14f));}
inline float coefficient(float ms,float sr) noexcept {
    return 1.f-std::exp(-1000.f/(std::max(.02f,ms)*sr));
}
// Clockwise attack 1 fastest -> 9 slowest.
// PROVISIONAL ngspice control-network RC candidates, NOT Chandler measured times.
static constexpr std::array<float,9> rsAttackMs = {
    12.f,22.f,33.f,47.f,68.f,100.f,150.f,220.f,330.f
};
// Recovery #1 fastest -> #6 slowest, based on SPICE 180k..3.3M / 1uF.
static constexpr std::array<float,6> rsReleaseMs = {
    180.f,330.f,680.f,1000.f,1800.f,3300.f
};
// Physical switch from left to right: slowest #6, HOLD, #5, HOLD, ... #1.
// 0,2,4,6,8,10 -> #6,#5,#4,#3,#2,#1; odd positions -> HOLD.
inline bool recoveryIsHold(int position) noexcept {return (position&1)!=0;}
inline int recoveryNumber(int position) noexcept {
    return 6-std::clamp(position,0,10)/2;
}

struct Parameters {
    bool rs124=true;
    float input=5.f;       // 0..10 knob, calibrated 5 means nominal input gain
    float output=0.f;      // RS124 output attenuator -10..0 dB
    int attack=3;          // 0..8; RS124 1..9
    int recovery=6;        // 0..10, even: release, odd: HOLD
    float balance=0.f;     // push-pull trim; -1..+1
    bool superfuse=false;  // optional reinterpretation (unverified proprietary)
    float wet=1.f;
    bool bypass=false;
    float stereoLink=1.f;  // linked by default
    float outputOhms=600.f;
};

struct Meter {
    float gainReduction=0.f, inputPeak=0.f, outputPeak=0.f;
};

// Cheap highpass for coupling networks and DC removal. 1-pole digital HP,
// corner corresponds approximately to 0.022uF / known grid leak values.
class HighPass {
public:
    void configure(float freq,float sampleRate) noexcept {
        alpha=std::exp(-2.f*pi*freq/sampleRate);
    }
    float tick(float x) noexcept {
        const float y=alpha*(y1+x-x1);
        x1=x;y1=std::fabs(y)<1.e-24f?0.f:y;return y1;
    }
    void reset() noexcept{x1=y1=0.f;}
private:
    float alpha=.99f,x1=0.f,y1=0.f;
};
class LowPass {
public:
    void configure(float freq,float sampleRate) noexcept {
        alpha=1.f-std::exp(-2.f*pi*freq/sampleRate);
    }
    float tick(float x) noexcept {
        y+=alpha*(x-y);
        if(std::fabs(y)<1.e-24f)y=0.f;
        return y;
    }
    void reset() noexcept{y=0.f;}
private:
    float alpha=.9f,y=0.f;
};

// DC-blocked nonlinear plate stages. A finite-power series approximates the
// local device transfer at each operating point. Shape coefficients vary with
// gain-reduction voltage and bias imbalance rather than staying a fixed static
// "tube saturator". Values are exploratory calibration parameters.
class TubeCell {
public:
    void prepare(float fs) noexcept {dc.configure(3.f,fs);reset();}
    void reset() noexcept {dc.reset();}
    // z is normalized to typical high-level tube drive. 6BC8 stage is
    // variable-mu and becomes increasingly asymmetric under high GR.
    float varMu(float x,float gr,float balance,bool rs) noexcept {
        const float z=clamp(x*.90f,-3.0f,3.0f);
        const float bias=clamp(gr/30.f,0.f,1.f);
        const float even=(rs?.00010f:.00008f)+balance*.013f+bias*.00004f;
        const float odd=(rs?-.0095f:-.0088f)-bias*.0025f;
        // Local 6BC8 transfer and push-pull imbalance.
        const float y=z+even*z*z+odd*z*z*z;
        return dc.tick(y/.90f);
    }
    // 6CG7 push-pull output stage: lower K2, moderate K3 at high level;
    // voltage swing depends on output load (200/600ohm -> slight level loss).
    float outputAmp(float x,float gr,float balance,float impedance) noexcept {
        const float z=clamp(x*.77f,-3.1f,3.1f);
        const float e=.00009f+balance*.0045f;
        const float o=-.012f-.0012f*clamp(gr/30.f,0.f,1.f);
        const float load=impedance<=300.f?.985f:1.f;
        return dc.tick((z+e*z*z+o*z*z*z)/.77f)*load;
    }
private:
    HighPass dc;
};

class Engine final {
public:
    void prepare(float sampleRate,int) noexcept {
        fs=std::max(8000.f,sampleRate);
        peakCoef=coefficient(.55f,fs);
        inputSmooth=coefficient(9.f,fs);
        outputSmooth=coefficient(9.f,fs);
        bypassSmooth=coefficient(7.f,fs);
        for(int c=0;c<2;++c){
            inputHP[(size_t)c].configure(11.f,fs);
            couplingHP[(size_t)c].configure(8.f,fs);
            outLP[(size_t)c].configure(std::min(25000.f,fs*.46f),fs);
            cells[(size_t)c][0].prepare(fs);
            cells[(size_t)c][1].prepare(fs);
        }
        reset();
        setParameters(params,true);
    }
    void reset() noexcept {
        cv={0.f,0.f};
        detector={0.f,0.f};
        inLevel=outLevel=1.f;wetLevel=1.f;
        for(int c=0;c<2;++c){
            inputHP[(size_t)c].reset();
            couplingHP[(size_t)c].reset();
            outLP[(size_t)c].reset();
            for(auto& cell:cells[(size_t)c])cell.reset();
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
        targetInput=db2lin((p.input-5.f)*4.5f);
        targetOutput=db2lin(p.rs124?p.output:0.f);
        targetWet=p.bypass?0.f:p.wet;
        attack63=p.rs124?rsAttackMs[(size_t)p.attack]:50.f;
        release63=p.rs124?rsReleaseMs[(size_t)(recoveryNumber(p.recovery)-1)]:1000.f;
        attackAlpha=coefficient(attack63,fs);
        releaseAlpha=coefficient(release63,fs);
        if(immediate) {
            inLevel=targetInput;outLevel=targetOutput;wetLevel=targetWet;
        }
    }
    float attackMs() const noexcept {return attack63;}
    float releaseMs() const noexcept {return release63;}
    bool holdEnabled() const noexcept {
        return params.rs124 && recoveryIsHold(params.recovery);
    }
    void process(float& left,float& right) noexcept {
        // Preamp gain (INPUT increases compression and tube drive).
        inLevel+=(targetInput-inLevel)*inputSmooth;
        outLevel+=(targetOutput-outLevel)*outputSmooth;
        wetLevel+=(targetWet-wetLevel)*bypassSmooth;
        const float dry[2]={left,right};
        const float source[2]={left*inLevel,right*inLevel};
        float drive[2],targetGR[2];
        // Rectifier envelope is derived from the signal before GR.
        for(int c=0;c<2;++c) {
            drive[c]=inputHP[(size_t)c].tick(source[c]);
            const float value=std::fabs(drive[c]);
            detector[(size_t)c]+=peakCoef*(value-detector[(size_t)c]);
            const float measured=std::max(1.e-8f,detector[(size_t)c]);
            // Variable-mu soft knee. Fixed internal threshold; INPUT varies
            // drive relative to it, as in historic amplifier.
            const float over=lin2db(measured)-threshold();
            const float knee=6.f;
            const float inKnee=clamp((over+knee*.5f)/knee,0.f,1.f);
            const float softOver=over>knee*.5f?over:
                  (over < -knee*.5f ? 0.f:(over+knee*.5f)*inKnee*.5f);
            const float compression=clamp(softOver/34.f,0.f,1.f);
            const float ratio=3.0f+3.5f*compression+
                              (params.rs124?.3f:0.f);
            targetGR[c]=clamp(softOver*(1.f-1.f/ratio),0.f,
                              params.superfuse&&params.rs124?34.f:30.f);
        }
        const float joint=std::max(targetGR[0],targetGR[1]);
        float wet[2];
        for(int c=0;c<2;++c) {
            const float aim=targetGR[c]*(1.f-params.stereoLink)+joint*params.stereoLink;
            const float current=cv[(size_t)c];
            if(aim>current) {
                cv[(size_t)c]+=(aim-current)*attackAlpha;
            } else if(!holdEnabled()) {
                cv[(size_t)c]+=(aim-current)*releaseAlpha;
            }
            const float gr=clamp(cv[(size_t)c],0.f,34.f);
            const float variableMuGain=db2lin(-gr);
            // Distortion produced by the input gain cell and modulated with
            // sidechain bias rather than one post-compressor static saturator.
            const float t1=cells[(size_t)c][0].varMu(
                 drive[c],gr,params.balance,params.rs124);
            const float vca=t1*variableMuGain;
            const float coupling=couplingHP[(size_t)c].tick(vca);
            const float t2=cells[(size_t)c][1].outputAmp(
                 coupling,gr,params.balance,
                 params.rs124?params.outputOhms:600.f);
            const float filtered=outLP[(size_t)c].tick(t2)*outLevel;
            // Mix compensates preamp boost only at 100% wet; zero mix is
            // truly original input and unprocessed.
            wet[c]=dry[c]+wetLevel*(filtered-dry[c]);
        }
        left=std::isfinite(wet[0])?wet[0]:0.f;
        right=std::isfinite(wet[1])?wet[1]:0.f;
        lastMeter.gainReduction=std::max(cv[0],cv[1]);
        lastMeter.inputPeak=std::max(std::fabs(dry[0]),std::fabs(dry[1]));
        lastMeter.outputPeak=std::max(std::fabs(left),std::fabs(right));
    }
    Meter meters() const noexcept{return lastMeter;}
private:
    float threshold() const noexcept {
        // These calibration targets refer to nominal +4dBu modeled as
        // -18dBFS in a 24-bit DAW, not absolute hardware dBu.
        // Substitution of measured Altec and RS124 values remains pending.
        return params.rs124?-28.f:-25.5f;
    }
    float fs=44100.f;
    Parameters params;
    Meter lastMeter {};
    std::array<float,2> detector{},cv{};
    float peakCoef=.1f,inputSmooth=.001f,outputSmooth=.001f,bypassSmooth=.001f;
    float inLevel=1.f,outLevel=1.f,wetLevel=1.f;
    float targetInput=1.f,targetOutput=1.f,targetWet=1.f;
    float attack63=50.f,release63=1000.f,attackAlpha=.0005f,releaseAlpha=.0001f;
    std::array<HighPass,2> inputHP,couplingHP;
    std::array<LowPass,2> outLP;
    std::array<std::array<TubeCell,2>,2> cells;
};
} // namespace tubecomp
