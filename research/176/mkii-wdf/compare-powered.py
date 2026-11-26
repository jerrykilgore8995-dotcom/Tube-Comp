"""Offline parity audit; never substitute failed or missing measurements."""
import argparse, json
from pathlib import Path
import numpy as np

p=argparse.ArgumentParser()
p.add_argument('wave',type=Path)
p.add_argument('spice',type=Path)
p.add_argument('--frequency',type=float,default=1000)
p.add_argument('--output',type=Path,default=Path('comparison.json'))
a=p.parse_args()
w=np.genfromtxt(a.wave,delimiter=',',names=True)
s=np.loadtxt(a.spice,skiprows=1)
if len(w)<100 or len(s)<100: raise RuntimeError('missing settled data')
# SPICE wrdata order: time rect filter t3_ct reg cv audio
if s.shape[1]!=7: raise RuntimeError(f'unexpected SPICE columns {s.shape}')
t=w['time']; mask=(t>=1.105)&(t<1.195)
t=t[mask]
if len(t)<100 or t.min()<s[0,0] or t.max()>s[-1,0]: raise RuntimeError('no common settled time window')
def metrics(x):
    # Coherent least squares at fundamental, harmonics below Nyquist, and 120 Hz supply terms.
    dt=np.median(np.diff(t)); f=a.frequency
    hs=np.arange(1,min(20,int(.49/dt/f))+1)
    cols=[np.ones(len(t))]
    for h in hs:
        cols.extend([np.cos(2*np.pi*f*h*t),np.sin(2*np.pi*f*h*t)])
    co=np.linalg.lstsq(np.column_stack(cols),x,rcond=None)[0]
    mags=np.hypot(co[1::2],co[2::2])
    if mags[0]<=0: raise RuntimeError('zero fundamental')
    return {'fundamental_vpk':float(mags[0]),'phase_deg':float(np.degrees(np.arctan2(-co[2],co[1]))),
            'thd_percent_inband_k2_k20':float(100*np.linalg.norm(mags[1:])/mags[0]),
            'harmonics_normalized_db':{f'K{h}':float(20*np.log10(max(v/mags[0],1e-15))) for h,v in zip(hs,mags)}}
def dc(x):return {'mean':float(np.mean(x)),'peak_to_peak':float(np.ptp(x))}
wa=metrics(w['audio'][mask]); sa=metrics(np.interp(t,s[:,0],s[:,6]))
report={'wave':wa,'spice':sa,'gain_error_db':20*np.log10(wa['fundamental_vpk']/sa['fundamental_vpk']),
 'phase_error_deg':(wa['phase_deg']-sa['phase_deg']+180)%360-180,
 'rails':{k:{'wave':dc(w[k][mask]),'spice':dc(np.interp(t,s[:,0],s[:,i]))} for k,i in [('rect',1),('bplus',3),('reg',4),('cv',5)]}}
a.output.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
