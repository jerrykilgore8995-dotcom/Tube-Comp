#!/usr/bin/env python3
"""Real ngspice .AC audit of nonlinear gain cell vs forced negative CV.

Reject the new 6BC8_REMOTE_EXP unless the *full circuit* transfer is monotonic.
The previous 6BC8_REMOTE sigmoid has a nonmonotonic -12 to -18V section.
"""
from pathlib import Path
import csv,re,subprocess,math
import numpy as np
from run_lab import prepare, parse_wrdata,op_nodes
ROOT=Path("output/6bc8_cv_monotonic")
ROOT.mkdir(parents=True,exist_ok=True)
rows=[]
for model in ("REMOTE","REMOTE_EXP"):
 for mode in ("A-436B","RS124"):
  ref=None
  previous=None
  for cv in (0,-3,-6,-9,-12,-15,-18,-22,-26,-30):
   wd=ROOT/f"{model}_{mode.replace('-','')}_CV{str(cv).replace('-','m')}"
   case=dict(mode=mode,hz=1000,dbu=-36,attack=0,recovery=0,
             hold=False,input_knob=.8)
   f=prepare(case,wd)
   s=f.read_text()
   actual="V6BC8_REMOTE" if model=="REMOTE" else "V6BC8_REMOTE_EXP"
   s,n=re.subn(r"\bk1 V6BC8(?:_REMOTE(?:_EXP)?)?(?=\n)",
               lambda match:"k1 "+actual,s)
   assert n==2
   lines=s.splitlines()
   for prefix in ("R12 detector cv ","C4 cv 0 ","R9 cv 0 "):
    idx=[i for i,v in enumerate(lines) if v.startswith(prefix)]
    if len(idx)!=1:raise RuntimeError("Expected RC part "+prefix)
    lines[idx[0]]="* "+lines[idx[0]]
   s="\n".join(lines)+"\n"
   s=s.replace(".control\n",f"VFORCE cv 0 {cv}\n.control\n",1)
   s,n=re.subn(r"(?s)\.control\n.*?\.endc",lambda m:
    ".control\nset wr_singlescale\nset wr_vecnames\nop\n"+
    "ac lin 4 999 1002\nwrdata ac_cv.txt v(out)\nquit\n.endc",s,count=1)
   assert n==1
   f.write_text(s)
   p=subprocess.run(["ngspice","-b","case.cir"],cwd=wd,capture_output=True,
                    text=True,timeout=20)
   txt=p.stdout+"\n"+p.stderr
   if p.returncode!=0 or not (wd/"ac_cv.txt").exists():
    raise RuntimeError(f"ngspice {model} {mode} {cv}: {txt[-850:]}")
   tab=parse_wrdata(wd/"ac_cv.txt",2)
   closest=tab[int(np.argmin(np.abs(tab[:,0]-1000)))]
   mag=20*math.log10(max(1e-20,math.hypot(closest[1],closest[2])))
   if ref is None:ref=mag
   gr=ref-mag
   delta=gr-previous if previous is not None else 0.
   row=dict(model=model,mode=mode,control_V=cv,one_kHz_gain_dB=mag,
            GR_from_uncompressed_dB=gr,delta_GR_dB=delta,
            monotonic=delta>=-0.15)
   rows.append(row)
   print(f"MONOTONIC {model} {mode} CV={cv:>3}V "
         f"GR={gr:.3f}dB ΔGR={delta:+.3f}dB "
         f"PASS={delta>=-.15}",flush=True)
   if model=="REMOTE_EXP" and delta<-.15:
    raise RuntimeError(f"Exponential remote-cutoff has gain-regrowth {delta:.2f}dB at {cv}V")
   previous=gr
with (ROOT/"monotonic_ac.csv").open("w",newline="") as fp:
 w=csv.DictWriter(fp,fieldnames=list(rows[0]))
 w.writeheader();w.writerows(rows)
