import json,sys
import numpy as np
from pathlib import Path
f=float(sys.argv[1]);rate=int(sys.argv[2]);w=np.genfromtxt('ripple.csv',delimiter=',',names=True)
if len(w)<rate*.49:raise RuntimeError('incomplete capture')
t=w['time']
def spec(x,hs):
    D=np.column_stack([np.ones(len(t))]+[g(2*np.pi*f*h*t) for h in hs for g in (np.cos,np.sin)])
    c=np.linalg.lstsq(D,x,rcond=None)[0]
    return np.hypot(c[1::2],c[2::2])
a=spec(w['audio'],list(range(1,21)));cv=spec(w['cv'],[1,2,3,4])
r={'audio_vpk':float(a[0]),'thd_pct':float(100*np.linalg.norm(a[1:])/a[0]),'harmonics_db':{str(i+1):float(20*np.log10(max(x/a[0],1e-15))) for i,x in enumerate(a)},'cv_mean_v':float(np.mean(w['cv'])),'cv_pp_v':float(np.ptp(w['cv'])),'cv_2f_vpk':float(cv[1]),'cv_f_vpk':float(cv[0])}
Path('ripple-result.json').write_text(json.dumps(r,indent=2));print(json.dumps(r,indent=2))
