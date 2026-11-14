#include "TubeCompDSP.h"
#include <iostream>
#include <iomanip>
#include <cmath>
using namespace tubecomp;
int main(){
 constexpr int fs=44100,N=4410; constexpr double tau=6.283185307179586;
 std::cout<<"mode,load,input_percent,source_dBu,output_dBu,GR_dB,ratio_previous,THD_percent";
 for(int k=2;k<=20;++k)std::cout<<",K"<<k<<"_dBc";std::cout<<"\n";
 for(bool rs:{false,true})for(float load:{200.f,600.f})for(int knob=1;knob<=10;++knob){
 double prev=0;bool first=true;
 for(int level=-40;level<=22;level+=2){
 Engine e; Parameters p;p.rs124=rs;p.input=float(knob);p.outputOhms=load;e.prepare(fs,256);e.setParameters(p,true);
 double re[21]={},im[21]={}; const double peak=std::pow(10.,(level-22.)/20.);
 for(int i=0;i<fs*8+N;++i){double angle=tau*1000*i/fs;float l=peak*std::sin(angle),r=l;e.process(l,r);if(i>=fs*8)for(int k=1;k<=20;++k){re[k]+=l*std::cos(k*angle);im[k]+=l*std::sin(k*angle);}}
 double fund=std::hypot(re[1],im[1]),out=22+20*std::log10(std::max(1e-15,2*fund/N)),power=0;
 for(int k=2;k<=20;++k)power+=re[k]*re[k]+im[k]*im[k];
 std::cout<<std::setprecision(9)<<(rs?"RS124":"436B")<<","<<load<<","<<knob*10<<","<<level<<","<<out<<","<<e.meters().gainReduction<<","<<(first?0:2/(out-prev))<<","<<100*std::sqrt(power)/fund;
 for(int k=2;k<=20;++k)std::cout<<","<<20*std::log10(std::max(1e-16,std::hypot(re[k],im[k])/fund));std::cout<<"\n";prev=out;first=false;
 }}}
