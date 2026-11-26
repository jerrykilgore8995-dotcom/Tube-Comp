import sys,re,json,subprocess
from pathlib import Path
import numpy as np
from scipy.optimize import brentq
f,ratio,target,attack,release=map(float,sys.argv[1:6])
base=Path('powered-mismatch.cir').read_text()
def run(model,loss,on):
    s=base
    # Restrict parameter edits to top-level declarations after subcircuits.
    pos=s.index('.param AMP=')
    head,tail=s[:pos],s[pos:]
    for k,v in dict(AMP=2*np.sqrt(2)*.775*10**(.2),FREQ=f,RATIO=int(ratio),ATTACK=attack,RELEASE=release,LOSSDB=loss,LIMITING=int(on)).items():
        tail,n=re.subn(r'(?m)(^\.param[^\n]*?\b'+k+r'=)[^\s]+',lambda m:m.group(1)+str(v),tail,count=1)
        if n!=1:raise RuntimeError('parameter missing '+k)
    s=head+tail
    if model!='legacy':
        ka,kb,eps,ex={'GE':(.004147079,.000702,-.648792,.757482),'Philips':(.000403541,.002172,.185831,.458181)}[model]
        cell=f'.subckt UA176_6AL5 A K\nBDET A K I={{max(0,{ka}+{kb}*v(A,K))*pow(max(0,v(A,K)+({eps})),{ex})}}\nCAK A K 3.2p\n.ends UA176_6AL5'
        s,n=re.subn(r'(?ims)^\.subckt UA176_6AL5 A K\s*\n.*?^\.ends UA176_6AL5[^\n]*',lambda _:cell,s,count=1)
        if n!=1:raise RuntimeError('detector subckt not found')
    s=re.sub(r'(?m)^tran .*$', 'tran 20u 3 2.5 20u',s)
    Path('detector-model-run.cir').write_text(s)
    with open('spice.log','w') as log:subprocess.run(['ngspice','-b','detector-model-run.cir'],stdout=log,stderr=subprocess.STDOUT,check=True)
    z=np.loadtxt('powered.txt',skiprows=1)
    if z[-1,0]<2.999:raise RuntimeError('incomplete transient')
    t=np.arange(2.5,3,1/44100);x=np.interp(t,z[:,0],z[:,6]);cv=np.interp(t,z[:,0],z[:,5])
    D=np.column_stack([np.ones(len(t))]+[g(2*np.pi*f*h*t) for h in range(1,21) for g in (np.cos,np.sin)])
    c=np.linalg.lstsq(D,np.column_stack([x,cv]),rcond=None)[0];m=np.hypot(c[1::2],c[2::2]);a=m[:,0]
    return dict(audio_vpk=float(a[0]),thd_pct=float(100*np.linalg.norm(a[1:])/a[0]),cv_2f_vpk=float(m[1,1]),cv_pp=float(np.ptp(cv)),cv_mean=float(np.mean(cv)),harmonics_db={str(i+1):float(20*np.log10(max(v/a[0],1e-15))) for i,v in enumerate(a)})
report=[]
for model in ['legacy','GE','Philips']:
    cache={}
    def fun(loss):
        if loss not in cache:
            a=run(model,loss,True);b=run(model,loss,False);a.update(loss=loss,gr_db=20*np.log10(b['audio_vpk']/a['audio_vpk']),off_thd_pct=b['thd_pct']);cache[loss]=a
        return cache[loss]['gr_db']-target
    result=None
    grid=[0,5,10,15,20,25,30,40]
    for lo,hi in zip(grid,grid[1:]):
        if fun(lo)*fun(hi)<=0:
            x=brentq(fun,lo,hi,xtol=.05);fun(x);result=cache[x];break
    row=dict(model=model,f=f,ratio=ratio,target=target,attack=attack,release=release,result=result,points=list(cache.values()))
    report.append(row);print(json.dumps(row),flush=True)
    Path('detector-model-results.json').write_text(json.dumps(report,indent=2))
