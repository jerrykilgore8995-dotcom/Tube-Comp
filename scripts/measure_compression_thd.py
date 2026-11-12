#!/usr/bin/env python3
"""Measure gain reduction and K2-K20 THD from actual ngspice audio-path runs.

The paired "detector disabled" case keeps the exact same signal path and input
level, but sets SCGAIN=0, removing the 6AL5 drive.  The level difference
estimates closed-loop reduction at the fundamental.  These are provisional
model results, not verified Altec or Chandler hardware data.
"""
from pathlib import Path
import argparse
import csv
import json
import math
import re
import subprocess
import numpy as np
from run_lab import prepare, parse_wrdata, harmonics, op_nodes, OUTFIELDS

def measure(case, workdir, disable_feedback=False):
    workdir.mkdir(parents=True, exist_ok=True)
    deck=prepare(case,workdir)
    if disable_feedback:
        source=deck.read_text()
        modified,n=re.subn(r"SCGAIN=[^\s]+","SCGAIN=0",source,count=1)
        if n!=1:raise RuntimeError("Failed to disable feedback detector")
        deck.write_text(modified)
    proc=subprocess.run(["ngspice","-b",deck.name],cwd=workdir,
                        capture_output=True,text=True,timeout=120)
    log=proc.stdout+"\n"+proc.stderr
    (workdir/"ngspice.log").write_text(log)
    if proc.returncode or not (workdir/"tran.txt").exists():
        raise RuntimeError("ngspice failed: "+log[-1200:])
    wave=parse_wrdata(workdir/"tran.txt",len(OUTFIELDS))
    time=wave[:,0]
    result=harmonics(time,wave[:,1],case["hz"])
    cv=float(np.mean(wave[-max(10,len(wave)//10):,3]))
    ops=op_nodes(log)
    return dict(
        output_dBu=result["fundamental_output_dBu"],
        thd_percent=result["THD_percent"],
        k_db={f"K{k}":result.get(f"K{k}_dBc") for k in range(2,21)},
        cv_V=cv,
        outfeed_V=ops.get("v(outputfeed)"),
        plate_6cg7_a_V=ops.get("v(p2a)"),
        plate_6cg7_b_V=ops.get("v(p2b)"),
        plate_6bc8_a_V=ops.get("v(pa)"),
        plate_6bc8_b_V=ops.get("v(pb)"),
        )
def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--output",default="output/compression_thd")
    args=parser.parse_args()
    root=Path(args.output)
    root.mkdir(parents=True,exist_ok=True)
    results=[]
    for mode in ("A-436B","RS124"):
        for level in (-36,-24,-18,-12,-6,0,6,12):
            case=dict(mode=mode,hz=1000,dbu=level,attack=0,recovery=0,
                      hold=False,input_knob=.8)
            prefix=f"{mode.replace('-','')}_L{str(level).replace('-','m')}"
            on=measure(case,root/"raw"/(prefix+"_on"),False)
            off=measure(case,root/"raw"/(prefix+"_sidechain_off"),True)
            gr=off["output_dBu"]-on["output_dBu"]
            row=dict(mode=mode,input_dBu=level,
                     output_with_feedback_dBu=on["output_dBu"],
                     output_detector_disabled_dBu=off["output_dBu"],
                     feedback_reduction_dB=gr,
                     THD_feedback_percent=on["thd_percent"],
                     THD_detector_disabled_percent=off["thd_percent"],
                     control_voltage_V=on["cv_V"],
                     output_feed_idle_V=on["outfeed_V"],
                     plate_6CG7_idle_V=on["plate_6cg7_a_V"])
            row.update({f"feedback_K{k}_dBc":on["k_db"][f"K{k}"]
                         for k in range(2,21)})
            results.append(row)
            print(f"{mode:7s} {level:+4d}dBu: "
                  f"feedback reduction={gr:.2f}dB, "
                  f"THD={on['thd_percent']:.3f}%, "
                  f"CV={on['cv_V']:.3f}V, "
                  f"6CG7 idle plate={on['plate_6cg7_a_V']:.2f}V",
                  flush=True)
    with (root/"compression_thd_1khz.csv").open("w",newline="") as output:
        writer=csv.DictWriter(output,fieldnames=list(results[0].keys()))
        writer.writeheader()
        writer.writerows(results)
    (root/"compression_thd_1khz.json").write_text(json.dumps(results,indent=2))
    summary={"description":"1kHz, matched-input detector-on versus detector-off ngspice",
             "results":len(results),
             "operating_point_6CG7_plate_V":results[0]["plate_6CG7_idle_V"],
             "important":"Output transformer winding DC resistances remain provisional 18k proxies."}
    (root/"summary.json").write_text(json.dumps(summary,indent=2))
    print(json.dumps(summary,indent=2))
if __name__=="__main__":
    main()
