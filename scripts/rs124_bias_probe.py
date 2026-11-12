#!/usr/bin/env python3
"""Probe RS124 mode's provisional 6BC8 idle bias against THD and true GR.

No historical 436B/RS124 correctness is implied by a lower THD alone.
The tube, PSU, transformers and output cathode loads are unchanged.
"""
from pathlib import Path
import csv,re,subprocess
import numpy as np
from run_lab import prepare,parse_wrdata,harmonics,op_nodes,OUTFIELDS
ROOT=Path("output/rs124_bias_probe");ROOT.mkdir(exist_ok=True,parents=True)
def measure(bias,dbu,detector=True):
 c=dict(mode="RS124",hz=1000,dbu=dbu,attack=0,recovery=0,hold=False,input_knob=.8)
 wd=ROOT/f"G{str(bias).replace('-','m').replace('.','p')}_L{str(dbu).replace('-','m')}_{int(detector)}"
 deck=prepare(c,wd)
 s=deck.read_text()
 s=s.replace("BIAS=-1.0",f"BIAS={bias}",1)
 if not detector:
  s,n=re.subn(r"^DDET[12]\s+.*$",lambda m:"* "+m.group(0),s,count=2,flags=re.M)
  assert n==2
 deck.write_text(s)
 p=subprocess.run(["ngspice","-b",deck.name],cwd=wd,
   capture_output=True,text=True,timeout=70)
 txt=p.stdout+"\n"+p.stderr
 if p.returncode or not (wd/"tran.txt").exists():
  raise RuntimeError("Invalid ngspice "+txt[-1000:])
 w=parse_wrdata(wd/"tran.txt",len(OUTFIELDS))
 h=harmonics(w[:,0],w[:,1],1000)
 op=op_nodes(txt)
 return dict(output_dBu=h["fundamental_output_dBu"],
             THD_percent=h["THD_percent"],
             K2_dBc=h["K2_dBc"],K3_dBc=h["K3_dBc"],
             CV_mean_V=float(np.mean(w[-max(10,len(w)//10):,3])),
             plate_6BC8_V=op.get("v(pa)"),
             cathode_6BC8_V=op.get("v(k1)"),
             plate_6CG7_V=op.get("v(p2a)"),
             cathode_6CG7_V=op.get("v(k2)"))
rows=[]
for bias in (-0.5,-0.65,-0.8,-0.9,-1.0,-1.1,-1.2):
 try:
  ref=measure(bias,-36,False)
  smallgain=ref["output_dBu"]+36
  for level in (-18,-6):
   x=measure(bias,level)
   row=dict(bias_V=bias,input_dBu=level,
      actual_gain_reduction_dB=level+smallgain-x["output_dBu"],
      **x)
   rows.append(row)
   print(f"BIAS_RS124 {bias:+.2f} V {level:+d}dBu "
      f"THD={x['THD_percent']:.3f}% "
      f"GR={row['actual_gain_reduction_dB']:.2f}dB "
      f"plate={x['plate_6BC8_V']:.2f}V CV={x['CV_mean_V']:.2f}V",flush=True)
 except Exception as ex:
  print("BIAS_RS124_FAIL",bias,str(ex)[-600:],flush=True)
if not rows:raise RuntimeError("No successful bias tests")
with (ROOT/"bias_thd_gr.csv").open("w",newline="") as f:
 w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
