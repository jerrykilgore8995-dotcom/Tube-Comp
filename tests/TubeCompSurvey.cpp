// Dense, repeatable offline measurement lab for a low-CPU RS124/A-436B DSP.
// Values are measurements OF THE DSP. They are not measurements of real hardware.
#include "TubeCompDSP.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
using namespace tubecomp;
constexpr float SR=44100.f;

struct Spectra {
    float gr=0,peakOutput=0,peakInput=0;
    std::array<double,21> harmonics {};
    double thd=0;
    double gain=0;
};
Spectra measure(const Parameters& p,float hz,float peak) {
    Engine engine;engine.prepare(SR,256);engine.setParameters(p,true);
    const int prep=(int)(2.f*SR);
    for(int i=0;i<prep;++i){
        float s=peak*std::sin(2*pi*hz*i/SR),l=s,r=s;
        engine.process(l,r);
    }
    // 0.2s coherent window at sample rate 44.1k, 100/1k/5k/10k Hz tones.
    constexpr int count=8820;
    std::array<float,count> recorded{};
    Spectra result;
    for(int i=0;i<count;++i){
        float s=peak*std::sin(2*pi*hz*i/SR),l=s,r=s;
        engine.process(l,r);
        recorded[(size_t)i]=l;
        result.peakInput=std::max(result.peakInput,std::fabs(s));
        result.peakOutput=std::max(result.peakOutput,std::fabs(l));
    }
    result.gr=engine.meters().gainReduction;
    for(int k=1;k<=20;++k) {
        if(k*hz >= SR*.49)continue;
        double re=0,im=0;
        const double twopi=2.*3.14159265358979323846;
        // k*hz*window/SR is integer cycles, hence coherent projection.
        for(int n=0;n<count;++n) {
            double angle=twopi*double(k)*hz*n/SR;
            re+=recorded[(size_t)n]*std::cos(angle);
            im+=recorded[(size_t)n]*std::sin(angle);
        }
        result.harmonics[(size_t)k]=2*std::hypot(re,im)/count;
    }
    const double fundamental=std::max(result.harmonics[1],1.e-15);
    double sum=0;
    for(int k=2;k<=20;++k)
        sum+=result.harmonics[(size_t)k]*result.harmonics[(size_t)k];
    result.thd=100.*std::sqrt(sum)/fundamental;
    result.gain=20*std::log10(fundamental/std::max(peak,1.e-15f));
    return result;
}
void sweepSpectra() {
    std::ofstream out("harmonic_grid.csv");
    out<<"mode,input,outputDb,balance,attack,recoverySelector,hold,frequencyHz,"
          "sourcePeakDbFS,grDb,transferGainDb,THD_percent,peakOutput";
    for(int k=2;k<=20;++k)out<<",K"<<k<<"_dBc";
    out<<"\n";
    const std::array<float,5> levels={-30.f,-24.f,-18.f,-12.f,-6.f};
    const std::array<float,4> frequencies={100.f,1000.f,5000.f,10000.f};
    int rows=0;
    for(bool rs:{false,true})
    for(float input:{5.f,8.f})
    for(float bal:{0.f,.5f})
    for(float hz:frequencies)
    for(float db:levels) {
        Parameters p;p.rs124=rs;p.input=input;p.balance=bal;
        p.attack=3;p.recovery=6;p.output=0;
        const float peak=db2lin(db);
        const auto result=measure(p,hz,peak);
        out<<(rs?"RS124":"A-436B")<<","<<input<<","<<p.output<<","
           <<bal<<","<<(p.attack+1)<<","<<p.recovery<<",0,"
           <<hz<<","<<db<<","<<result.gr<<","
           <<result.gain<<","<<result.thd<<","<<result.peakOutput;
        for(int k=2;k<=20;++k) {
            const double a=result.harmonics[(size_t)k];
            if(k*hz>=SR*.49)out<<",";
            else out<<","<<20*std::log10(std::max(a,1.e-16)/
                                        std::max(result.harmonics[1],1.e-16));
        }
        out<<"\n";++rows;
    }
    std::cout<<"Wrote "<<rows<<" output THD and harmonic scenarios\n";
}
struct Timing {
    double attackMs=0,releaseMs=0,grDb=0;
};
Timing timeResponse(Parameters p) {
    Engine e;e.prepare(SR,256);e.setParameters(p,true);
    auto play=[&](float peak,int i) {
        float v=peak*std::sin(2*pi*1000.f*i/SR),a=v,b=v;
        e.process(a,b);
    };
    // Stabilize with silence, then 2 seconds of a moderately strong 1k signal.
    for(int i=0;i<(int)SR/3;++i)play(0,i);
    std::vector<float> gr;
    gr.reserve((size_t)SR*2);
    for(int i=0;i<(int)SR*2;++i){
        play(.40f,i);gr.push_back(e.meters().gainReduction);
    }
    Timing result;
    result.grDb=gr.back();
    const float gate=.632f*(float)result.grDb;
    result.attackMs=std::numeric_limits<double>::quiet_NaN();
    for(size_t i=0;i<gr.size();++i) {
        if(gr[i]>=gate){result.attackMs=1000.*double(i)/SR;break;}
    }
    const float recoveryThreshold=.368f*(float)result.grDb;
    result.releaseMs=std::numeric_limits<double>::quiet_NaN();
    for(int i=0;i<(int)SR*14;++i){
        play(0.f,i);
        if(e.meters().gainReduction<=recoveryThreshold) {
            result.releaseMs=1000.*i/SR;break;
        }
    }
    return result;
}
void sweepTiming() {
    std::ofstream out("timing_grid.csv");
    out<<"mode,attackPosition,recoveryPosition,recoveryNumber,hold,"
          "configuredAttack63ms,configuredRelease63ms,"
          "measuredAttack63ms,measuredRelease63ms,steadyGRdB\n";
    int n=0;
    for(bool rs:{false,true}) {
        const int attacks=rs?9:1;
        const int recoveries=rs?11:1;
        for(int a=0;a<attacks;++a)
        for(int r=0;r<recoveries;++r){
            Parameters p;p.rs124=rs;p.input=8.f;p.attack=a;p.recovery=r;
            Engine data;data.prepare(SR,256);data.setParameters(p,true);
            const auto t=timeResponse(p);
            out<<(rs?"RS124":"A-436B")<<","<<(a+1)<<","<<r<<","
               <<(rs?recoveryNumber(r):0)<<","<<(rs?recoveryIsHold(r):false)
               <<","<<data.attackMs()<<","<<data.releaseMs()
               <<","<<t.attackMs<<","<<t.releaseMs<<","<<t.grDb<<"\n";
            ++n;
        }
    }
    std::cout<<"Wrote "<<n<<" attack/recovery/HOLD measurements\n";
}
int main(){
    const auto start=std::chrono::steady_clock::now();
    sweepSpectra();sweepTiming();
    const double sec=std::chrono::duration<double>(
      std::chrono::steady_clock::now()-start).count();
    std::cout<<"ALL DSP SURVEYS FINISHED IN "<<sec<<" seconds\n";
}
