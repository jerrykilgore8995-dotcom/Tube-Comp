#!/usr/bin/env python3
"""Long 1.6 s closed-loop 436B/RS124 compression and THD validation.

This verifies the end of the B+ 10k/47uF settling and 6AL5 feedback, rather
than trusting the 0.22s ramp measurements.  Maximum 30dB Altec factory test
is +7dBm into 600ohm with +19dBm loaded output; simulated source uses 600ohm
series impedance, and output is loaded with 600ohm.

K2-K20 read from 1kHz coherent last-24-cycle window at 15us max timestep;
harmonics >= Nyquist receive 'None' as run_lab harmonics() specifies.
"""
from pathlib import Path
import csv,json,re,subprocess,os
import numpy as np
from run_lab import prepare,parse_wrdata,harmonics,op_nodes,OUTFIELDS

ROOT=Path("output/long_steady_compression")/os.getenv("TUBECOMP_PARITY_MODE","baseline");ROOT.mkdir(parents=True,exist_ok=True)
def measure(mode,level,knob):
    case=dict(mode=mode,hz=1000,dbu=level,attack=0,recovery=0,
              hold=False,input_knob=knob)
    wd=ROOT/f"{mode.replace('-','')}_L{str(level).replace('-','m')}_G{int(knob*100)}"
    f=prepare(case,wd)
    s=f.read_text()
    old="VIN source 0 SIN(0 {AMP} {FREQ}) AC 1"
    new="BINPUT source 0 V={AMP*sin(2*pi*FREQ*time)*(0.5-0.5*cos(pi*min(time/0.04,1)))}"
    if old not in s:raise RuntimeError("Cannot find VIN")
    s=s.replace(old,new)
    s=re.sub(r"tran [^\n]+(?=\nwrdata tran.txt)","tran 15u 1.6 0 15u",s,count=1)
    old_w="wrdata tran.txt v(out) v(in) v(cv) v(pa) v(pb) v(p2a) v(p2b) v(k1) v(k2)"
    if old_w not in s:raise RuntimeError("missing waveform outputs")
    s=s.replace(old_w,old_w+" v(platefeed) v(bplus) v(outputfeed)")
    f.write_text(s)
    try:
        p=subprocess.run(["ngspice","-b","case.cir"],cwd=wd,
                         text=True,capture_output=True,timeout=200)
    except subprocess.TimeoutExpired:
        return dict(mode=mode,input_dBu=level,input_pct=int(knob*100),status="TIMEOUT")
    log=p.stdout+"\n"+p.stderr
    (wd/"ngspice.log").write_text(log)
    if p.returncode or not (wd/"tran.txt").exists():
        return dict(mode=mode,input_dBu=level,input_pct=int(knob*100),
                    status="FAILED",error=log[-800:])
    cols=parse_wrdata(wd/"tran.txt",len(OUTFIELDS)+3)
    t=cols[:,0];out=cols[:,1];cv=cols[:,3]
    end=harmonics(t,out,1000)
    stage1=harmonics(t,cols[:,4]-cols[:,5],1000)
    stage2=harmonics(t,cols[:,6]-cols[:,7],1000)
    tail=cols[t>=t[-1]-.05]
    early=cols[(t>=t[-1]-.10)&(t<t[-1]-.05)]
    snapshots={}
    for at in (.25,.5,.75,1.0,1.25,1.5,1.6):
        samples=(t>=at-.024)&(t<=at)
        if np.count_nonzero(samples)>=96:
            h=harmonics(t[samples],out[samples],1000)
            snapshots[str(at)]=dict(out_dBu=h["fundamental_output_dBu"],
                thd_percent=h["THD_percent"],
                cv_V=float(np.mean(cv[samples])))
    op=op_nodes(log)
    v=lambda node:op.get("v("+node+")")
    return dict(mode=mode,input_dBu=level,input_pct=int(knob*100),status="OK",
        final_output_dBu=end["fundamental_output_dBu"],
        final_THD_percent=end["THD_percent"],
        final_CV_V=float(np.mean(tail[:,3])),
        CV_drift_final_50ms_V=float(np.mean(tail[:,3])-np.mean(early[:,3])),
        platefeed_6bc8_late_V=float(np.mean(tail[:,10])),
        bplus_late_V=float(np.mean(tail[:,11])),
        outputfeed_6cg7_late_V=float(np.mean(tail[:,12])),
        plate_6bc8_late_V=float(np.mean(tail[:,4])),
        plate_6cg7_late_V=float(np.mean(tail[:,6])),
        stage_6BC8_THD_percent=stage1["THD_percent"],
        stage_6CG7_THD_percent=stage2["THD_percent"],
        **{f"K{k}_dBc":end.get(f"K{k}_dBc") for k in range(2,21)},
        snapshots=snapshots,
        plate_6bc8_idle_V=v("pa"),plate_6cg7_idle_V=v("p2a"),
        supply_platefeed_idle_V=v("platefeed"))

def main():
    rows=[]
    parity_mode=os.getenv("TUBECOMP_PARITY_MODE")
    cases=([(mode,knob,level)
           for mode in (parity_mode,)
           for knob in (.8,1.)
           for level in (-24,-18,-12,-6,0,3,7,10,13)]
           if parity_mode in ("A-436B","RS124") else
           [("A-436B",.8,7),("A-436B",1.,7),
            ("A-436B",1.,10),("A-436B",1.,13),
            ("RS124",.8,7),("RS124",1.,7)])
    for mode,knob,level in cases:
        row=measure(mode,level,knob)
        rows.append(row)
        if row["status"]=="OK":
            print(f"LONG_STEADY {mode} input={int(knob*100)}% {level:+}dBu "
                  f"OUT={row['final_output_dBu']:+.2f}dBu "
                  f"THD={row['final_THD_percent']:.3f}% "
                  f"CV={row['final_CV_V']:.2f}V "
                  f"CVdrift50ms={row['CV_drift_final_50ms_V']:+.3f}V "
                  f"6BC8plate={row['plate_6bc8_late_V']:.2f}V "
                  f"Bplus_filtered={row['platefeed_6bc8_late_V']:.2f}V",
                  flush=True)
        else:print("LONG_STEADY_FAILED",row,flush=True)
    with (ROOT/"long_steady.csv").open("w",newline="") as fh:
        flat=[{k:v for k,v in x.items() if k!="snapshots"} for x in rows]
        cols=sorted({k for x in flat for k in x});w=csv.DictWriter(fh,fieldnames=cols)
        w.writeheader();w.writerows(flat)
    (ROOT/"long_steady.json").write_text(json.dumps(rows,indent=2))

if __name__=='__main__':
    main()
