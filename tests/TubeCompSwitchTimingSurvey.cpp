// Exact same 1kHz burst / 1ms envelope used by SPICE switch timing test.
// No configured milliseconds are passed off as measured attack/recovery.
// The reported CV comes from the *actual* DSP feedback-control state.
#include "TubeCompDSP.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

constexpr double sr=44100.;
constexpr double ON=.100,OFF=1.600;
constexpr double twopi=6.2831853071795864769;
struct Timing {
    std::string mode;
    int attack=0,recovery=0;
    bool hold=false;
    double baseline=0,plateau=0,offcv=0,endcv=0;
    double attack10=std::numeric_limits<double>::quiet_NaN();
    double attack63=std::numeric_limits<double>::quiet_NaN();
    double attack90=std::numeric_limits<double>::quiet_NaN();
    double recovery63=std::numeric_limits<double>::quiet_NaN();
    double recovery90=std::numeric_limits<double>::quiet_NaN();
    double holdFraction=0;
};
double avg(const std::vector<float>& v,double a,double b){
 const size_t start=static_cast<size_t>(a*sr);
 const size_t stop=std::min(v.size(),static_cast<size_t>(b*sr));
 if(start>=stop)return 0;
 double sum=0;for(size_t i=start;i<stop;++i)sum+=v[i];
 return sum/(stop-start);
}
double cross(const std::vector<float>& v,double start,
             double threshold,bool above){
 size_t from=static_cast<size_t>(start*sr);
 for(size_t i=from;i<v.size();++i)
     if((above?double(v[i])>=threshold:double(v[i])<=threshold))
         return 1000.*(i/sr-start);
 return std::numeric_limits<double>::quiet_NaN();
}
Timing run(bool rs,int attack,int recovery,bool hold){
 tubecomp::Parameters p;
 p.rs124=rs;p.input=8.f;p.output=0.f;p.attack=attack;
 p.altecAttack=rs?2:attack;
 p.recovery=hold?2*recovery+1:10-2*recovery;
 p.balance=0.f;p.superfuse=false;p.wet=1.f;p.stereoLink=1.f;
 p.outputOhms=600.f;
 tubecomp::Engine engine;engine.prepare(float(sr),256);
 engine.setParameters(p,true);
 const std::array<double,6> recms={180,330,680,1000,1800,3300};
 const double recoverySeconds=rs?recms[size_t(recovery)]*.001:1.;
 const double duration=hold?3.5:std::max(3.5,OFF+4*recoverySeconds);
 const int total=int(std::ceil(sr*duration));
 const double peak=std::pow(10.,(6.-22.)/20.);
 std::vector<float> cv;cv.reserve(size_t(total));
 for(int i=0;i<total;++i){
     const double t=double(i)/sr;
     const double rise=std::clamp((t-ON)/.001,0.,1.);
     const double fall=std::clamp((OFF-t)/.001,0.,1.);
     const double envelope=rise*fall;
     float left=float(peak*std::sin(twopi*1000*t)*envelope);
     float right=left;
     engine.process(left,right);
     cv.push_back(engine.controlVolts());
 }
 Timing m;
 m.mode=rs?"RS124":"A-436B";m.attack=attack;m.recovery=recovery;m.hold=hold;
 m.baseline=avg(cv,0,.09);m.plateau=avg(cv,OFF-.10,OFF-.015);
 const double rise=m.plateau-m.baseline;
 m.attack10=cross(cv,ON,m.baseline+rise*.10,true);
 m.attack63=cross(cv,ON,m.baseline+rise*.632,true);
 m.attack90=cross(cv,ON,m.baseline+rise*.90,true);
 m.offcv=avg(cv,OFF,OFF+.010);
 const double drop=m.offcv-m.baseline;
 m.recovery63=cross(cv,OFF,m.baseline+.368*drop,false);
 m.recovery90=cross(cv,OFF,m.baseline+.10*drop,false);
 m.endcv=avg(cv,duration-.10,duration);
 m.holdFraction=(m.endcv-m.baseline)/std::max(.01,drop);
 return m;
}
int main(int argc,char** argv){
 if(argc!=3)return 2;
 std::string family=argv[2];
 if(family!="attack" && family!="recovery" && family!="hold")return 3;
 std::ofstream out(argv[1]);
 if(!out)return 4;
 out<<std::setprecision(9)
    <<"mode,attack,recovery,hold,baseline_cv_V,plateau_cv_V,"
      "off_cv_V,final_cv_V,attack10_ms,attack63_ms,attack90_ms,"
      "recovery63_ms,recovery90_ms,hold_fraction\n";
 auto measure=[&](bool rs,int attack,int rec,bool hold){
    const Timing m=run(rs,attack,rec,hold);
    out<<m.mode<<","<<m.attack<<","<<m.recovery<<","<<(m.hold?1:0)
       <<","<<m.baseline<<","<<m.plateau<<","<<m.offcv<<","<<m.endcv
       <<","<<m.attack10<<","<<m.attack63<<","<<m.attack90
       <<","<<m.recovery63<<","<<m.recovery90<<","<<m.holdFraction<<"\n";
    std::cout<<"REAL_DSP_SWITCH "<<m.mode<<" attack="<<m.attack
         <<" rec="<<m.recovery<<" HOLD="<<m.hold
         <<" GRcv="<<m.plateau<<"V"
         <<" A63="<<m.attack63<<"ms R63="<<m.recovery63
         <<"ms hold="<<m.holdFraction<<"\n";
 };
 if(family=="attack"){
     for(int a=0;a<3;++a)measure(false,a,3,false);
     for(int a=0;a<9;++a)measure(true,a,3,false);
 }else if(family=="recovery"){
     for(int r=0;r<6;++r)measure(true,4,r,false);
 }else {
     for(int r=0;r<5;++r)measure(true,4,r,true);
 }
}
