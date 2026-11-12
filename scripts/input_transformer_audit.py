#!/usr/bin/env python3
"""Targeted ngspice measurement of Peerless 4651A proxy / 2x 50k grid pots.

This is a physically connected linear-transformer reference, not a proof of
transformer core behavior or an original Peerless 4651A winding measurement.
"""
from pathlib import Path
import argparse
import csv
import re
import subprocess
import numpy as np
from run_lab import prepare, parse_wrdata

def run_case(output,mode,knob,source_ohms):
    c=dict(mode=mode,hz=1000,dbu=-24,attack=0,recovery=0,hold=False,input_knob=knob)
    wd=output/f"{mode.replace('-','')}_I{round(knob*100)}_S{source_ohms}"
    deck=prepare(c,wd)
    s=deck.read_text()
    s,n=re.subn(r"^RSOURCE source in \S+$",
                f"RSOURCE source in {source_ohms}",s,flags=re.MULTILINE)
    if n!=1:raise RuntimeError("Source impedance element not found")
    ac_control=r""".control
set wr_singlescale
set wr_vecnames
op
ac lin 4 999 1002
let zin = v(in)/(-i(VIN))
let gridgain = v(ga,gb)/v(source)
wrdata input_load.txt zin gridgain
quit
.endc"""
    s,n=re.subn(r"(?s)\.control\n.*?\.endc",lambda m:ac_control,s,count=1)
    if n!=1:raise RuntimeError("Missing .control section")
    deck.write_text(s)
    p=subprocess.run(["ngspice","-b",deck.name],cwd=wd,
                     capture_output=True,text=True,timeout=25)
    log=p.stdout+"\n"+p.stderr
    (wd/"ngspice.log").write_text(log)
    if p.returncode!=0 or not (wd/"input_load.txt").exists():
        raise RuntimeError("ngspice AC failed "+log[-1500:])
    table=parse_wrdata(wd/"input_load.txt",4)
    v=table[int(np.argmin(np.abs(table[:,0]-1000)))]
    zin=complex(v[1],v[2])
    gain=complex(v[3],v[4])
    return dict(mode=mode,knob_fraction=knob,source_ohms=source_ohms,
                frequency_hz=float(v[0]),input_ohms=abs(zin),
                input_resistive_ohms=zin.real,
                input_reactive_ohms=zin.imag,
                grid_differential_gain=abs(gain),
                grid_differential_gain_db=float(20*np.log10(max(1e-20,abs(gain)))),
                grid_phase_deg=float(np.degrees(np.angle(gain))))

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--output",default="output/input_transformer")
    args=parser.parse_args()
    output=Path(args.output)
    output.mkdir(exist_ok=True,parents=True)
    rows=[]
    for mode in ("A-436B","RS124"):
        for src in (600,15000):
            for knob in (.05,.25,.5,.8,1.):
                row=run_case(output,mode,knob,src)
                rows.append(row)
                print(f"INPUT_AUDIT {mode} source={src}Ω "
                      f"input={knob:.2f} Zin={row['input_ohms']:.0f}Ω "
                      f"ReZ={row['input_resistive_ohms']:.0f}Ω "
                      f"gridgain={row['grid_differential_gain_db']:+.2f}dB "
                      f"gridphase={row['grid_phase_deg']:+.2f}deg",
                      flush=True)
    with (output/"input_loading_1khz.csv").open("w",newline="") as fh:
        w=csv.DictWriter(fh,fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
if __name__=="__main__":main()
