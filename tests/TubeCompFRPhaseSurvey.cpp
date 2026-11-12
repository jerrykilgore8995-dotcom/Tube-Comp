// Real 44.1kHz VST3 DSP .AC-style sweep at forced negative grid CV.
// Matches scripts/frequency_phase_acceptance.py's separate SPICE bias points.
// The 1k reference amplitude is intentionally removed from FR; this test
// measures frequency-dependent gain and PHASE, not compressor GR/timing.
#include "TubeCompDSP.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

constexpr double SR=44100.0;
constexpr double PI=3.14159265358979323846;
struct Point {double hz=0,gainDb=0,phaseDeg=0;};
Point measure(bool rs,int controlV,int hz) {
 tubecomp::Parameters p;p.rs124=rs;p.input=8.f;
 p.attack=0;p.recovery=10;p.output=0;p.wet=1;
 tubecomp::Engine engine;engine.prepare(float(SR),256);
 engine.setParameters(p,true);
 engine.setCalibrationControlVolts(float(-controlV));
 const double peak=std::pow(10.,(-52.)/20.); // -30 dBu normalized input
 const int warm=int(SR*.8);
 const int samples=int(SR);
 std::vector<float> render;render.reserve(size_t(samples));
 for(int j=0;j<warm+samples;++j){
  const double t=double(j)/SR;
  float x=float(peak*std::sin(2*PI*double(hz)*t));
  float l=x,r=x;engine.process(l,r);
  if(j>=warm)render.push_back(l);
 }
 double real=0,imag=0;
 for(int i=0;i<samples;++i){
  const double ang=2*PI*double(hz)*double(i)/SR;
  real+=double(render[size_t(i)])*std::cos(ang);
  imag-=double(render[size_t(i)])*std::sin(ang);
 }
 const double fundamental=2*std::hypot(real,imag)/samples;
 const double outGain=20*std::log10(std::max(fundamental,1.e-18)/peak);
 double phase=(std::atan2(imag,real)+PI*.5)*180/PI;
 while(phase>180)phase-=360;
 while(phase<-180)phase+=360;
 return {double(hz),outGain,phase};
}
int main(int argc,char** argv){
 if(argc!=2)return 2;
 std::ofstream o(argv[1]);
 if(!o)return 2;
 o<<std::setprecision(10);
 o<<"mode,cv_V,frequency_hz,relative_gain_dB,phase_deg,one_kHz_gain_dB\n";
 const std::vector<int> frequencies={
     20,30,40,50,100,200,500,975,1000,1025,
     2000,4975,5000,5025,10000,15000,20000};
 for(bool rs:{false,true}) for(int cv:{0,-6,-12,-15,-17}){
  const auto base=measure(rs,cv,1000);
  for(auto hz:frequencies){
   const auto d=hz==1000?base:measure(rs,cv,hz);
   o<<(rs?"RS124":"A-436B")<<","<<cv<<","<<hz<<","
      <<d.gainDb-base.gainDb<<","<<d.phaseDeg<<","<<base.gainDb<<"\n";
   std::cout<<"FR_PHASE "<<(rs?"RS124":"A-436B")
            <<" CV="<<cv<<"Hz="<<hz
            <<" response="<<(d.gainDb-base.gainDb)
            <<" dB phase="<<d.phaseDeg<<" deg\n";
  }
 }
}
