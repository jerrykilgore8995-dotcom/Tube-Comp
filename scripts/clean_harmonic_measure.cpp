#include "TubeCompDSP.h"
#include <iostream>
#include <complex>
#include <iomanip>
using namespace tubecomp;
int main(){
 constexpr int fs=44100,N=8820;constexpr double tau=6.2831853071795864769;
 std::cout<<"mode,source_dBu,input_percent,hz,GR_dB,THD_percent";for(int k=2;k<=20;++k)std::cout<<",K"<<k<<"_dBc";std::cout<<"\n";
 for(int mode:{0,1})for(int level:{-18,0,18})for(int knob=1;knob<=10;++knob)for(int hz:{100,1000}){
 Engine e;e.prepare(fs,256);Parameters p;p.rs124=bool(mode);p.input=float(knob);e.setParameters(p,true);
 std::complex<double> h[21]={};double peak=std::pow(10.,(level-22.)/20);
 for(int i=0;i<fs*6+N;++i){float l=float(peak*std::sin(tau*hz*i/fs)),r=l;e.process(l,r);if(i>=fs*6)for(int k=1;k<=20;++k)h[k]+=double(l)*std::polar(1.,-tau*k*hz*i/fs);}
 double sum=0;for(int k=2;k<=20;++k)sum+=std::norm(h[k]);
 std::cout<<std::setprecision(10)<<mode<<","<<level<<","<<knob*10<<","<<hz<<","<<e.meters().gainReduction<<","<<100*sqrt(sum)/abs(h[1]);
 for(int k=2;k<=20;++k)std::cout<<","<<20*log10(std::max(1.e-16,abs(h[k])/abs(h[1])));std::cout<<"\n";
 }
}
