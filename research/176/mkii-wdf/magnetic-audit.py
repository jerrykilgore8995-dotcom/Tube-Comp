import json
from pathlib import Path
import numpy as np
from scipy import signal
m=json.loads(Path('magnetic-meta.json').read_text())
w=np.genfromtxt('magnetic-wave.csv',delimiter=',',names=True,deletechars='')
if m['error'] or len(w)<100: raise RuntimeError('incomplete run')
t=w['time'];keep=(t>=1.15)&(t<1.35);t=t[keep];f=m['freq'];rate=m['rate']
def spec(x,t,fs):
    hs=np.arange(1,min(20,int(.49*fs/f))+1)
    # Include supply lines which do not coincide with the tone harmonics.
    freqs=list(hs*f)
    nuisance=[120*j for j in range(1,9) if all(abs(120*j-z)>1e-6 for z in freqs)]
    design=np.column_stack([np.ones(len(t))]+[g(2*np.pi*z*t) for z in freqs+nuisance for g in (np.cos,np.sin)])
    co=np.linalg.lstsq(design,x,rcond=None)[0]
    mags=np.hypot(co[1:1+2*len(hs):2],co[2:1+2*len(hs):2])
    if mags[0]<1e-15: return {'vpk_or_apk':float(mags[0]),'harmonics_db':None}
    return {'vpk_or_apk':float(mags[0]),'phase_deg':float(np.degrees(np.arctan2(co[1],co[2]))),'thd_pct':float(100*np.linalg.norm(mags[1:])/mags[0]),'harmonics_db':{str(k):float(20*np.log10(max(a/mags[0],1e-15))) for k,a in zip(hs,mags)}}
m['stage_spectra']={k:spec(w[k][keep],t,rate) for k in w.dtype.names if ':' in k or k=='audio'}
for n,pri,sec in [('T1','T1:Lmag','T1:Csec'),('T2','T2:Lmag','T2:Csec'),('T3','T3:RC','T3:CS')]:
    p=m['stage_spectra'][pri];s=m['stage_spectra'][sec]
    m.setdefault('winding_transfer',{})[n]={'db':float(20*np.log10(s['vpk_or_apk']/p['vpk_or_apk'])),'phase_deg':(s['phase_deg']-p['phase_deg']+180)%360-180}
factor=rate//44100
if factor>1:
    y=signal.resample_poly(w['audio'],1,factor,window=('kaiser',12))
    tt=w['time'][0]+np.arange(len(y))/44100; k=(tt>=1.15)&(tt<1.35)
    m['host_audio']=spec(y[k],tt[k],44100)
Path('magnetic-result.json').write_text(json.dumps(m,indent=2));print(json.dumps(m,indent=2))
