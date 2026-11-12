#!/usr/bin/env python3
"""Static bias-conditioned AC frequency/phase acceptance at several GR states.

These are real ngspice nonlinear operating-point linearizations, NOT claims
that small-signal response fully characterizes an audio-limited compressor.
Factory Altec 436B response ±1.5dB at 30Hz-15kHz is the benchmark.
"""
from pathlib import Path
import csv,json
import numpy as np
from static_cv_ac import simulate

ROOT=Path("output/frequency_phase_acceptance");ROOT.mkdir(exist_ok=True,parents=True)
rows=[];summaries=[]
for mode in ("A-436B","RS124"):
 for cv in (0,-6,-12,-15,-17):
    wd,f,g,phase,delay,op=simulate(mode,float(cv),ROOT/"raw")
    ix=int(np.argmin(abs(f-1000)))
    g0=float(g[ix])
    valid=(f>=30)&(f<=15000)
    low=float(np.min(g[valid]-g0))
    high=float(np.max(g[valid]-g0))
    passes=(low>=-1.5 and high<=1.5)
    j=int(np.argmin(abs(f-1000)))
    summary=dict(mode=mode,forced_cv_V=cv,
       one_kHz_gain_dB=g0,
       one_kHz_phase_deg=float(phase[j]),
       one_kHz_group_delay_ms=float(delay[j]),
       low_dB_vs_1k=low,high_dB_vs_1k=high,
       meets_Altec_1p5dB_30Hz_to_15kHz=passes,
       plate_6BC8_V=op.get("v(pa)"),
       plate_6CG7_V=op.get("v(p2a)"))
    summaries.append(summary)
    print(f"FREQUENCY_ACCEPTANCE {mode} CV={cv:+}V "
          f"relative_30_to_15k_dB=[{low:.2f},{high:.2f}] "
          f"PASS={passes} 1kHzdelay={delay[j]:.4f}ms",flush=True)
    for freq,mag,ph,gd in zip(f,g,phase,delay):
        rows.append(dict(mode=mode,cv_V=cv,frequency_hz=float(freq),
            relative_gain_dB=float(mag-g0),phase_deg=float(ph),
            group_delay_ms=float(gd)))
with (ROOT/"frequency_phase_all_points.csv").open("w",newline="") as fp:
    w=csv.DictWriter(fp,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
with (ROOT/"acceptance_summary.csv").open("w",newline="") as fp:
    w=csv.DictWriter(fp,fieldnames=list(summaries[0]));w.writeheader();w.writerows(summaries)
(ROOT/"acceptance_summary.json").write_text(json.dumps(summaries,indent=2))
