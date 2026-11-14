"""Independent 436B-only mismatch experiment. Production DSP and original deck unchanged."""
from pathlib import Path
import sys, re, subprocess, csv
import numpy as np
from scipy.interpolate import CubicSpline
sys.path.insert(0,str(Path(__file__).resolve().parent))
from run_lab import prepare

def spectrum(t,y,hz):
    # Exactly 100 complete cycles, 256 samples per cycle, no duplicate endpoint.
    cycles=min(100,int(.1*hz))
    n=cycles*256
    x=t[-1]-cycles/hz+np.arange(n)/(256*hz)
    z=CubicSpline(t,y)(x)
    z=z-z.mean()
    a=np.abs(np.fft.rfft(z))*2/len(z)
    h=a[np.arange(1,21)*cycles]
    return 20*np.log10(np.maximum(h/h[0],1e-18)),100*np.linalg.norm(h[1:])/h[0],20*np.log10(h[0]/np.sqrt(2)/.775)

def run(level,knob,step,mismatch,hz=1000,caps=False):
    wd=Path("audit")/f"F{hz}_L{level}_I{knob}_T{step}_M{int(mismatch)}_C{int(caps)}"
    f=prepare(dict(mode="A-436B",hz=hz,dbu=level,attack=0,recovery=0,hold=False,input_knob=knob),wd)
    s=f.read_text()
    lib=wd/"models/tube_cells.lib"
    l=lib.read_text()
    if mismatch:
        for model,delta in [("V6BC8_REMOTE_EXP",.025),("V6CG7_GE",.05)]:
            block=re.search(r"(?im)^\.subckt "+model+r"\b[\s\S]*?^\.ends "+model+r"\b",l).group()
            for side,mult in [("A",1+delta),("B",1-delta)]:
                name=model+"_"+side
                replica=block.replace(model,name)
                replica,n=re.subn(r"(BPL P K I=)\{([^\n]+)\}",lambda m:m[1]+"{"+str(mult)+"*("+m[2]+")}",replica)
                assert n==1
                l+="\n"+replica+"\n"
            s=re.sub(r"(?m)^(XU[12]A .* )"+model+r"$",r"\g<1>"+model+"_A",s)
            s=re.sub(r"(?m)^(XU[12]B .* )"+model+r"$",r"\g<1>"+model+"_B",s)
        # Scale diode area: scales saturation current and junction capacitance,
        # inverse scales series resistance. Explicit physical proxy, not measured tube mismatch.
        s=s.replace("DDET1 detector sca D6AL5","DDET1 detector sca D6AL5 area=1.025").replace("DDET2 detector scb D6AL5","DDET2 detector scb D6AL5 area=0.975")
        assert all(x in s for x in ["V6BC8_REMOTE_EXP_A","V6BC8_REMOTE_EXP_B","V6CG7_GE_A","V6CG7_GE_B"])
        lib.write_text(l)
    if caps:
        for name,value in [("C2",22.88),("C3",21.12),("C5",20.90),("C6",22.88)]:
            s,n=re.subn(r"(?m)^("+name+r"\s+\S+\s+\S+\s+)22n\s*$",lambda m:m[1]+str(value)+"n",s)
            assert n==1, name
    s=s.replace("VIN source 0 SIN(0 {AMP} {FREQ}) AC 1","BINPUT source 0 V={AMP*sin(2*pi*FREQ*time)*(0.5-0.5*cos(pi*min(time/0.04,1)))}")
    s=re.sub(r"(?m)^tran .*",f"tran {step}u 3.2 3.0 {step}u",s)
    # Preserve the proven solver tolerances; convergence is checked across maxsteps.
    # Tightening all tolerances caused a startup timestep collapse at the transformer.
    s=s.replace("set wr_singlescale","set numdgt=16\nset wr_singlescale")
    s=re.sub(r"(?m)^wrdata tran.txt .*","wrdata tran.txt v(out) v(source) v(pa) v(pb) v(p2a) v(p2b) v(cv)",s)
    f.write_text(s)
    p=subprocess.run(["ngspice","-b","case.cir"],cwd=wd,capture_output=True,text=True,timeout=1200)
    (wd/"ngspice.log").write_text(p.stdout+p.stderr)
    if p.returncode or not (wd/"tran.txt").exists() or "simulation(s) aborted" in (p.stdout+p.stderr):
        raise RuntimeError(str(wd)+"\n"+(p.stdout+p.stderr)[-2500:])
    z=np.loadtxt(wd/"tran.txt",skiprows=1)
    assert z[-1,0]>3.19999 and np.all(np.diff(z[:,0])>0)
    rows=[]
    for name,y in [("source",z[:,2]),("6BC8_A",z[:,3]),("6BC8_B",z[:,4]),("6BC8_diff",z[:,3]-z[:,4]),("6CG7_A",z[:,5]),("6CG7_B",z[:,6]),("6CG7_diff",z[:,5]-z[:,6]),("output",z[:,1])]:
        h,thd,out=spectrum(z[:,0],y,hz)
        rows.append([level,knob,step,int(mismatch),name,thd,out,np.mean(z[:,7]),*h,hz,int(caps)])
    print(wd,rows[-1],flush=True)
    return rows

if __name__=="__main__":
    import argparse
    p=argparse.ArgumentParser();p.add_argument("--level",type=float,default=-18);p.add_argument("--knob",type=float,default=.8);p.add_argument("--step",type=float,default=1);p.add_argument("--mismatch",type=int,default=0);p.add_argument("--hz",type=int,default=1000);p.add_argument("--caps",type=int,default=0);a=p.parse_args()
    rows=run(a.level,a.knob,a.step,bool(a.mismatch),a.hz,bool(a.caps))
    with open("harmonics.csv","w") as f:
        w=csv.writer(f);w.writerow(["input_dBu","input_fraction","maxstep_us","mismatch","node","THD_percent","fundamental_dBu","CV_V",*[f"K{k}_dBc" for k in range(1,21)],"frequency_Hz","capacitor_mismatch"]);w.writerows(rows)
