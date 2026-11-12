#!/usr/bin/env python3
"""ngspice A/B 6BC8 Sylvania-Koren vs dual-region semi-remote-cutoff.

Deep bias is calibrated against Altec's two distinct idle/compressed schematic
annotations, not claimed factory exact. Check RMS/THD and both plate voltages.
"""
from pathlib import Path
import csv,re,subprocess
import numpy as np
from run_lab import prepare,parse_wrdata,harmonics,op_nodes,OUTFIELDS
ROOT=Path("output/6bc8_remote_compare")
ROOT.mkdir(parents=True,exist_ok=True)
def run(model,dbu,detector=True,cv_override=None,bplus_override=None):
 case=dict(mode="A-436B",hz=1000,dbu=dbu,attack=0,recovery=0,
           hold=False,input_knob=.8)
 suffix=f"{model}_L{str(dbu).replace('-','m')}_{'on' if detector else 'off'}"
 if cv_override is not None:suffix+=f"_CV{str(cv_override).replace('-','m')}"
 wd=ROOT/suffix
 deck=prepare(case,wd)
 s=deck.read_text()
 if model=="REMOTE":
  s,n=re.subn(r"\bk1 V6BC8(?=\n)","k1 V6BC8_REMOTE",s)
  if n!=2:raise RuntimeError("6BC8 two-device replacement failed")
 if not detector:
  s,n=re.subn(r"^DDET[12]\s+.*$",lambda m:"* "+m.group(0),s,count=2,flags=re.M)
  if n!=2:raise RuntimeError("Failed detector-off ref")
 if cv_override is not None:
  lines=s.splitlines()
  for prefix in ("R12 detector cv ","C4 cv 0 ","R9 cv 0 "):
   matches=[i for i,x in enumerate(lines) if x.startswith(prefix)]
   if len(matches)!=1:raise RuntimeError(f"Expected {prefix}")
   lines[matches[0]]="* "+lines[matches[0]]
  s="\n".join(lines)+"\n"
  s=s.replace(".control\n",f"VFORCE cv 0 {cv_override}\n.control\n",1)
 if bplus_override is not None:
  s,n=re.subn(r"\bBPLUS=272",f"BPLUS={bplus_override}",s,count=1)
  if n!=1:raise RuntimeError("No upstream BPLUS param")
 if cv_override is not None and bplus_override is not None:
  # Factory compressed stage operating-point diagnostic only.
  s,n=re.subn(r"(?s)\.control\n.*?\.endc",lambda m:".control\nop\nprint v(bplus) v(platefeed) v(pa) v(k1) v(ga) v(cv)\nquit\n.endc",s,count=1)
 deck.write_text(s)
 try:
  p=subprocess.run(["ngspice","-b",deck.name],cwd=wd,capture_output=True,text=True,timeout=55)
 except subprocess.TimeoutExpired:
  print(f"REMOTE_TIMEOUT {suffix}",flush=True);return None
 text=p.stdout+"\n"+p.stderr
 (wd/"log.txt").write_text(text)
 ops=op_nodes(text)
 if p.returncode or (cv_override is None and not (wd/"tran.txt").exists()):
  print(f"REMOTE_FAIL {suffix} "+text[-700:],flush=True);return None
 row=dict(model=model,input_dBu=dbu,detector=detector,forced_CV=cv_override,
          upstream_BPLUS=bplus_override,platefeed_V=ops.get("v(platefeed)"),
          plate_6BC8_V=ops.get("v(pa)"),cathode_6BC8_V=ops.get("v(k1)"))
 if cv_override is None or bplus_override is None:
  w=parse_wrdata(wd/"tran.txt",len(OUTFIELDS))
  h=harmonics(w[:,0],w[:,1],1000)
  plate=harmonics(w[:,0],w[:,4]-w[:,5],1000)
  row.update(output_dBu=h["fundamental_output_dBu"],
             output_THD_percent=h["THD_percent"],K3_dBc=h["K3_dBc"],
             varimu_THD_percent=plate["THD_percent"],
             late_CV_V=float(np.mean(w[-max(10,len(w)//10):,3])))
 return row

rows=[]
for model in ("KOREN","REMOTE"):
 for condition in ("IDLE","COMPRESSED"):
  if condition=="IDLE":
   row=run(model,-36,detector=False,cv_override=0,bplus_override=272)
  else:
   row=run(model,-36,detector=False,cv_override=-30,bplus_override=290)
  if row:
   rows.append(row)
   print(f"REMOTE_OP {model} {condition} platefeed={row['platefeed_V']} "
         f"6BC8_plate={row['plate_6BC8_V']} "
         f"6BC8_cathode={row['cathode_6BC8_V']}",flush=True)
 baseline=run(model,-36,False)
 if baseline is None:continue
 nominal=baseline["output_dBu"]+36
 for level in (-24,-18,-12,-6):
  x=run(model,level,True)
  if not x:continue
  gr=level+nominal-x["output_dBu"]
  x["linear_reference_GR_dB"]=gr
  rows.append(x)
  print(f"REMOTE_AUDIO {model} {level:+}dBu "
        f"GR={gr:.3f}dB THD={x['output_THD_percent']:.3f}% "
        f"6BC8_THD={x['varimu_THD_percent']:.3f}% "
        f"CV={x['late_CV_V']:.3f}V",flush=True)
if len(rows)<8:raise RuntimeError(f"Only {len(rows)} points. Expected at least 8")
headers=sorted(set().union(*(set(x.keys()) for x in rows)))
with (ROOT/"comparison.csv").open("w",newline="") as fp:
 w=csv.DictWriter(fp,fieldnames=headers)
 w.writeheader();w.writerows(rows)
