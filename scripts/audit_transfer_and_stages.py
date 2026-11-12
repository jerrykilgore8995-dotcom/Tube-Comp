#!/usr/bin/env python3
"""Calibrate tubecomp gain reduction against SMALL-SIGNAL LINEAR extrapolation.

Comparing the detector-enabled output to a no-detector signal at the SAME HOT
input understates real GR when the no-detector amplifier has saturated. This
script records BOTH and shows harmonic buildup per differential stage. It
runs real ngspice, explicitly labels the still-provisional 436B topology.
"""
from pathlib import Path
import csv,json,math,subprocess
import numpy as np
from run_lab import prepare,parse_wrdata,harmonics,op_nodes,OUTFIELDS

ROOT=Path("output/transfer_stage_audit")
ROOT.mkdir(parents=True,exist_ok=True)
def measure(mode,dbu,off=False):
    case=dict(mode=mode,hz=1000,dbu=dbu,attack=0,recovery=0,
              hold=False,input_knob=.8)
    tag=(mode.replace("-","")+("_off" if off else "_on")+str(dbu).replace("-","m"))
    wd=ROOT/tag
    f=prepare(case,wd)
    if off:
        s=f.read_text()
        lines=s.splitlines()
        n=0
        for i,line in enumerate(lines):
            if line.startswith("DDET1 ") or line.startswith("DDET2 "):
                lines[i]="* "+line
                n+=1
        if n!=2:raise RuntimeError("Expected 2 detector diodes")
        f.write_text("\n".join(lines)+"\n")
    p=subprocess.run(["ngspice","-b",f.name],cwd=wd,
                     capture_output=True,text=True,timeout=65)
    log=p.stdout+"\n"+p.stderr
    (wd/"ngspice.log").write_text(log)
    if p.returncode!=0 or not (wd/"tran.txt").exists():
        raise RuntimeError(tag+" ngspice failed: "+log[-1400:])
    w=parse_wrdata(wd/"tran.txt",len(OUTFIELDS))
    t=w[:,0]
    def hh(y):return harmonics(t,y,1000)
    out=hh(w[:,1])
    nodes={
      "out":out,
      "6BC8_plateA":hh(w[:,4]),
      "6BC8_pp":hh(w[:,4]-w[:,5]),
      "6CG7_plateA":hh(w[:,6]),
      "6CG7_pp":hh(w[:,6]-w[:,7]),
    }
    op=op_nodes(log)
    tail=w[-max(10,len(w)//10):]
    return dict(mode=mode,input_dBu=dbu,detector_enabled=not off,
                output_dBu=out["fundamental_output_dBu"],
                output_THD_percent=out["THD_percent"],
                cv_mean_V=float(np.mean(tail[:,3])),
                v6BC8_plate_idle_V=op.get("v(pa)"),
                v6CG7_plate_idle_V=op.get("v(p2a)"),
                v6CG7_cathode_idle_V=op.get("v(k2)"),
                v6BC8_plate_late_mean_V=float(np.mean(tail[:,4])),
                v6CG7_plate_late_mean_V=float(np.mean(tail[:,6])),
                **{name+"_THD_percent":val["THD_percent"] for name,val in nodes.items()},
                **{name+"_K2_dBc":val["K2_dBc"] for name,val in nodes.items()},
                **{name+"_K3_dBc":val["K3_dBc"] for name,val in nodes.items()},
                **{name+"_fundamental_peak_V":val["fundamental_peak_volts"] for name,val in nodes.items()})

rows=[]
for mode in ("A-436B","RS124"):
    baseline_off=measure(mode,-36,True)
    linear_gain=baseline_off["output_dBu"]-(-36)
    for dbu in (-36,-24,-18,-12,-6,0):
        try:
            row=measure(mode,dbu,False)
        except Exception as e:
            print("STAGE_FAILED",mode,dbu,str(e)[-500:],flush=True)
            continue
        row["linear_reference_gain_dB"]=linear_gain
        row["actual_GR_vs_linear_dB"]=dbu+linear_gain-row["output_dBu"]
        rows.append(row)
        print(f"STAGE_DIAG {mode} {dbu:+}dBu "
              f"GR_LINEAR={row['actual_GR_vs_linear_dB']:.2f}dB "
              f"OUT_THD={row['output_THD_percent']:.3f}% "
              f"6BC8ppTHD={row['6BC8_pp_THD_percent']:.3f}% "
              f"6CG7ppTHD={row['6CG7_pp_THD_percent']:.3f}% "
              f"CV={row['cv_mean_V']:.2f}V "
              f"output={row['output_dBu']:.2f}dBu",flush=True)
if not rows:raise RuntimeError("No valid SPICE runs")
with (ROOT/"stages_and_real_GR.csv").open("w",newline="") as fh:
    w=csv.DictWriter(fh,fieldnames=list(rows[0].keys()))
    w.writeheader();w.writerows(rows)
(ROOT/"stages_and_real_GR.json").write_text(json.dumps(rows,indent=2))
