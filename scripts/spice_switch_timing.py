#!/usr/bin/env python3
"""Measure actual 6AL5 SPICE attack/recovery/HOLD on a long 1kHz burst.

1kHz +6dBu source, dual 50k pot at 80%, 0.1 second initial silence,
1.5sec signal, then recovery over up to 3.5 RC time constants. A 1ms
source ramp prevents unphysical instantaneous transients.

The test measures 63% of actual CV plateau after 1.5s, not nominal R*C.
RS124 attack/recovery values remain documented *reconstruction candidates*.
"""
from pathlib import Path
import csv,os,json,re,subprocess
import numpy as np
from run_lab import prepare,parse_wrdata

FAMILY=os.getenv("TUBECOMP_TIMING_FAMILY","attack")
if FAMILY not in ("attack","recovery","hold"):raise ValueError("Bad family")
ROOT=Path("output/actual_spice_timing")/FAMILY
ROOT.mkdir(parents=True,exist_ok=True)
ON=.1
OFF=1.6
def crossing(t,y,threshold,start,above):
    m=(t>=start)&((y>=threshold) if above else (y<=threshold))
    hits=np.flatnonzero(m)
    return float(t[hits[0]]-start) if len(hits) else float("nan")
def simulate(mode,attack,recovery,hold):
    case=dict(mode=mode,hz=1000,dbu=6,attack=attack,recovery=recovery,
              hold=hold,input_knob=.8)
    tag=f"{mode.replace('-','')}_A{attack}_R{recovery}_H{int(hold)}"
    wd=ROOT/tag
    deck=prepare(case,wd)
    s=deck.read_text()
    old="VIN source 0 SIN(0 {AMP} {FREQ}) AC 1"
    new=(
       "BINPUT source 0 V={AMP*sin(2*pi*FREQ*time)*"
       "max(0,min(1,(time-0.100)/0.001))*"
       "max(0,min(1,(1.600-time)/0.001))}"
    )
    if old not in s:raise RuntimeError("Missing SPICE stimulus")
    s=s.replace(old,new)
    rec_ms=[180,330,680,1000,1800,3300][recovery] if mode=="RS124" else 1000
    duration=max(3.5,OFF+4.*rec_ms/1000.)
    if hold:duration=3.5
    s,n=re.subn(r"tran [^\n]+(?=\nwrdata tran.txt)",
               f"tran 30u {duration:.6f} 0 30u",s,count=1)
    if n!=1:raise RuntimeError("Missing SPICE transient")
    deck.write_text(s)
    try:
        p=subprocess.run(["ngspice","-b",deck.name],cwd=wd,
                  capture_output=True,text=True,timeout=210)
    except subprocess.TimeoutExpired:
        return dict(mode=mode,attack=attack,recovery=recovery,hold=hold,
                    status="TIMEOUT")
    log=p.stdout+"\n"+p.stderr
    if p.returncode!=0 or not (wd/"tran.txt").is_file():
        return dict(mode=mode,attack=attack,recovery=recovery,hold=hold,
                    status="FAILED",error=log[-500:])
    w=parse_wrdata(wd/"tran.txt",9)
    t=w[:,0];cv=-w[:,3]
    baseline=float(np.median(cv[t<.09]))
    plateau=float(np.median(cv[(t>OFF-.10)&(t<OFF-.015)]))
    maxDelta=plateau-baseline
    if maxDelta <.10:
        return dict(mode=mode,attack=attack,recovery=recovery,hold=hold,
             status="NO_GAIN_REDUCTION",baseline=baseline,plateau=plateau)
    # 0.1sec–1.6sec has sufficient time for every 12..330ms attack candidate.
    attack10=crossing(t,cv,baseline+maxDelta*.10,ON,True)
    attack63=crossing(t,cv,baseline+maxDelta*.632,ON,True)
    attack90=crossing(t,cv,baseline+maxDelta*.90,ON,True)
    offMean=float(np.median(cv[(t>OFF)&(t<OFF+.010)]))
    fall=offMean-baseline
    recovery63=crossing(t,cv,baseline+.368*fall,OFF,False)
    recovery90=crossing(t,cv,baseline+.1*fall,OFF,False)
    end=float(np.median(cv[t>t[-1]-.10]))
    return dict(mode=mode,attack=attack,recovery=recovery,hold=hold,
       status="OK",baseline_cv_V=baseline,plateau_cv_V=plateau,
       off_cv_V=offMean,final_cv_V=end,
       attack10_ms=1000*attack10,attack63_ms=1000*attack63,
       attack90_ms=1000*attack90,recovery63_ms=1000*recovery63,
       recovery90_ms=1000*recovery90,
       hold_fraction=(end-baseline)/max(.01,fall))
cases=[]
if FAMILY=="attack":
    cases.append(("A-436B",0,3,False))
    cases.extend(("RS124",a,3,False) for a in range(9))
elif FAMILY=="recovery":
    cases.extend(("RS124",4,r,False) for r in range(6))
elif FAMILY=="hold":
    cases.extend(("RS124",4,r,True) for r in range(5))
out=[]
for m,a,r,h in cases:
    row=simulate(m,a,r,h)
    out.append(row)
    print("SPICE_SWITCH_TIMING",row,flush=True)
if len(out)!=len(cases) or any(r["status"]!="OK" for r in out):
    raise RuntimeError("One or more SPICE switch-timing cases failed")
with (ROOT/"timing.csv").open("w",newline="") as fp:
    w=csv.DictWriter(fp,fieldnames=list(out[0]))
    w.writeheader();w.writerows(out)
(ROOT/"timing.json").write_text(json.dumps(out,indent=2))
