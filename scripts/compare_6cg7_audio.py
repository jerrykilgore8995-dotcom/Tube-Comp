#!/usr/bin/env python3
"""Compare full ngspice 6CG7 audio-stage behavior, not just DC operating points.

RCA single-anchor curve + 15k dropper against GE 1963 Koren curve + 20k.
Both yield ~149V idle plates at the user-selected 372V power-supply source.
Neither is a factory-original circuit reconstruction yet.
"""
from pathlib import Path
import csv
import math
import re
import subprocess
import tempfile
import numpy as np
from run_lab import prepare, parse_wrdata, harmonics, op_nodes, OUTFIELDS

def simulate(root,mode,dbu,model,feedback):
    c=dict(mode=mode,hz=1000,dbu=dbu,attack=0,recovery=0,
           hold=False,input_knob=.8)
    wd=root/(mode.replace("-","")+"_"+str(dbu).replace("-","m")+
             "_"+model+("_on" if feedback else "_off"))
    deck=prepare(c,wd)
    s=deck.read_text()
    if model=="GE":
        s,n=re.subn(r"\bk2 V6CG7(?=\n)", "k2 V6CG7_GE", s)
        if n!=2:raise RuntimeError("Could not replace both 6CG7 device models")
        s=s.replace("R6CG7_DROP=15000","R6CG7_DROP=20000")
    if not feedback:
        s,n=re.subn(r"^DDET[12]\s+.*$",lambda m:"* "+m.group(0),
                    s,count=2,flags=re.MULTILINE)
        if n!=2:raise RuntimeError("Cannot disable 6AL5 detector")
    deck.write_text(s)
    p=subprocess.run(["ngspice","-b",deck.name],cwd=wd,capture_output=True,
                     text=True,timeout=90)
    log=p.stdout+"\n"+p.stderr
    if p.returncode or not (wd/"tran.txt").exists():
        raise RuntimeError("Failed "+str(wd)+"\n"+log[-1200:])
    table=parse_wrdata(wd/"tran.txt",len(OUTFIELDS))
    h=harmonics(table[:,0],table[:,1],1000)
    dc=op_nodes(log)
    return dict(out_dBu=h["fundamental_output_dBu"],
                THD_percent=h["THD_percent"],
                K2_dBc=h["K2_dBc"],K3_dBc=h["K3_dBc"],
                CV_V=float(np.mean(table[-max(10,len(table)//10):,3])),
                plate_V=dc.get("v(p2a)"),
                cathode_V=dc.get("v(k2)"))

def main():
    root=Path("output/6cg7_audio_comparison")
    root.mkdir(parents=True,exist_ok=True)
    rows=[]
    for model in ("RCA","GE"):
        for mode in ("A-436B","RS124"):
            for dbu in (-18,-12,-6):
                a=simulate(root,mode,dbu,model,True)
                b=simulate(root,mode,dbu,model,False)
                row=dict(valve=model,mode=mode,input_dBu=dbu,
                         plate_V=a["plate_V"],cathode_V=a["cathode_V"],
                         gain_reduction_dB=b["out_dBu"]-a["out_dBu"],
                         THD_percent=a["THD_percent"],K2_dBc=a["K2_dBc"],
                         K3_dBc=a["K3_dBc"],CV_V=a["CV_V"],
                         output_dBu=a["out_dBu"])
                rows.append(row)
                print(f"TUBE_AUDIO {model} {mode} {dbu:+}dBu "
                      f"plate={row['plate_V']:.2f}V "
                      f"cathode={row['cathode_V']:.2f}V "
                      f"GR={row['gain_reduction_dB']:.2f}dB "
                      f"THD={row['THD_percent']:.3f}% "
                      f"K2={row['K2_dBc']:.2f}dBc "
                      f"K3={row['K3_dBc']:.2f}dBc",flush=True)
    with (root/"comparison.csv").open("w",newline="") as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
if __name__=="__main__":
    main()
