#include "TubeCompDSP.h"
#include <iostream>
#include <iomanip>
#include <complex>
using namespace tubecomp;
int main(){
 constexpr int fs=44100,N=8820; constexpr double tau=6.2831853071795864769;
 std::cout<<"input_percent,source_dBu,THD_percent,GR_dB";
 for(int k=2;k<=20;++k)std::cout<<",K"<<k<<"_dBc";std::cout<<"\n";
 for(int p=1;p<=10;++p){
 Engine e; e.prepare(fs,256);Parameters q;q.rs124=false;q.input=float(p);e.setParameters(q,true);
 std::complex<double> h[21]={};for(int i=0;i<fs*5+N;++i){
 float l=float(.01*std::sin(tau*1000*i/fs)),r=l;e.process(l,r);
 if(i>=fs*5)for(int k=1;k<=20;++k)h[k]+=double(l)*std::polar(1.,-tau*k*1000*i/fs);
 }
 double sum=0;for(int k=2;k<=20;++k)sum+=std::norm(h[k]);
 std::cout<<p*10<<",-18,"<<std::setprecision(9)<<100*std::sqrt(sum)/std::abs(h[1])<<","<<e.meters().gainReduction;
 for(int k=2;k<=20;++k)std::cout<<","<<20*std::log10(std::max(1.e-16,std::abs(h[k])/std::abs(h[1])));std::cout<<"\n";
 }}
