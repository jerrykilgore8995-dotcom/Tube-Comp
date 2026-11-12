#include "TubeCompDSP.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
using namespace tubecomp;
constexpr float SR=44100.f;
void check(bool yes,const char* reason){
    if(!yes){std::cerr<<"FAIL: "<<reason<<"\n";std::exit(1);}
}
void run(Engine& eng,float level,int n,float frequency=1000.f){
    for(int i=0;i<n;++i){
        const float x=level*std::sin(2*pi*frequency*i/SR);
        float a=x,b=x;
        eng.process(a,b);
        check(std::isfinite(a)&&std::isfinite(b),"finite output");
    }
}
float rise(Parameters p,float ms){
    Engine eng;eng.prepare(SR,128);eng.setParameters(p,true);
    run(eng,.50f,(int)(SR*.35f));
    return eng.meters().gainReduction;
}
struct Distortion{double thd=0;std::array<double,21> k{};float gr=0;};
Distortion measure(Parameters p,float peak,float hz=1000.f){
    Engine eng;eng.prepare(SR,128);eng.setParameters(p,true);
    const int warm=(int)(SR*4.f);
    for(int i=0;i<warm;++i){
        const float x=peak*std::sin(2*pi*hz*(float)i/SR);
        float l=x,r=x;eng.process(l,r);
    }
    constexpr int N=44100;
    std::array<double,N> samples{};
    for(int i=0;i<N;++i){
        const float x=peak*std::sin(2*pi*hz*(float)i/SR);
        float l=x,r=x;eng.process(l,r);
        samples[(size_t)i]=l;
    }
    Distortion result;result.gr=eng.meters().gainReduction;
    const double twopi=6.2831853071795864769;
    for(int h=1;h<=20;++h){
        const double harmonic=hz*h;
        if(harmonic>=SR/2-1)break;
        double re=0,im=0;
        for(int n=0;n<N;++n){
            const double angle=twopi*harmonic*n/SR;
            re+=samples[(size_t)n]*std::cos(angle);
            im+=samples[(size_t)n]*std::sin(angle);
        }
        result.k[(size_t)h]=2*std::hypot(re,im)/N;
    }
    double total=0;
    for(int h=2;h<=20;++h)total+=result.k[(size_t)h]*result.k[(size_t)h];
    result.thd=100*std::sqrt(total)/std::max(1.e-18,result.k[1]);
    return result;
}
}
int main() {
    static_assert(rsAttackMs.size()==9,"all RS124 attack positions");
    static_assert(rsReleaseMs.size()==6,"all RS124 recovery positions");
    for(size_t i=1;i<rsAttackMs.size();++i)
        check(rsAttackMs[i]>rsAttackMs[i-1],"attack positions ascend");
    for(size_t i=1;i<rsReleaseMs.size();++i)
        check(rsReleaseMs[i]>rsReleaseMs[i-1],"recovery 1 fastest to 6 slowest");
    for(int pos=0;pos<11;++pos){
        check(recoveryIsHold(pos)==(pos%2==1),"five HOLD positions interleaved");
        check(recoveryNumber(pos)==6-pos/2,"physical recovery mapping 6 to 1");
    }
    Parameters p;
    Engine a;
    a.prepare(SR,128);
    p.rs124=false;p.attack=8;p.recovery=0;
    a.setParameters(p,true);
    check(std::fabs(a.attackMs()-50.f)<.0001f,"Altec fixed 50ms attack");
    check(std::fabs(a.releaseMs()-1000.f)<.0001f,"Altec fixed 1s recovery");
    p.rs124=true;
    for(int pos=0;pos<9;++pos){
        p.attack=pos;a.setParameters(p);
        check(a.attackMs()==rsAttackMs[(size_t)pos],"9 physical attack values");
    }
    for(int pos=0;pos<11;++pos){
        p.recovery=pos;a.setParameters(p);
        check(a.releaseMs()==rsReleaseMs[(size_t)(recoveryNumber(pos)-1)],
              "11-step recovery values and HOLD pairing");
        check(a.holdEnabled()==(pos%2==1),"HOLD exact positions");
    }

    // Measurement: fast vs slow attack on the same program material.
    p.input=8.f;p.recovery=10;
    p.attack=0;
    const float fast=rise(p,80);
    p.attack=8;
    const float slow=rise(p,80);
    std::cout<<"RS124 attack test: fast="<<fast<<"dB slow="<<slow<<"dB\n";
    // This helper captures GR after 350ms; 12ms should exceed 330ms at 350ms.
    check(fast>slow+1.f,"nine-step attack changes real GR envelope");

    // HOLD must freeze the last achieved control voltage, not reset to unity.
    p.attack=0;p.recovery=6;
    Engine hold;hold.prepare(SR,128);hold.setParameters(p,true);
    run(hold,.5f,(int)(SR*1.5f));
    const float before=hold.meters().gainReduction;
    p.recovery=5;hold.setParameters(p);
    run(hold,0.f,(int)(SR*1.5f));
    const float after=hold.meters().gainReduction;
    std::cout<<"RS124 HOLD: before="<<before<<"dB after="<<after<<"dB\n";
    check(before>4.f&&std::fabs(after-before)<.03f,"HOLD freezes real compression");

    // All-pass-through dry mix and bypass, finite output and low CPU.
    p.rs124=true;p.recovery=10;p.wet=0.f;p.input=5.f;
    Engine dry;dry.prepare(SR,128);dry.setParameters(p,true);
    float error=0;
    for(int i=0;i<10000;++i){
        const float x=.13f*std::sin(i*.14f);
        float l=x,r=-x;dry.process(l,r);
        const float prev=i>=phasefit::kLatency?
           .13f*std::sin((i-phasefit::kLatency)*.14f):0.f;
        error=std::max(error,std::fabs(l-prev));
        error=std::max(error,std::fabs(r+prev));
    }
    std::cout<<"Dry mix max error "<<error<<"\n";
    check(error<1.e-6f,"0%-wet should null to latency-aligned dry audio");

    // Distortion at a representative practical level. All harmonic bins
    // must be generated by real DSP, not hard-coded "measurement" values.
    p.wet=1.f;p.input=7.f;p.balance=0.f;
    const auto d=measure(p,.17f);
    std::cout<<"1kHz / peak=0.17: GR="<<d.gr<<" dB THD="<<d.thd
             <<"% K2="<<d.k[2]<<" K3="<<d.k[3]<<"\n";
    check(d.gr>2.f,"compressor must actually compress");
    check(d.thd<2.5,"avoid excessive uncalibrated THD at working levels");
    check(d.k[2]>1.e-9&&d.k[3]>1.e-7,"must generate even and odd harmonics");
    std::cout<<"Balanced push-pull K2/K3: "<<d.k[2]/d.k[3]
             <<" (should be substantially K3 dominant)\n";
    check(d.k[3]>d.k[2]*4.0,"balanced push-pull K3 must dominate K2");

    p.input=10.f;
    Engine crush;crush.prepare(SR,128);crush.setParameters(p,true);
    run(crush,.9f,(int)(SR*3.f));
    std::cout<<"Max GR at hot input "<<crush.meters().gainReduction<<" dB\n";
    check(crush.meters().gainReduction>20.f,"can access 20+ dB gain reduction");

    // Output impedance, push-pull balance and 2 modes must change signal.
    p.input=6.f;p.balance=0.f;p.rs124=true;
    auto rs=measure(p,.12f);
    p.balance=.8f;
    auto unbalanced=measure(p,.12f);
    std::cout<<"Balance THD: centered "<<rs.thd<<" vs trimmed "<<unbalanced.thd<<"\n";
    check(std::fabs(unbalanced.k[2]-rs.k[2])>1.e-7,
          "push-pull BAL must affect K2");
    p.balance=0.f;p.rs124=false;
    auto altec=measure(p,.12f);
    std::cout<<"A-436B THD "<<altec.thd<<"%, RS124 "<<rs.thd<<"%\n";
    check(std::fabs(altec.thd-rs.thd)>1.e-5,"modes must differ electrically");

    // Factory THD guardrail at a deliberately hot, 25-30dB-GR scenario.
    // Tests use physical analog limits as goals, not as proof of exact hardware.
    p.balance=0.f;p.input=8.f;p.superfuse=false;
    p.rs124=true;
    const auto hotRS=measure(p,db2lin(-6.f));
    std::cout<<"RS124 hot-drive GR="<<hotRS.gr<<"dB THD="<<hotRS.thd<<"%\\n";
    // These limits only guard major regressions. SPICE parity is separately\n    // checked against the reference for 6-8% and must not be waived.\n    // This is a functional smoke test, not a historical acceptance test.
    // A single arbitrary -6 dBFS peak level cannot establish 25dB GR at the
    // Altec factory dBu test point. Exact matching belongs in calibration.
    check(hotRS.gr>20.f && hotRS.thd<4.5,
          "RS124 feedback-path smoke test: insufficient GR or excessive THD");
    p.rs124=false;
    const auto hotAltec=measure(p,db2lin(-6.f));
    std::cout<<"A-436B hot-drive GR="<<hotAltec.gr<<"dB THD="<<hotAltec.thd<<"%\\n";
    check(hotAltec.gr>20.f&&hotAltec.thd<1.5,
          "Stock A-436B hot-drive distortion exceeds 1.5 percent target");
    // The proprietary real SuperFuse is unavailable, but its explicitly
    // identified reconstruction must actually alter the audio compression.
    p.rs124=true;p.input=6.f;
    p.superfuse=false;
    Engine normal;normal.prepare(SR,128);normal.setParameters(p,true);
    run(normal,.18f,(int)SR);
    const float regularGR=normal.meters().gainReduction;
    p.superfuse=true;
    Engine fuse;fuse.prepare(SR,128);fuse.setParameters(p,true);
    run(fuse,.18f,(int)SR);
    const float fuseGR=fuse.meters().gainReduction;
    std::cout<<"SuperFuse reconstruction: normal="<<regularGR
             <<"dB / FUSE+="<<fuseGR<<"dB\\n";
    check(fuseGR>regularGR+.25f,"FUSE+ reconstruction must change actual compression");

    // Reference bench only: performance on GitHub runner is not a SER8 meter.
    p.rs124=true;p.input=5.f;
    Engine perf;perf.prepare(SR,128);perf.setParameters(p,true);
    const auto begin=std::chrono::steady_clock::now();
    for(int i=0;i<(int)(SR*3.f);++i){
        float l=.15f*std::sin(i*.05f),r=.14f*std::sin(i*.059f);
        perf.process(l,r);
    }
    const double sec=std::chrono::duration<double>(
          std::chrono::steady_clock::now()-begin).count();
    std::cout<<"3s stereo 44.1k DSP wall time "<<sec<<" sec\n";
    std::cout<<"PASS TubeComp DSP basic VST3 signal-path regressions\n";
}
