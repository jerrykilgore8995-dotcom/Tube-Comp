import sys,json,re,subprocess
from pathlib import Path
import numpy as np
from scipy import signal
mode,rate,ratio,freq,db,attack,release=sys.argv[1:]
rate,ratio,freq,db,attack,release=int(rate),int(ratio),float(freq),float(db),float(attack),float(release)
amp=2*np.sqrt(2)*.775*10**(db/20)
deck=Path('powered-mismatch.cir').read_text()
for key,value in {'AMP':amp,'FREQ':freq,'RATIO':ratio,'LIMITING':int(mode!='off'),'ATTACK':attack,'RELEASE':release}.items():
    deck=re.sub(r'(?<=\b'+key+r'=)[^\s]+',str(value),deck,count=1)
if mode=='burst':
    deck=re.sub(r'^VDRIVE DRIVEP.*$',f'BDRIVE DRIVEP DRIVEN V={{ {amp}*sin(2*pi*{freq}*time)*ternary_fcn(time<1,.001,ternary_fcn(time<1.5,1,.001)) }}',deck,flags=re.M)
    duration,start=3,.8
else: duration,start=1.4,1.1
deck=re.sub(r'^tran .*$',f'tran 2u {duration} {start} 2u',deck,flags=re.M)
Path('run.cir').write_text(deck)
with open('spice.log','w') as log: subprocess.run(['ngspice','-b','run.cir'],stdout=log,stderr=subprocess.STDOUT,check=True)
s=np.loadtxt('powered.txt',skiprows=1)
w=np.genfromtxt('wave.csv',delimiter=',',names=True)
if len(s)<100 or len(w)<100 or s[-1,0]<duration-.001 or w['time'][-1]<duration-.001: raise RuntimeError('incomplete simulation')
t=w['time']; a=w['audio']; b=np.interp(t,s[:,0],s[:,6])
def fit(t,x,h):
    D=np.column_stack([np.ones(len(t))]+[g(2*np.pi*freq*k*t) for k in h for g in (np.cos,np.sin)])
    c=np.linalg.lstsq(D,x,rcond=None)[0]; return c,np.hypot(c[1::2],c[2::2])
def metrics(t,x,fs):
    h=list(range(1,min(20,int(.49*fs/freq))+1)); c,m=fit(t,x,h)
    return dict(vpk=float(m[0]),gain_db=float(20*np.log10(m[0]/amp)),phase_deg=float(np.degrees(np.arctan2(c[1],c[2]))),thd_pct=float(100*np.linalg.norm(m[1:])/m[0]),harmonics_db={str(k):float(20*np.log10(max(v/m[0],1e-15))) for k,v in zip(h,m)})
def timing(t,x):
    # One-cycle sliding synchronous amplitude, referenced to input amplitude.
    n=round(rate/freq); osc=np.exp(-2j*np.pi*freq*t)
    env=2*np.abs(signal.fftconvolve(x*osc,np.ones(n)/n,mode='same'))
    inp=np.where((t>=1)&(t<1.5),amp,amp*.001)
    gain=20*np.log10(np.maximum(env,1e-15)/inp)
    base=np.median(gain[(t>.85)&(t<.95)]); end=np.median(gain[(t>1.4)&(t<1.49)])
    gr=base-gain; target=base-end
    def first(mask,cond,origin):
        q=t[mask&cond]; return float((q[0]-origin)*1000) if len(q) else None
    return dict(gr_db=float(target),attack_63_ms=first((t>=1+n/rate/2)&(t<1.5),gr>=.632*target,1),release_63_ms=first(t>=1.5+n/rate/2,gr<=.368*target,1.5),recovered_gr_db=float(np.median(gr[t>2.9])),envelope_resolution_ms=1000*n/rate)
report=dict(mode=mode,rate=rate,ratio=ratio,freq=freq,dbu=db,attack=attack,release=release)
if mode=='burst':
    report.update(wave=timing(t,a),spice=timing(t,b))
else:
    mask=(t>=1.15)&(t<1.35)
    report.update(wave=metrics(t[mask],a[mask],rate),spice=metrics(t[mask],b[mask],rate))
    report['gain_error_db']=report['wave']['gain_db']-report['spice']['gain_db']
    report['phase_error_deg']=(report['wave']['phase_deg']-report['spice']['phase_deg']+180)%360-180
    # Host-rate decimation with explicit anti-alias FIR; compare coherent folded
    # harmonic bins not coincident with actual harmonics or 120 Hz supply sidebands.
    factor=rate//44100
    y=a if factor==1 else signal.resample_poly(a,1,factor,window=('kaiser',12.0))
    tt=t[0]+np.arange(len(y))/44100
    keep=(tt>=1.15)&(tt<1.35); tt=tt[keep]; y=y[keep]
    actual={k*freq for k in range(1,int(22050//freq)+1)}
    folds=set()
    for k in range(2,81):
        hz=abs((k*freq+22050)%44100-22050)
        if k*freq>22050 and 20<=hz<=20000 and all(abs(hz-z)>10 for z in actual) and abs(hz/120-round(hz/120))>0.05: folds.add(hz)
    if folds:
        z=np.array(sorted(folds)); D=np.column_stack([np.ones(len(tt))]+[g(2*np.pi*h*tt) for h in sorted(actual|folds) for g in (np.cos,np.sin)])
        c=np.linalg.lstsq(D,y,rcond=None)[0]; mags=np.hypot(c[1::2],c[2::2]); mm=dict(zip(sorted(actual|folds),mags))
        report['fold_bins_db_relative']={str(h):float(20*np.log10(max(mm[h]/mm[freq],1e-15))) for h in z}
        report['worst_fold_bin_db_relative']=max(report['fold_bins_db_relative'].values())
    report['host_wave']=metrics(tt,y,44100)
report['runtime']=json.loads(Path('wave-meta.json').read_text())
Path('extended-result.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
