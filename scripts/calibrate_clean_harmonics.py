import json, pathlib, subprocess, math, csv
root=pathlib.Path('.')
levels=[-18,0,18]
refs={}
for p in pathlib.Path('paired-references').rglob('paired.json'):
    v=json.loads(p.read_text())
    if v['hz']!=1000 or v['output_load_ohm']!=600: continue
    mode=v.get('mode','A-436B')
    refs[mode,v['source_dBu'],v['input_percent']]=[10**(x['dBc']/20) for x in v['corrected']['harmonics'][1:]]
for mode in ['A-436B','RS124']:
    for level in (levels if mode=='RS124' else [-18]):
        for k in range(11):
            assert (mode,level,k*10) in refs, (mode,level,k)
# No arbitrary parity ratio or -90dB floor. Reference uses isolated detector
# and coherent analysis; tiny unresolved numerical residue below -140dBc is zeroed.
shapes=[]
for mode in ['A-436B','RS124']:
    m=[]
    for level in levels:
        a=[]
        for k in range(11):
            h=refs[mode,level if mode=='RS124' else -18,k*10]
            h=[x if x>=1e-7 else 0. for x in h]
            norm=math.sqrt(sum(x*x for x in h))
            assert norm>0, (mode,level,k)
            a.append([x/norm for x in h])
        m.append(a)
    shapes.append(m)
scales=[[[1. for k in range(11)] for l in levels] for m in range(2)]
def literal(a):
    if isinstance(a,list):return '{'+','.join(literal(x) for x in a)+'}'
    return f'{a:.10e}f'
def header():
    pathlib.Path('plugin/CleanHarmonicTargets.inc').write_text('''#pragma once
#include <array>
#include <cmath>
#include <algorithm>
namespace cleanharmonics {
static constexpr float shape[2][3][11][19]='''+literal(shapes)+''';
static constexpr float correction[2][3][11]='''+literal(scales)+''';
inline void apply(bool rs,float knob,float source,float balance,std::array<float,19>& v) noexcept {
    const int m=rs?1:0;
    const int i=std::clamp(int(knob),0,9);
    const float t=std::clamp(knob-i,0.f,1.f);
    const int j=source<0.f?0:1;
    const float u=std::clamp((source-(j==0?-18.f:0.f))/18.f,0.f,1.f);
    const float b=std::clamp(1.f+balance*(rs?1.3f:1.1f),.05f,3.5f);
    float power=0.f;
    for(int k=0;k<19;++k){float x=v[k]/((k%2==0)?b:1.f);power+=x*x;}
    const auto interp=[&](float a,float c,float d,float e){return (a+t*(c-a))*(1.f-u)+(d+t*(e-d))*u;};
    const float scale=interp(correction[m][j][i],correction[m][j][i+1],correction[m][j+1][i],correction[m][j+1][i+1]);
    float norm=0.f;
    for(int k=0;k<19;++k){
        v[k]=interp(shape[m][j][i][k],shape[m][j][i+1][k],shape[m][j+1][i][k],shape[m][j+1][i+1][k]);
        norm+=v[k]*v[k];
    }
    const float gain=std::sqrt(power/std::max(1e-30f,norm))*scale;
    for(int k=0;k<19;++k)v[k]*=gain*((k%2==0)?b:1.f);
}
}
''')
def run(stock=False):
    cmd=['g++','-O3','-std=c++17','-Iplugin','scripts/clean_harmonic_measure.cpp','-o','/tmp/cleanmeasure']
    if stock:cmd.insert(1,'-DTUBECOMP_STOCK_REFERENCE')
    subprocess.run(cmd,check=True)
    out=subprocess.check_output(['/tmp/cleanmeasure'],text=True)
    rows=list(csv.DictReader(out.splitlines()))
    return rows,out
def key(r):return tuple(int(r[x]) for x in ['mode','source_dBu','input_percent','hz'])
header()
base,raw=run(True)
pathlib.Path('stock-harmonics.csv').write_text(raw)
base={key(r):r for r in base}
for iteration in range(4):
    new,raw=run()
    errors=[]
    for r in new:
        m,l,k,hz=key(r)
        if hz!=1000 or k==0:continue
        target=float(base[key(r)]['THD_percent'])
        meas=float(r['THD_percent'])
        assert meas>0 and target>0
        err=abs(meas/target-1)
        errors.append(err)
        if iteration<3:scales[m][levels.index(l)][k//10]*=target/meas
    print('Calibration iteration',iteration,'max relative error',max(errors),flush=True)
    if iteration<3:header()
pathlib.Path('corrected-harmonics.csv').write_text(raw)
with open('calibration-comparison.csv','w') as f:
    w=csv.writer(f);w.writerow(['mode','source_dBu','input_percent','hz','stock_THD_percent','corrected_THD_percent','relative_error_percent','multiplier'])
    for r in new:
        m,l,k,hz=key(r);a=float(base[key(r)]['THD_percent']);b=float(r['THD_percent'])
        w.writerow([m,l,k,hz,a,b,100*(b/a-1) if a else '',scales[m][levels.index(l)][k//10]])
        if hz==1000 and k:print('RESULT',m,l,k,a,b,scales[m][levels.index(l)][k//10],flush=True)
# Calibration is output-measured; do not ship if the requested 1kHz
# stock-THD tolerance is missed at any measured dial point.
assert max(errors)<.05, max(errors)
