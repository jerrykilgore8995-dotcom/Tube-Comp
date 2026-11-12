#!/usr/bin/env python3
"""Measure live 6AL5 compressor over source level and input controls.

Unlike forced-CV AC tests, these are full ngspice *closed-loop* transients.
A 40ms raised-cosine source fade-in avoids artificial hot startup clipping
before the feedback loop has charged.  The ramp is fully finished when
the last 24 cycles (THD/GR window) are measured.

The source is unloaded dBu with a 600 ohm series generator. Loading is
set by Peerless 4651A modeled bridge; output is terminated with 600 ohms.
"""
import csv,json,re,subprocess,math
from pathlib import Path
import numpy as np
from run_lab import prepare,parse_wrdata,harmonics,op_nodes,OUTFIELDS

ROOT=Path("output/live_compression_range")
ROOT.mkdir(exist_ok=True,parents=True)

def measure(mode,dbu,knob,detector=True):
    c=dict(mode=mode,hz=1000,dbu=dbu,attack=0,recovery=0,
           hold=False,input_knob=knob)
    tag=f"{mode.replace('-','')}_L{str(dbu).replace('-','m')}_G{round(knob*100)}_{int(detector)}"
    wd=ROOT/tag
    deck=prepare(c,wd)
    text=deck.read_text()
    if not detector:
        lines=text.splitlines();n=0
        for j,line in enumerate(lines):
            if line.startswith("DDET1 ") or line.startswith("DDET2 "):
                lines[j]="* "+line;n+=1
        if n!=2:raise RuntimeError("Could not disable detector for low-signal baseline")
        text="\n".join(lines)+"\n"
    # Smooth 40 ms fade removes uncompressed full-scale startup transient:
    # reaches 100% and stays constant from t=40ms to the analysis region.
    a="VIN source 0 SIN(0 {AMP} {FREQ}) AC 1"
    b="BINPUT source 0 V={AMP*sin(2*pi*FREQ*time)*(0.5-0.5*cos(pi*min(time/0.04,1)))}"
    if a not in text:raise RuntimeError("Cannot find voltage source")
    text=text.replace(a,b)
    text=re.sub(r"tran [^\n]+(?=\nwrdata tran.txt)",
                "tran 15u 0.22 0 15u",text,count=1)
    deck.write_text(text)
    try:
        p=subprocess.run(["ngspice","-b","case.cir"],cwd=wd,
            capture_output=True,text=True,timeout=55)
    except subprocess.TimeoutExpired:
        return dict(mode=mode,input_dBu=dbu,input_control_pct=int(knob*100),
                    status="TIMEOUT")
    logs=p.stdout+"\n"+p.stderr
    (wd/"ngspice.log").write_text(logs)
    if p.returncode or not (wd/"tran.txt").exists():
        return dict(mode=mode,input_dBu=dbu,input_control_pct=int(knob*100),
                    status="NGSPICE_FAILED",error=logs[-700:])
    try:
        w=parse_wrdata(wd/"tran.txt",len(OUTFIELDS))
        h=harmonics(w[:,0],w[:,1],1000)
        pa=harmonics(w[:,0],w[:,4]-w[:,5],1000)
        pb=harmonics(w[:,0],w[:,6]-w[:,7],1000)
        tail=w[w[:,0]>w[-1,0]-.024]
        before=w[(w[:,0]>w[-1,0]-.048)&(w[:,0]<w[-1,0]-.024)]
        cv1=float(np.mean(tail[:,3]))
        cv0=float(np.mean(before[:,3])) if len(before) else float('nan')
        op=op_nodes(logs)
        return dict(mode=mode,input_dBu=dbu,input_control_pct=int(knob*100),
            status="OK",output_dBu=h["fundamental_output_dBu"],
            THD_percent=h["THD_percent"],
            **{f"K{k}_dBc":h.get(f"K{k}_dBc") for k in range(2,21)},
            cv_last_V=cv1,cv_last_24ms_change_V=cv1-cv0,
            stage_6BC8_THD_percent=pa["THD_percent"],
            stage_6CG7_THD_percent=pb["THD_percent"],
            plate_6bc8_idle_V=op.get("v(pa)"),
            plate_6cg7_idle_V=op.get("v(p2a)"),
            platefeed_idle_V=op.get("v(platefeed)"),
            k2_idle_V=op.get("v(k2)"))
    except Exception as ex:
        return dict(mode=mode,input_dBu=dbu,input_control_pct=int(knob*100),
                    status="ANALYSIS_ERROR",error=str(ex))

rows=[]
for mode in ("A-436B","RS124"):
 for knob in (.8,1.):
    base=measure(mode,-36,knob,False)
    if base["status"]!="OK":
        print("LIVE_RANGE_BASELINE_FAILED",base,flush=True);continue
    gain=base["output_dBu"]+36
    for dbu in (-18,-12,-6,0,3,7):
        r=measure(mode,dbu,knob,True)
        if r["status"]=="OK":
            r["linear_gain_dB"]=gain
            r["true_GR_dB"]=dbu+gain-r["output_dBu"]
            r["meets_2p5pct_THD"]=r["THD_percent"]<2.5
            r["cv_stable_0p5V"]=abs(r["cv_last_24ms_change_V"])<.5
            print(f"LIVE_RANGE {mode} knob={int(knob*100)}% {dbu:+}dBu "
                  f"GR={r['true_GR_dB']:.2f}dB THD={r['THD_percent']:.3f}% "
                  f"CV={r['cv_last_V']:.2f}V "
                  f"CVdrift={r['cv_last_24ms_change_V']:+.3f}V "
                  f"out={r['output_dBu']:+.2f}dBu",flush=True)
        else:
            print("LIVE_RANGE_FAILED",r,flush=True)
        rows.append(r)
if not rows:raise RuntimeError("No live dynamic measurements attempted")
with (ROOT/"live_range.csv").open("w",newline="") as fp:
    cols=sorted({k for row in rows for k in row})
    w=csv.DictWriter(fp,fieldnames=cols);w.writeheader();w.writerows(rows)
(ROOT/"live_range.json").write_text(json.dumps(rows,indent=2))
