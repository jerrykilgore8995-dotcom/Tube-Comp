#!/usr/bin/env python3
"""Input-control grid with real physical 200-ohm transformer secondary load.

Unlike the RS124 output pad, this resistor loads the 6CG7 transformer stage
and changes its operating point, THD and 6AL5 feedback gain. 1.6 seconds of
ngspice settle per case. No 600-ohm lookup substitutions.
"""
from pathlib import Path
import csv,os,json
import long_steady_compression as lab
mode=os.environ.get("TUBECOMP_MODE","A-436B")
if mode not in ("A-436B","RS124"):raise ValueError("Bad mode")
lab.ROOT=Path("output/input_grid_200ohm")/mode
lab.ROOT.mkdir(parents=True,exist_ok=True)
rows=[]
for knobPct in range(0,101,10):
 for inputDbu in (-18,-6,7):
  r=lab.measure(mode,inputDbu,knobPct*.01,load_ohms=200)
  rows.append(r)
  if r["status"]!="OK":
   print("SPICE_200_FAIL",r,flush=True)
  else:
   print(f"SPICE_200 {mode} pot={knobPct}% {inputDbu:+}dBu "
         f"out={r['final_output_dBu']:.4f}dBu "
         f"CV={r['final_CV_V']:.4f}V "
         f"THD={r['final_THD_percent']:.4f}%",flush=True)
if len(rows)!=33 or any(r["status"]!="OK" for r in rows):
 raise RuntimeError("Missing one or more 200ohm SPICE cases")
flat=[{k:v for k,v in row.items() if k!="snapshots"} for row in rows]
with (lab.ROOT/"input200.csv").open("w",newline="") as f:
 w=csv.DictWriter(f,fieldnames=sorted(set().union(*(r.keys() for r in flat))))
 w.writeheader();w.writerows(flat)
(lab.ROOT/"input200.json").write_text(json.dumps(rows,indent=2))
