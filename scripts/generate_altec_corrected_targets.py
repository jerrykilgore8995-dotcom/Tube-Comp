import pathlib, json, math, sys
root=pathlib.Path(sys.argv[1])
rows={}
for p in root.rglob('paired.json'):
    v=json.loads(p.read_text())
    if v['source_dBu'] != -18 or v['output_load_ohm']!=600: continue
    rows[v['hz'],v['input_percent']]=v
freqs=[100,1000,5000,10000]
missing=[(f,p) for f in freqs for p in range(0,101,10) if (f,p) not in rows]
if missing: raise RuntimeError(f'Missing measured targets: {missing}')
lines=['#pragma once','// Generated from run 43572085333: complete corrected output spectra at -18 dBu.','namespace tubecomp {','static constexpr float altecHz[4]={100,1000,5000,10000};','static constexpr float altecTarget[4][11][19]={']
for f in freqs:
    lines.append('{')
    for p in range(0,101,10):
        h=rows[f,p]['corrected']['harmonics'][1:]
        assert [x['K'] for x in h]==list(range(2,21))
        lines.append('{'+','.join(f"{10**(x['dBc']/20):.10e}f" for x in h)+'},')
    lines.append('},')
lines.extend(['};', '''
inline float altecCorrectedRatio(float knob,int k,float hz,float source,float original,float fs) noexcept {
    if(hz*k>=.49f*fs)return 0.f;
    const int a=std::clamp(int(knob),0,9);
    const float t=std::clamp(knob-a,0.f,1.f);
    int b=0;while(b<2&&hz>altecHz[b+1])++b;
    const float u=std::clamp(std::log(std::max(hz,100.f)/altecHz[b])/std::log(altecHz[b+1]/altecHz[b]),0.f,1.f);
    const float x=altecTarget[b][a][k-2]+t*(altecTarget[b][a+1][k-2]-altecTarget[b][a][k-2]);
    const float y=altecTarget[b+1][a][k-2]+t*(altecTarget[b+1][a+1][k-2]-altecTarget[b+1][a][k-2]);
    // Only the -18dBu grid is validated for this preview. Keep the original reference away from that slice.
    const float w=std::clamp(1.f-std::fabs(source+18.f)/6.f,0.f,1.f);
    return original+w*(x+u*(y-x)-original);
}
}'''])
pathlib.Path('plugin/AltecCorrectedTargets.inc').write_text('\n'.join(lines)+'\n')
pathlib.Path('corrected-targets.json').write_text(json.dumps(list(rows.values()),indent=2))
print('Generated all 44 measured harmonic rows.')
