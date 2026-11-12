#!/usr/bin/env python3
"""Full physical 200Ω secondary load, closed-tube SPICE .AC FR/phase states.

This is a forced 6BC8 grid-control voltage linearization, not a 1.6s
closed-loop compression trace. Same 10 mode/CV states, frequency grid,
source and pre-output-pad topology as the validated 600ohm .AC baseline.
"""
import csv,json
from pathlib import Path
import numpy as np
from static_cv_ac import simulate

ROOT=Path("output/frequency_phase_load200")
ROOT.mkdir(parents=True,exist_ok=True)
rows=[];summary=[]
for mode in ("A-436B","RS124"):
 for cv in (0,-6,-12,-15,-17):
  wd,fr,gain,phase,groupdelay,op=simulate(mode,cv,ROOT/"raw",load_ohms=200)
  index=int(np.argmin(np.abs(fr-1000)))
  g1=float(gain[index])
  for hz,m,p,t in zip(fr,gain,phase,groupdelay):
   rows.append(dict(mode=mode,cv_V=cv,frequency_hz=float(hz),
       relative_gain_dB=float(m-g1),phase_deg=float(p),
       group_delay_ms=float(t)))
  item=dict(mode=mode,cv_V=cv,gain_1k_dB=g1,
            phase_1k_deg=float(phase[index]),
            delay_1k_ms=float(groupdelay[index]),
            plate_6BC8_V=op.get("v(pa)"),plate_6CG7_V=op.get("v(p2a)"))
  summary.append(item)
  print("LOAD200_FREQ_PHASE",item,flush=True)
with (ROOT/"frequency_phase_all_points.csv").open("w",newline="") as fp:
 w=csv.DictWriter(fp,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
with (ROOT/"phase200_summary.csv").open("w",newline="") as fp:
 w=csv.DictWriter(fp,fieldnames=list(summary[0]));w.writeheader();w.writerows(summary)
