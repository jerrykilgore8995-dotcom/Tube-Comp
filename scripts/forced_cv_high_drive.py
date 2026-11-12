#!/usr/bin/env python3
"""Forced-CV high-level static tube THD, at correct idle operating point.

This bypasses the TIME-VARYING 6AL5 sidechain so plate-stage distortion can be
measured in steady bias at 0 / +7dBu. It is NOT a closed-loop measurement.
Use alongside the separate live detector GR and output compression test.
"""
from pathlib import Path
import csv,re,subprocess,math
import numpy as np
from run_lab import prepare,parse_wrdata,harmonics,op_nodes,OUTFIELDS
ROOT=Path("output/forced_cv_high_drive")
ROOT.mkdir(parents=True,exist_ok=True)
def sim(dbu,cv,knob=.8):
 c=dict(mode="A-436B",hz=1000,dbu=dbu,attack=0,recovery=0,hold=False,input_knob=knob)
 wd=ROOT/f"level{str(dbu).replace('-','m')}_cv{str(cv).replace('-','m')}_input{int(knob*100)}"
 deck=prepare(c,wd)
 s=deck.read_text()
 for prefix in ("R12 detector cv ","C4 cv 0 ","R9 cv 0 "):
  lines=s.splitlines();n=0
  for j,line in enumerate(lines):
   if line.startswith(prefix):lines[j]="* "+line;n+=1
  if n!=1:raise RuntimeError("Cannot disconnect "+prefix)
  s="\n".join(lines)+"\n"
 # Source biases the shared grid reference without changing signal topology.
 s=s.replace(".control\n",f"VFORCE cv 0 {cv}\n.control\n",1)
 # Fixed bias should suppress the huge initial uncompressed transient.
 s=s.replace("tran 20u 0.20 0 20u","tran 20u 0.17 0 20u")
 s=re.sub(r"tran [^\n]+(?=\nwrdata tran.txt)","tran 20u 0.17 0 20u",s)
 deck.write_text(s)
 try:
  p=subprocess.run(["ngspice","-b",deck.name],cwd=wd,text=True,capture_output=True,timeout=35)
 except subprocess.TimeoutExpired:
  print(f"FORCED_CV_TIMEOUT {dbu}dBu CV={cv}V",flush=True);return None
 txt=p.stdout+"\n"+p.stderr
 (wd/"log.txt").write_text(txt)
 if p.returncode!=0 or not (wd/"tran.txt").exists():
  print(f"FORCED_CV_FAILED {dbu}dBu CV={cv}V {txt[-350:]}",flush=True);return None
 w=parse_wrdata(wd/"tran.txt",len(OUTFIELDS))
 out=harmonics(w[:,0],w[:,1],1000)
 first=harmonics(w[:,0],w[:,4]-w[:,5],1000)
 second=harmonics(w[:,0],w[:,6]-w[:,7],1000)
 op=op_nodes(txt)
 row=dict(input_dBu=dbu,forced_cv_V=cv,input_control_pct=round(knob*100),
    output_dBu=out["fundamental_output_dBu"],
    output_thd_percent=out["THD_percent"],
    **{f"K{k}_dBc":out.get(f"K{k}_dBc") for k in range(2,21)},
    varimu_pp_thd_percent=first["THD_percent"],
    output_stage_pp_thd_percent=second["THD_percent"],
    plate_6BC8_V=op.get("v(pa)"),plate_6CG7_V=op.get("v(p2a)"))
 print(f"FORCED_CV {dbu:+}dBu input={int(knob*100)}% CV={cv:+}V OUT={row['output_dBu']:.2f}dBu "
       f"THD={row['output_thd_percent']:.3f}% "
       f"6BC8={row['varimu_pp_thd_percent']:.3f}% "
       f"6CG7={row['output_stage_pp_thd_percent']:.3f}%",flush=True)
 return row
rows=[]
for knob in (.8,1.0):
 baseline=sim(-36,0,knob)
 if baseline is None:raise RuntimeError(f"Reference at knob={knob} did not converge")
 linear_gain=baseline["output_dBu"]+36
 print(f"LINEAR_REF knob={knob:.1f} small-signal gain={linear_gain:.3f}dB",flush=True)
 for dbu in (0,7):
  for cv in (-12,-13,-14,-15,-16,-17,-18,-20,-22):
   v=sim(dbu,cv,knob)
   if v:
    v["gain_reduction_vs_linear_dB"]=dbu+linear_gain-v["output_dBu"]
    v["small_signal_gain_dB"]=linear_gain
    rows.append(v)
    print(f"GR_AT_CV input={dbu:+}dBu knob={int(knob*100)}% "
          f"CV={cv}V GR={v['gain_reduction_vs_linear_dB']:.2f}dB "
          f"THD={v['output_thd_percent']:.3f}%",flush=True)
if not rows:raise RuntimeError("All forced-CV tests failed")
with (ROOT/"output_thd_vs_forced_cv.csv").open("w",newline="") as fp:
 writer=csv.DictWriter(fp,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
