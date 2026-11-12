#!/usr/bin/env python3
"""Compare fixed 272V B+ with fitted 340V+3.9k unregulated source.

At DC, the modeled 150uF reservoir is AFTER the source resistor and before
user's 10k / 47uF filter; no source resistance has been added in the audio
path. This is a proposed unregulated power-supply variant to test against
Altec annotated 272V (idle) and 290V (compressed), not a verified rectifier.
"""
import csv,re,subprocess,json
from pathlib import Path
import numpy as np
from run_lab import prepare,parse_wrdata,harmonics,op_nodes,OUTFIELDS

ROOT=Path("output/psu_thd_compare");ROOT.mkdir(parents=True,exist_ok=True)
def measure(psu,knob,dbu,cv):
    c=dict(mode="A-436B",hz=1000,dbu=dbu,attack=0,
           recovery=0,hold=False,input_knob=knob)
    d=ROOT/f"{psu}_{int(knob*100)}pct_L{str(dbu).replace('-','m')}_CV{str(cv).replace('-','m')}"
    f=prepare(c,d);s=f.read_text()
    if psu=="UNREGULATED":
        a="VHT bplus 0 {BPLUS}"
        b="VHT htopen 0 340\nRSOURCE_BPLUS htopen bplus 3900"
        assert a in s;s=s.replace(a,b)
    # Isolate 6AL5 to compare supply effect at EXACT same control voltage.
    lines=s.splitlines()
    n=0
    for i,line in enumerate(lines):
        if line.startswith("R12 detector cv ") or line.startswith("C4 cv 0 ") or line.startswith("R9 cv 0 "):
            lines[i]="* "+line;n+=1
    assert n==3
    s="\n".join(lines)+"\n"
    s=s.replace(".control\n",f"VFORCE cv 0 {cv}\n.control\n")
    s=re.sub(r"tran [^\n]+(?=\nwrdata tran.txt)","tran 20u 0.17 0 20u",s,count=1)
    f.write_text(s)
    try:
        p=subprocess.run(["ngspice","-b",f.name],cwd=d,
                         capture_output=True,text=True,timeout=50)
    except subprocess.TimeoutExpired:
        return dict(psu=psu,knob_pct=int(knob*100),input_dBu=dbu,forced_cv=cv,
                    status="TIMEOUT")
    log=p.stdout+"\n"+p.stderr
    (d/"log.txt").write_text(log)
    if p.returncode or not (d/"tran.txt").exists():
        return dict(psu=psu,knob_pct=int(knob*100),input_dBu=dbu,forced_cv=cv,
                    status="FAILED",error=log[-500:])
    w=parse_wrdata(d/"tran.txt",len(OUTFIELDS))
    out=harmonics(w[:,0],w[:,1],1000)
    op=op_nodes(log)
    gv=lambda name:op.get(f"v({name})",float("nan"))
    return dict(psu=psu,knob_pct=int(knob*100),input_dBu=dbu,
        forced_cv=cv,status="OK",
        output_dBu=out["fundamental_output_dBu"],
        THD_percent=out["THD_percent"],
        **{f"K{k}_dBc":out.get(f"K{k}_dBc") for k in range(2,21)},
        reservoir_V=gv("bplus"),
        platefeed_6BC8_V=gv("platefeed"),plate_6BC8_V=gv("pa"),
        plate_6CG7_V=gv("p2a"),cathode_6CG7_V=gv("k2"))

results=[]
for psu in ("FIXED","UNREGULATED"):
 for knob in (.8,1.0):
    ref=measure(psu,knob,-36,0)
    if ref["status"]!="OK":
        print("PSU_REFERENCE_FAILED",ref,flush=True);continue
    gain=ref["output_dBu"]+36
    for cv in (-14,-15,-16,-17):
        m=measure(psu,knob,7,cv)
        if m["status"]=="OK":
            m["GR_from_linear_dB"]=7+gain-m["output_dBu"]
            print(f"PSU_COMPARE {psu} knob={int(knob*100)}% CV={cv}V "
                  f"GR={m['GR_from_linear_dB']:.2f}dB "
                  f"THD={m['THD_percent']:.3f}% "
                  f"platefeed={m['platefeed_6BC8_V']:.2f}V "
                  f"6BC8plate={m['plate_6BC8_V']:.2f}V "
                  f"6CG7plate={m['plate_6CG7_V']:.2f}V",flush=True)
        else:print("PSU_COMPARE_FAILED",m,flush=True)
        results.append(m)
if not results:raise RuntimeError("No measurements")
with (ROOT/"psu_thd_vs_gr.csv").open("w",newline="") as f:
    cols=sorted({key for x in results for key in x})
    w=csv.DictWriter(f,fieldnames=cols);w.writeheader();w.writerows(results)
(ROOT/"psu_thd_vs_gr.json").write_text(json.dumps(results,indent=2))
