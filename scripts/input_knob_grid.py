#!/usr/bin/env python3
"""1.6s ngspice grid through the ACTUAL dual-50k input-pot SPICE network.

11 knob positions (0..100% in 10% steps), 3 drive levels and both modes.
These measurements fill the critical 0..80% interpolation blind spot in the
initial 36-case 1kHz 8% DSP parity suite. Output pad tested separately.
"""
import csv,json,os
from pathlib import Path
import long_steady_compression as lab

mode=os.getenv("TUBECOMP_MODE","A-436B")
if mode not in ("A-436B","RS124"):raise RuntimeError("Bad mode")
lab.ROOT=Path("output/input_knob_grid")/mode
lab.ROOT.mkdir(parents=True,exist_ok=True)
rows=[]
for knobPct in range(0,101,10):
 for level in (-18,-6,7):
  knob=knobPct*.01
  row=lab.measure(mode,level,knob)
  rows.append(row)
  if row["status"]!="OK":
   print("INPUT_GRID_FAILED",mode,knobPct,level,row,flush=True)
  else:
   print(f"INPUT_GRID {mode} knob={knobPct:3d}% "
         f"{level:+d}dBu OUT={row['final_output_dBu']:+.3f}dBu "
         f"THD={row['final_THD_percent']:.4f}% "
         f"CV={row['final_CV_V']:.3f}V",flush=True)
if sum(r["status"]=="OK" for r in rows)!=33:
 raise RuntimeError("Not every one of 33 measured input/drive cases succeeded")
flat=[{k:v for k,v in r.items() if k!="snapshots"} for r in rows]
with (lab.ROOT/"input_grid.csv").open("w",newline="") as f:
 w=csv.DictWriter(f,fieldnames=sorted({k for r in flat for k in r}))
 w.writeheader();w.writerows(flat)
(lab.ROOT/"input_grid.json").write_text(json.dumps(rows,indent=2))
