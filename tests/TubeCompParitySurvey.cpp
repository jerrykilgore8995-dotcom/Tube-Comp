// Independent native-DSP render of the same 1.6s sine, input source levels,
// and settings used by scripts/long_steady_compression.py. No result uses
// SPICE numbers to "measure" itself: actual 44.1kHz Engine output is Fourier
// analyzed. Full acceptance must also include other frequencies/controls.
#include "TubeCompDSP.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

constexpr double SR=44100.;
constexpr double TWOPI=6.2831853071795864769;
constexpr int FUND=1000;
struct Result {
 double outputDbu=0,thd=0,phaseDeg=0,grDb=0,cvV=0;
 std::array<double,21> dBc{};
};
Result measure(bool rs,int knob,int inputDb,bool load200=false) {
 tubecomp::Engine engine;
 tubecomp::Parameters p;
 p.rs124=rs;p.input=float(knob)*.1f;
 p.output=0.f;p.attack=0;p.recovery=10;p.stereoLink=1.f;p.wet=1.f;
 p.outputOhms=load200?200.f:600.f;
 engine.prepare(float(SR),256);
 engine.setParameters(p,true);
 // 0dBFS PEAK is +22dBu, +4dBu corresponds -18dBFS peak.
 const double peak=std::pow(10.,(double(inputDb)-22.)/20.);
 const int total=int(SR*1.6);
 const int window=int(SR*.1); // coherent 100x 1kHz cycles at 44.1k
 std::vector<double> y;
 y.reserve(size_t(window));
 for(int i=0;i<total;++i){
    const double t=double(i)/SR;
    const double fade=.5-.5*std::cos(
                         3.141592653589793*std::min(t/.04,1.));
    const float signal=float(peak*std::sin(TWOPI*FUND*t)*fade);
    float l=signal,r=signal;
    engine.process(l,r);
    if(i>=total-window)y.push_back(double(l));
 }
 Result out;
 std::array<double,21> k{};
 const auto n=static_cast<int>(y.size());
 double fundamentalRe=0,fundamentalIm=0;
 for(int h=1;h<=20;++h) {
    if(h*FUND>=SR*.5)continue;
    double re=0,im=0;
    for(int i=0;i<n;++i){
      const double angle=TWOPI*double(h)*FUND*(double(i)/SR);
      re+=y[size_t(i)]*std::cos(angle);
      im-=y[size_t(i)]*std::sin(angle);
    }
    if(h==1){fundamentalRe=re;fundamentalIm=im;}
    k[size_t(h)]=2.*std::hypot(re,im)/n;
 }
 double square=0.;
 for(int h=2;h<=20;++h){
   square+=k[size_t(h)]*k[size_t(h)];
   if(k[size_t(h)]>0 && k[1]>0)
      out.dBc[size_t(h)]=20*std::log10(
                           std::max(1.e-18,k[size_t(h)])/k[1]);
   else out.dBc[size_t(h)]=-300.;
 }
 out.outputDbu=22.+20.*std::log10(std::max(1.e-18,k[1]));
 out.thd=100.*std::sqrt(square)/std::max(1.e-18,k[1]);
 // Input sin reference (DFT phase -90deg). The circuit reverses polarity.
 // Wrapped phase is compared modulo 360deg by the parity script.
 out.phaseDeg=(std::atan2(fundamentalIm,fundamentalRe)+
                3.141592653589793*.5)*180./3.141592653589793;
 while(out.phaseDeg>180.)out.phaseDeg-=360.;
 while(out.phaseDeg< -180.)out.phaseDeg+=360.;
 out.cvV=engine.controlVolts();
 const float baseGainDb=tubecomp::spice::linearGainDb(rs,float(knob)*.1f);
 out.grDb=inputDb+baseGainDb-out.outputDbu;
 return out;
}
int main(int argc,char** argv) {
 bool fullInputGrid=false,load200=false;
 for(int i=2;i<argc;++i){
    const std::string arg=argv[i];
    if(arg=="--input-grid")fullInputGrid=true;
    else if(arg=="--load-200")load200=true;
    else {
      std::cerr<<"Unrecognized survey setting "<<arg<<"\n";return 2;
    }
 }
 if(argc<2 || argc>4){
    std::cerr<<"Use: TubeCompParitySurvey output.csv "
               "[--input-grid] [--load-200]\n";return 2;
 }
 std::ofstream out(argv[1]);
 if(!out){std::cerr<<"Cannot create "<<argv[1]<<"\n";return 2;}
 out<<std::setprecision(9);
 out<<"mode,input_pct,input_dBu,final_output_dBu,final_THD_percent,"
       "CV_magnitude_V,phase_deg,estimated_GR_dB";
 for(int h=2;h<=20;++h)out<<",K"<<h<<"_dBc";
 out<<"\n";
 const std::vector<int> knobs=fullInputGrid?
    std::vector<int>{0,10,20,30,40,50,60,70,80,90,100}:
    std::vector<int>{80,100};
 const std::vector<int> drive=fullInputGrid?
    std::vector<int>{-18,-6,7}:
    std::vector<int>{-24,-18,-12,-6,0,3,7,10,13};
 for(bool rs:{false,true})
  for(int knob:knobs)
   for(int inputDb:drive) {
    const auto p=measure(rs,knob,inputDb,load200);
    out<<(rs?"RS124":"A-436B")<<","<<knob<<","<<inputDb<<","
       <<p.outputDbu<<","<<p.thd<<","<<p.cvV<<","
       <<p.phaseDeg<<","<<p.grDb;
    for(int h=2;h<=20;++h)out<<","<<p.dBc[size_t(h)];
    out<<"\n";
    std::cout<<(rs?"RS124":"A-436B")<<" input "<<knob
       <<"% "<<inputDb<<"dBu  out="<<p.outputDbu
       <<"dBu  THD="<<p.thd<<"%  CV="<<p.cvV
       <<"V  phase="<<p.phaseDeg<<"deg\n";
 }
}
