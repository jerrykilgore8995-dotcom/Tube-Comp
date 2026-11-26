"""Detector-only SPICE investigation: original/corrected wiring, true/frozen CV, identical linear magnetics."""
import numpy as np, json, subprocess, re, sys
from pathlib import Path
from scipy.optimize import brentq
f,ratio,attack,release=map(float,sys.argv[1:5])
deck=Path('powered-mismatch.cir').read_text()
def run(db,loss,mode,frozen=None):
    amp=2*np.sqrt(2)*.775*10**(db/20)
    s=deck
    for k,v in dict(AMP=amp,FREQ=f,RATIO=int(ratio),ATTACK=attack,RELEASE=release,LOSSDB=loss,LIMITING=int(mode!='off')).items():
        s=re.sub(r'(?m)(^\.param[^\n]*?\b'+k+r'=)[^\s]+',lambda m:m.group(1)+str(v),s,count=1)
    if mode=='old':
        s=re.sub(r'^R32 ATK_IN ATK_WIPER (.*)$',r'R32 ATK_IN DET_A \1',s,flags=re.M)
        s=re.sub(r'^R33A .*\nR33B ATK_WIPER DET_A (.*)$',r'R33 ATK_IN DET_A \1',s,flags=re.M)
    if mode=='frozen':
        s=re.sub(r'^R_S4_ON .*$', 'R_S4_ON CV ATK_IN 1T',s,flags=re.M)
        s=re.sub(r'^R_S4_OFF .*$', 'R_S4_OFF CV 0 1T\nVFIX CV 0 '+str(frozen),s,flags=re.M)
    s=re.sub(r'^tran .*$', 'tran 20u 3 2.5 20u',s,flags=re.M)
    Path('ripple-run.cir').write_text(s)
    with open('spice-'+mode+'.log','w') as log:subprocess.run(['ngspice','-b','ripple-run.cir'],stdout=log,stderr=subprocess.STDOUT,check=True)
    z=np.loadtxt('powered.txt',skiprows=1)
    if z[-1,0]<2.999:raise RuntimeError('incomplete SPICE')
    t=np.arange(2.5,3,1/44100);x=np.interp(t,z[:,0],z[:,6]);cv=np.interp(t,z[:,0],z[:,5])
    def fit(y,hs):
        D=np.column_stack([np.ones(len(t))]+[g(2*np.pi*f*h*t) for h in hs for g in (np.cos,np.sin)])
        c=np.linalg.lstsq(D,y,rcond=None)[0];return np.hypot(c[1::2],c[2::2])
    a=fit(x,range(1,21));c=fit(cv,[1,2,3,4])
    return dict(db=db,loss=loss,mode=mode,audio_vpk=float(a[0]),thd_pct=float(100*np.linalg.norm(a[1:])/a[0]),harmonics_db={str(i+1):float(20*np.log10(max(v/a[0],1e-15))) for i,v in enumerate(a)},cv_mean=float(np.mean(cv)),cv_pp=float(np.ptp(cv)),cv_2f_vpk=float(c[1]))
out=[]
for loss in [0,5,10,15,20,25,30,35,40]:
    off=run(4,loss,'off')
    for mode in ['old','corrected']:
        a=run(4,loss,mode);a['gr_db']=20*np.log10(off['audio_vpk']/a['audio_vpk'])
        frozen=run(4,loss,'frozen',a['cv_mean'])
        # Small-signal static gain sensitivity at the actual mean bias, dB/V.
        lo=run(4,loss,'frozen',a['cv_mean']-.025);hi=run(4,loss,'frozen',a['cv_mean']+.025)
        a['static_gain_sensitivity_db_per_v']=20*np.log10(hi['audio_vpk']/lo['audio_vpk'])/.05
        a['frozen_cv_thd_pct']=frozen['thd_pct'];a['frozen_cv_harmonics_db']=frozen['harmonics_db'];a['off_thd_pct']=off['thd_pct']
        out.append(a);print(json.dumps(a),flush=True)
Path('detector-spice-results.json').write_text(json.dumps(out,indent=2))
