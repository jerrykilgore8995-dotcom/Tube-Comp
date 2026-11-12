#!/usr/bin/env python3
"""6AL5-controlled attack, release and HOLD time measurements using ngspice.

All values are exploratory; tested topologies and tube curves aren't yet
verified as genuine factory/EMI circuit wiring.
"""
import argparse,csv,json,math,re,shutil,subprocess
from pathlib import Path
import numpy as np
from run_lab import ROOT, BASE_DECK, LIB, values, parse_wrdata, dbu_to_peak

def make_deck(case,place):
    place.mkdir(parents=True,exist_ok=True)
    (place/"models").mkdir(exist_ok=True)
    shutil.copy2(LIB,place/"models/tube_cells.lib")
    d=values(case)
    d["FREQ"]=1000
    d["AMP"]=dbu_to_peak(case["dbu"])
    source=BASE_DECK.read_text()
    # 100ms silence, 350ms steady 1k, followed by 850ms recovery.
    source=source.replace("VIN source 0 SIN(0 {AMP} {FREQ}) AC 1",
        "VSTIM stim 0 SIN(0 {AMP} {FREQ}) AC 1\n"
        "VGATE gate 0 PULSE(0 1 0.100 10u 10u 0.350 2)\n"
        "BSTEP source 0 V={V(stim)*V(gate)}")
    for k,v in d.items():
        source=re.sub(r"(?<![A-Za-z0-9_])"+re.escape(k)+r"=[^\s]+",
                      f"{k}={v}",source,count=1)
    # Long release positions require multiple RC time constants to measure
    # 63% and 90% recovery, not just the initial 850ms of a short trace.
    duration=2.1 if case["hold"] else max(2.0,.45+2.8*d["REC_R"]*1e-6)
    source=source.replace("tran 20u 0.20 0 20u",
                          f"tran 50u {duration:.6f} 0 50u")
    (place/"case.cir").write_text(source)
    return source

def crossing(t,v,threshold,start,above=True):
    valid=np.flatnonzero((t>=start)&((v>=threshold) if above else (v<=threshold)))
    return float(t[valid[0]]) if len(valid) else float("nan")

def one(case,root):
    tag=(f"MODE{case['mode']}_ATT{case['attack']}_REC{case['recovery']}_"
         f"H{int(case['hold'])}_L{case['dbu']}").replace("-","m")
    wd=root/tag
    make_deck(case,wd)
    p=subprocess.run(["ngspice","-b","case.cir"],cwd=wd,text=True,
                     capture_output=True,timeout=160)
    log=p.stdout+"\n"+p.stderr
    (wd/"ngspice.log").write_text(log)
    result={**case,"provenance":"NGSPICE_EXPLORATORY"}
    if p.returncode or not (wd/"tran.txt").exists():
        return {**result,"status":"FAILED","reason":log[-1200:]}
    rows=parse_wrdata(wd/"tran.txt",9)
    t=rows[:,0];cv=-rows[:,3]
    t_start=.100;t_off=.450
    baseline=float(np.median(cv[t<.09])) if np.any(t<.09) else 0
    plateau=float(np.median(cv[(t>.39)&(t<.448)]))
    size=plateau-baseline
    if size<.0001:
        return {**result,"status":"NO_CONTROL_VOLTAGE","baseline":baseline,
                "plateau":plateau}
    t10=crossing(t,cv,baseline+.1*size,t_start)
    t63=crossing(t,cv,baseline+.632*size,t_start)
    t90=crossing(t,cv,baseline+.9*size,t_start)
    release_start=float(np.median(cv[(t>.450)&(t<.457)]))
    release_size=release_start-baseline
    r63=crossing(t,cv,baseline+.368*release_size,t_off,above=False)
    r90=crossing(t,cv,baseline+.1*release_size,t_off,above=False)
    final=float(np.median(cv[t>t[-1]-.2]))
    return {**result,"status":"OK","CV_baseline_V":baseline,
            "CV_plateau_V":plateau,"CV_end_V":final,
            "attack_10_ms":(t10-t_start)*1000,
            "attack_63_ms":(t63-t_start)*1000,
            "attack_90_ms":(t90-t_start)*1000,
            "recovery_63_ms":(r63-t_off)*1000,
            "recovery_90_ms":(r90-t_off)*1000,
            "hold_remaining_fraction":(final-baseline)/max(release_size,1e-10)}

def main():
    p=argparse.ArgumentParser();p.add_argument("--out",default="output/timing")
    p.add_argument("--broad",action="store_true");args=p.parse_args()
    root=Path(args.out);root.mkdir(parents=True,exist_ok=True)
    cases=[dict(mode="A-436B",hz=1000,dbu=6,attack=0,recovery=0,
                hold=False,input_knob=.8)]
    attacks=list(range(9)) if args.broad else [0,4,8]
    releases=list(range(6)) if args.broad else [0,2,5]
    for a in attacks:
        for r in releases:
            cases.append(dict(mode="RS124",hz=1000,dbu=6,attack=a,recovery=r,
                              hold=False,input_knob=.8))
    for a in [4]:
        cases.append(dict(mode="RS124",hz=1000,dbu=6,attack=a,recovery=2,
                          hold=True,input_knob=.8))
    results=[]
    for i,case in enumerate(cases,1):
        r=one(case,root/"raw")
        print(f"{i}/{len(cases)} {case['mode']} ATT={case['attack']} REC={case['recovery']} HOLD={case['hold']}: {r['status']}",flush=True)
        results.append(r)
    with (root/"timing.json").open("w") as f:json.dump(results,f,indent=2)
    keys=["mode","dbu","attack","recovery","hold","status","CV_baseline_V",
          "CV_plateau_V","CV_end_V","attack_10_ms","attack_63_ms","attack_90_ms",
          "recovery_63_ms","recovery_90_ms","hold_remaining_fraction"]
    with (root/"timing.csv").open("w",newline="") as f:
        w=csv.DictWriter(f,fieldnames=keys,extrasaction="ignore")
        w.writeheader();w.writerows(results)
    if any(x["status"]=="FAILED" for x in results):
        raise SystemExit("Some SPICE simulations did not complete")
if __name__=="__main__":main()
