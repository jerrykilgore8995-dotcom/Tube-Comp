import json,sys,re,subprocess
from pathlib import Path
import numpy as np
from scipy.optimize import brentq
f,ratio,target,turns,L,taps= sys.argv[1:]
f=float(f);ratio=int(ratio);target=float(target);turns=float(turns);L=float(L)
base=Path('powered-mismatch.cir').read_text()
if taps=='factory':
    base=base.replace('RATIO==2,.12','RATIO==2,.13').replace('RATIO==4,.29','RATIO==4,.32').replace('RATIO==8,.58','RATIO==8,.68')
base=re.sub(r'^LPA HALF_A CT .*$',f'LPA HALF_A CT {L/4}',base,flags=re.M)
base=re.sub(r'^LPB CT HALF_B .*$',f'LPB CT HALF_B {L/4}',base,flags=re.M)
base=re.sub(r'^LS OUT_RAW OUTN .*$',f'LS OUT_RAW OUTN {L/turns**2}',base,flags=re.M)
def run(loss,on):
    deck=base
    for k,v in dict(AMP=2*np.sqrt(2)*.775*10**(.2),FREQ=f,RATIO=ratio,LOSSDB=loss,LIMITING=int(on)).items():
        deck=re.sub(r'(?<=\b'+k+r'=)[^\s]+',str(v),deck,count=1)
    deck=re.sub(r'^tran .*$', 'tran 20u 3 2.5 20u',deck,flags=re.M)
    deck=deck.replace('v(cv) audio','v(cv) audio v(det_a) v(det_k1) v(det_k2) v(tap_a,t3_ct) v(tap_b,t3_ct)')
    Path('xfmr-run.cir').write_text(deck)
    with open('xfmr.log','w') as log: subprocess.run(['ngspice','-b','xfmr-run.cir'],stdout=log,stderr=subprocess.STDOUT,check=True)
    z=np.loadtxt('powered.txt',skiprows=1)
    if z[-1,0]<2.999:raise RuntimeError('incomplete simulation')
    t=np.arange(2.5,3,1/44100); y=np.column_stack([np.interp(t,z[:,0],z[:,j]) for j in range(1,z.shape[1])])
    D=np.column_stack([np.ones(len(t))]+[g(2*np.pi*f*h*t) for h in range(1,21) for g in (np.cos,np.sin)])
    c=np.linalg.lstsq(D,y,rcond=None)[0];m=np.hypot(c[1::2],c[2::2]);a=m[:,5]
    return dict(audio_vpk=float(a[0]),thd_pct=float(100*np.linalg.norm(a[1:])/a[0]),harmonics_db={str(i+1):float(20*np.log10(max(v/a[0],1e-15))) for i,v in enumerate(a)},cv_mean=float(np.mean(y[:,4])),cv_pp=float(np.ptp(y[:,4])),cv_2f_vpk=float(m[1,4]),tap_a_vpk=float(m[0,9]),tap_b_vpk=float(m[0,10]))
cache={}
def measure(loss):
    if loss not in cache:
        off=run(loss,False);on=run(loss,True)
        on['gr_db']=20*np.log10(off['audio_vpk']/on['audio_vpk']);on['loss_db']=loss;on['off_thd_pct']=off['thd_pct'];cache[loss]=on
        print(json.dumps(on),flush=True)
    return cache[loss]['gr_db']-target
grid=[0,5,10,15,20,30,40];result=None
for a,b in zip(grid,grid[1:]):
    if measure(a)*measure(b)<=0:
        x=brentq(measure,a,b,xtol=.03);measure(x);result=cache[x];break
report=dict(freq=f,ratio=ratio,target_gr=target,turns=turns,L=L,taps=taps,result=result,measured=list(cache.values()))
Path('xfmr-result.json').write_text(json.dumps(report,indent=2));print(json.dumps(report),flush=True)
