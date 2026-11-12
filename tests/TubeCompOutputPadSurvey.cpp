// Verify the SPICE BOUT post-detector output-pad identity against real DSP.
// In ngspice: BOUT out 0 V={OUTPAD*V(secOut)} (linear, after detector),
// so changing OUTPAD must not alter 6BC8 control, harmonic ratios or phase.
// The RS124 GUI 0..-10 dB is a RELATIVE pad atop the SPICE nominal .65.
// A436B factory mode must ignore the modern output control.
#include "TubeCompDSP.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

constexpr float sampleRate=44100.f;
constexpr int cycles=100;
constexpr int sampleCount=4410;
struct Measured{
    double outputDb=0,thd=0,phase=0;
    float cv=0;
    std::array<double,21> k{};
};
Measured render(bool rs,int inputPct,int inputDbu,float outputDb) {
    tubecomp::Engine engine;
    tubecomp::Parameters p;
    p.rs124=rs;p.input=inputPct*.1f;
    p.output=outputDb;
    p.attack=0;p.recovery=10;
    p.wet=1.f;p.balance=0.f;p.stereoLink=1.f;
    engine.prepare(sampleRate,256);
    engine.setParameters(p,true);
    const double peak=std::pow(10.,(inputDbu-22.)/20.);
    std::array<double,sampleCount> samples{};
    constexpr int total=int(sampleRate*1.6f);
    constexpr double pi=3.14159265358979323846;
    for(int i=0;i<total;++i){
        double t=double(i)/sampleRate;
        double fade=.5-.5*std::cos(pi*std::min(t/.04,1.));
        float signal=float(peak*std::sin(2*pi*1000.*t)*fade);
        float l=signal,r=signal;
        engine.process(l,r);
        if(i>=total-sampleCount)
            samples[size_t(i-(total-sampleCount))]=l;
    }
    Measured result;
    result.cv=engine.controlVolts();
    double totalThd=0.;
    for(int k=1;k<=20;++k){
        double re=0,im=0;
        for(int i=0;i<sampleCount;++i){
            const double ang=2*pi*k*1000.*double(i)/sampleRate;
            re+=samples[size_t(i)]*std::cos(ang);
            im-=samples[size_t(i)]*std::sin(ang);
        }
        result.k[size_t(k)]=2*std::hypot(re,im)/sampleCount;
        if(k==1){
            result.phase=std::atan2(im,re)*180/pi;
        }else{
            totalThd+=result.k[size_t(k)]*result.k[size_t(k)];
        }
    }
    result.outputDb=22.+20.*std::log10(std::max(1.e-22,result.k[1]));
    result.thd=100.*std::sqrt(totalThd)/
               std::max(1.e-22,result.k[1]);
    return result;
}
int main(int argc,char** argv){
    if(argc!=2){std::cerr<<"Usage: TubeCompOutputPadSurvey out.csv\n";return 2;}
    std::ofstream out(argv[1]);
    if(!out)return 3;
    out<<std::setprecision(9);
    out<<"mode,input_pct,input_dBu,output_pad_dB,"
          "measured_output_dBu,reference_dBu_difference,"
          "output_error_pct,cv_error_pct,thd_error_pct,"
          "worst_harmonic_error_pct\n";
    bool fail=false;
    for(bool rs:{false,true})
    for(int inputPct:{80,100})
    for(int level:{-18,-6,7}){
        const auto baseline=render(rs,inputPct,level,0.f);
        for(int i=0;i<=20;++i){
            const float outputDb=-.5f*i;
            const auto got=render(rs,inputPct,level,outputDb);
            const double expectedDelta=rs?outputDb:0.;
            const double measuredDelta=got.outputDb-baseline.outputDb;
            const double outputErrorPct=100.*std::fabs(
                      std::pow(10.,(measuredDelta-expectedDelta)/20.)-1.);
            const double cvErrorPct=100.*std::fabs(got.cv-baseline.cv)/
                                  std::max(.02f,baseline.cv);
            const double thdErrorPct=100.*std::fabs(got.thd-baseline.thd)/
                                   std::max(.02,baseline.thd);
            double worstHarmonicError=0.;
            for(int k=2;k<=20;++k){
                if(baseline.k[size_t(k)]/
                      std::max(1.e-22,baseline.k[1])<std::pow(10.,-85./20.))
                    continue;
                const double observed=got.k[size_t(k)]/
                                      std::max(1.e-22,got.k[1]);
                const double reference=baseline.k[size_t(k)]/
                                       std::max(1.e-22,baseline.k[1]);
                worstHarmonicError=std::max(worstHarmonicError,
                     100*std::fabs(observed/reference-1.));
            }
            out<<(rs?"RS124":"A-436B")<<","<<inputPct<<","<<level
               <<","<<outputDb<<","<<got.outputDb<<","
               <<expectedDelta<<","<<outputErrorPct<<","
               <<cvErrorPct<<","<<thdErrorPct<<","
               <<worstHarmonicError<<"\n";
            if(outputErrorPct>8.||cvErrorPct>8.||
               thdErrorPct>8.||worstHarmonicError>8.){
                std::cout<<"OUTPUT_PAD_FAIL "<<(rs?"RS124":"Altec")
                         <<" "<<inputPct<<"% "<<level<<"dBu "
                         <<outputDb<<"dB LEVEL_ERR="<<outputErrorPct
                         <<"% CV_ERR="<<cvErrorPct<<"%"
                         <<" THD_ERR="<<thdErrorPct<<"%"
                         <<" K_ERR="<<worstHarmonicError<<"%\n";
                fail=true;
            }
        }
    }
    if(fail){std::cerr<<"OUTPUT_PAD_GATES_FAILED\n";return 1;}
    std::cout<<"PASS: 252 output-pad scenarios, -10..0dB,"
             <<" phase/THD/harmonic ratios unchanged; output scales linearly\n";
    return 0;
}
