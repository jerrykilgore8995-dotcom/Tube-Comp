#!/usr/bin/env python3
"""Real loaded output stage: 200Ω vs 600Ω at steady 1.6sec.

The RLOAD secondary resistor is INSIDE the SPICE transformer/6CG7 network,
not a post-processing gain pad. The physical output load alters voltages,
6CG7 current, 6AL5 feedback, phase and harmonic spectra. This is distinct
from RS124's ideal OUTPAD knob which sits after the sampled output node.
"""
import csv,json,os
from pathlib import Path
import long_steady_compression as lab
mode=os.getenv("TUBECOMP_MODE","A-436B")
if mode not in ("A-436B","RS124"):raise ValueError("Unsupported mode")
lab.ROOT=Path("output/impedance_grid")/mode
lab.ROOT.mkdir(parents=True,exist_ok=True)
rows=[]
for output_ohms in (200,600):
 for knob in (.8,1.):
  for input_dbu in (-18,-6,7):
   r=lab.measure(mode,input_dbu,knob,load_ohms=output_ohms)
   rows.append(r)
   if r["status"]=="OK":
    print(f"LOAD_200_600 {mode} Z={output_ohms} "
          f"G={knob:.1f} {input_dbu:+}dBu "
          f"out={r['final_output_dBu']:+.3f}dBu "
          f"THD={r['final_THD_percent']:.4f}% "
          f"CV={r['final_CV_V']:.4f}V",flush=True)
   else:print("LOAD_200_600_FAILED",r,flush=True)
if not all(r["status"]=="OK" for r in rows):
 raise RuntimeError("One or more physical output-load ngspice cases failed")
out=[{k:v for k,v in r.items() if k!="snapshots"} for r in rows]
with (lab.ROOT/"load_grid.csv").open("w",newline="") as fp:
 w=csv.DictWriter(fp,fieldnames=sorted(set().union(*(x.keys() for x in out))))
 w.writeheader();w.writerows(out)
(lab.ROOT/"load_grid.json").write_text(json.dumps(rows,indent=2))
