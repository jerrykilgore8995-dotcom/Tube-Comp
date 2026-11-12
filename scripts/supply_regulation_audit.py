#!/usr/bin/env python3
"""Calibrate UNREGULATED B+ rise from the 436B annotated idle/compressed nodes.

A perfect 272 V battery cannot produce a 290 V post-R13 bus during compression.
This compares a simple Thevenin power supply (source voltage + finite series
resistance before user's 150uF/10k/47uF network). Unlike a fixed +272 V source,
the capacitors then affect real power-supply modulation and time constants.
Thevenin estimates are NOT a measurement of original transformer or rectifier.
"""
import csv, json, subprocess, re
from pathlib import Path
from run_lab import prepare, op_nodes

ROOT=Path("output/supply_regulation")
ROOT.mkdir(parents=True,exist_ok=True)
def run(open_v,series_ohm,cv,idx):
    case=dict(mode="A-436B",hz=1000,dbu=-36,attack=0,recovery=0,hold=False,input_knob=.8)
    wd=ROOT/f"sample{idx}_V{open_v}_R{series_ohm}_CV{str(cv).replace('-','m')}"
    f=prepare(case,wd)
    s=f.read_text()
    if series_ohm:
        old="VHT bplus 0 {BPLUS}"
        new=f"VHT htopen 0 {open_v}\nRSUPPLY htopen bplus {series_ohm}"
    else:
        old="VHT bplus 0 {BPLUS}"
        new=f"VHT bplus 0 {open_v}"
    assert old in s
    s=s.replace(old,new)
    if cv:
        ls=s.splitlines()
        n=0
        for k,line in enumerate(ls):
            if line.startswith("R12 detector cv ") or line.startswith("C4 cv 0 ") or line.startswith("R9 cv 0 "):
                ls[k]="* "+line;n+=1
        if n!=3:raise RuntimeError("Cannot isolate detector for forced control bias")
        s="\n".join(ls)+"\n"
        s=s.replace(".control\n",f"VFORCE cv 0 {cv}\n.control\n")
    a=s.index("\n.control\n")
    b=s.index("\n.endc",a)
    s=s[:a]+"""
.control
op
print v(bplus) v(platefeed) v(pa) v(pb) v(k1) v(outputfeed) v(p2a) v(p2b) v(k2) v(cv)
quit""" +s[b:]
    f.write_text(s)
    p=subprocess.run(["ngspice","-b",f.name],cwd=wd,timeout=15,
                     capture_output=True,text=True)
    log=p.stdout+"\n"+p.stderr
    if p.returncode:
        raise RuntimeError(log[-1500:])
    v=op_nodes(log)
    get=lambda node:v.get(f"v({node})",float("nan"))
    return dict(source_open_V=open_v,source_resistor_ohm=series_ohm,forced_cv_V=cv,
                source_loaded_V=get("bplus"),platefeed_6bc8_V=get("platefeed"),
                plate_6bc8_V=get("pa"),cathode_6bc8_V=get("k1"),
                output_feed_V=get("outputfeed"),plate_6cg7_V=get("p2a"),
                cathode_6cg7_V=get("k2"))

results=[]
counter=0
for open_v in (324,328,332,336,340):
    for source_r in (3000,3200,3400,3600,3800):
        pair=[]
        for cv in (0,-15):
            counter+=1
            row=run(open_v,source_r,cv,counter)
            results.append(row);pair.append(row)
        idle,compressed=pair
        cost=(idle["source_loaded_V"]-272)**2+(
              idle["platefeed_6bc8_V"]-210)**2+(
              idle["plate_6bc8_V"]-65)**2+(
              compressed["platefeed_6bc8_V"]-290)**2+(
              compressed["plate_6bc8_V"]-285)**2
        idle["pair_error_sq_V2"]=cost
        print(f"SUPPLY_CANDIDATE source={open_v}V R={source_r}ohm "
              f"IDLE B+={idle['source_loaded_V']:.2f}V "
              f"platefeed={idle['platefeed_6bc8_V']:.2f}V "
              f"6BC8plate={idle['plate_6bc8_V']:.2f}V "
              f"COMPRESSED CV=-15: platefeed={compressed['platefeed_6bc8_V']:.2f}V "
              f"6BC8plate={compressed['plate_6bc8_V']:.2f}V "
              f"cost={cost:.1f}",flush=True)
best=sorted((r for r in results if r["forced_cv_V"]==0),
            key=lambda r:r.get("pair_error_sq_V2",1e30))[:5]
print("SUPPLY_BEST",json.dumps(best,indent=2),flush=True)
with (ROOT/"all_thevenin_supply_points.csv").open("w",newline="") as fp:
    w=csv.DictWriter(fp,fieldnames=list(results[0].keys()),extrasaction="ignore")
    w.writeheader();w.writerows(results)
(ROOT/"best_supply_points.json").write_text(json.dumps(best,indent=2))
