#!/usr/bin/env python3
"""Real ngspice circuit-lab runner: every reported value comes from a SPICE deck.

IMPORTANT: All results before schematic and valve-curve verification are
NGSPICE_EXPLORATORY, NEVER measurements of a genuine Altec or Abbey Road unit.
"""
from __future__ import annotations
import argparse, csv, hashlib, json, math, os, re, shutil, subprocess, sys
from datetime import datetime, timezone
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
BASE_DECK = ROOT / "spice/provisional/436b_exploratory.cir"
LIB = ROOT / "spice/models/tube_cells.lib"
OUTFIELDS = ("v(out)", "v(in)", "v(cv)", "v(pa)", "v(pb)",
             "v(p2a)", "v(p2b)", "v(k1)", "v(k2)")


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def dbu_to_peak(dbu):
    return math.sqrt(2)*0.775 * 10**(dbu/20)

def mk_cases(broad=False):
    cases=[]
    # Distinct, explicit controls; no spurious claim of complete continuous coverage.
    freqs = [100,500,1000,5000,10000] if broad else [100,1000,5000]
    levels = [-36,-24,-18,-12,-6,0,6,12] if broad else [-24,-12,0]
    for mode in ["A-436B","RS124"]:
        for f in freqs:
            for level in levels:
                cases.append(dict(mode=mode,hz=f,dbu=level,attack=0,
                                  recovery=0,hold=False,input_knob=0.8))
    if broad:
        for attack in range(9):
            for rec in range(6):
                for level in [-12,0]:
                    cases.append(dict(mode="RS124",hz=1000,dbu=level,attack=attack,
                                      recovery=rec,hold=False,input_knob=.8))
        for p in [0.0,.1,.25,.5,.75,1.0]:
            for level in [-24,-6,6]:
                cases.append(dict(mode="A-436B",hz=1000,dbu=level,attack=0,
                                  recovery=0,hold=False,input_knob=p))
        for hold in [True,False]:
            for level in [-18,6]:
                cases.append(dict(mode="RS124",hz=1000,dbu=level,attack=4,
                                  recovery=2,hold=hold,input_knob=.8))
    return cases


def values(case):
    # These attack/recovery values are explicitly ENGINEERING HYPOTHESES.
    # Not verified historic resistor/capacitor values.
    rs_att=[12_000,22_000,33_000,47_000,68_000,100_000,150_000,220_000,330_000]
    rs_rec=[180_000,330_000,680_000,1_000_000,1_800_000,3_300_000]
    mode=case["mode"]
    return {
      "FREQ":case["hz"], "AMP":round(dbu_to_peak(case["dbu"]),9),
      "GAIN":max(.001,case["input_knob"]),
      "BIAS":-0.8 if mode=="A-436B" else -1.0,
      "ATTACK_R":47_000 if mode=="A-436B" else rs_att[case["attack"]],
      "REC_R":1_000_000 if mode=="A-436B" else
                 (1e12 if case["hold"] else rs_rec[case["recovery"]]),
      "OUTPAD":1.0 if mode=="A-436B" else 0.65,
      "SCGAIN":.22 if mode=="A-436B" else .32,
      "BPLUS":275
    }


def prepare(case, run_dir):
    run_dir.mkdir(parents=True, exist_ok=True)
    (run_dir/"models").mkdir(exist_ok=True)
    shutil.copy2(LIB, run_dir/"models/tube_cells.lib")
    text=BASE_DECK.read_text()
    d=values(case)
    for k,v in d.items():
        text=re.sub(r"(?<![A-Za-z0-9_])"+re.escape(k)+r"=[^\s]+",
                    lambda m: f"{k}={v}", text, count=1)
    # Fixed time step by required highest requested harmonic.
    step=min(20e-6,1/(case["hz"]*64))
    text=re.sub(r"tran 20u 0\.20 0 20u",f"tran {step:.10g} 0.22 0 {step:.10g}",text)
    (run_dir/"case.cir").write_text(text)
    (run_dir/"case.json").write_text(json.dumps({**case,"parameters":d,
                    "provenance":"NGSPICE_EXPLORATORY"},indent=2))
    return run_dir/"case.cir"


def parse_wrdata(path,expected):
    # ngspice 'set wr_singlescale; set wr_vecnames' writes one scale column.
    lines=Path(path).read_text(errors="replace").splitlines()
    data=[]
    for line in lines:
        try:
            arr=[float(x.replace("D","E")) for x in line.split()]
            if len(arr)>1 and all(math.isfinite(v) for v in arr):
                data.append(arr)
        except ValueError:
            continue
    if len(data)<3:raise RuntimeError(f"{path}: fewer than three waveform samples")
    widths=[len(x) for x in data]
    width=max(set(widths),key=widths.count)
    rows=np.array([x for x in data if len(x)==width],dtype=float)
    # Older ngspice may write repeated X columns, use last format when necessary.
    if rows.shape[1]==1+expected:return rows
    if rows.shape[1]==2*expected:return rows[:,::2] if False else np.column_stack(
        [rows[:,0]]+[rows[:,2*i+1] for i in range(expected)])
    if rows.shape[1]>1+expected:return rows[:,:1+expected]
    raise RuntimeError(f"{path}: waveform width {rows.shape[1]} != {expected+1}")


def harmonics(t,y,hz):
    # FFT-frequency-aligned synchronous projection, after settling. Absolute
    # results depend on provisional tube model and source level calibration.
    tend=float(t[-1])
    cycles=min(24,max(3,int(.08*hz)))
    duration=cycles/hz
    start=max(t[0],tend-duration)
    rate=max(96_000,hz*64)
    times=np.arange(start,tend,1/rate)
    signal=np.interp(times,t,y)-np.mean(np.interp(times,t,y))
    # Exact integer-period resample mitigates leakage from adaptive transient mesh.
    last=times[-1]-times[0] if len(times)>1 else 0.
    if len(signal)<96:raise ValueError("Insufficient coherent analysis window")
    results={}
    amplitudes=[]
    for k in range(1,21):
        if k*hz>=rate*.46:
            results[f"K{k}_dBc"]=None;continue
        phase=np.exp(-2j*np.pi*k*hz*times)
        amplitude=abs(2/len(signal)*np.dot(signal,phase))
        amplitudes.append((k,amplitude))
    a1=next((a for k,a in amplitudes if k==1),1e-30)
    for k,a in amplitudes:
        results[f"K{k}_dBc"]=round(20*np.log10(max(a,1e-18)/max(a1,1e-18)),4) if k>1 else 0.
    thd=np.sqrt(sum(a*a for k,a in amplitudes if k>=2))/max(a1,1e-20)
    results.update(fundamental_peak_volts=float(a1),THD_percent=float(100*thd),
                   fundamental_output_dBu=float(20*math.log10(max(a1,1e-18)/math.sqrt(2)/.775)),
                   measured_cycles=int(cycles),sample_count=int(len(signal)))
    return results


def op_nodes(text):
    d={}
    # ngspice 'print v(x)' format: v(x) = 1.234e+02
    for name,value in re.findall(r"\b(v\([^\)]+\))\s*=\s*([-+\d.eEdD]+)",text,re.I):
        try:d[name.lower()]=float(value.replace("D","E"))
        except ValueError:pass
    return d


def analyze_ac(file):
    """Actual AC response: complex transfer, unwrapped phase and group delay.

    The baseline AC analysis linearizes at quiescent bias, NOT at sustained
    20 dB gain reduction. Under-GR AC is a separate required later test.
    """
    rows=parse_wrdata(file,2)
    frequency=rows[:,0]
    valid=(frequency>0)&np.isfinite(frequency)
    frequency=frequency[valid]
    z=rows[valid,1]+1j*rows[valid,2]
    magnitude_db=20*np.log10(np.maximum(np.abs(z),1e-30))
    phase=np.unwrap(np.angle(z))
    group_delay_s=-np.gradient(phase,2*np.pi*frequency)
    index=int(np.argmin(np.abs(frequency-1000)))
    relative=magnitude_db-magnitude_db[index]
    fields=["frequency_hz","magnitude_dB","relative_to_1k_dB",
            "phase_deg_unwrapped","group_delay_ms"]
    with (Path(file).parent/"frequency_phase.csv").open("w",newline="") as f:
        writer=csv.writer(f);writer.writerow(fields)
        writer.writerows(zip(frequency,magnitude_db,relative,
                             np.degrees(phase),group_delay_s*1000))
    def at(freq):
        idx=int(np.argmin(abs(frequency-freq)))
        return dict(frequency_hz=round(float(frequency[idx]),2),
                    relative_gain_dB=round(float(relative[idx]),4),
                    phase_deg=round(float(np.degrees(phase[idx])),4),
                    group_delay_ms=round(float(group_delay_s[idx]*1000),6))
    return {
        "state":"LINEARIZED_IDLE_AC",
        "frequency_response_points":int(len(frequency)),
        "midband_gain_dB":float(magnitude_db[index]),
        "max_relative_gain_dB":float(np.max(relative)),
        "min_relative_gain_dB":float(np.min(relative)),
        "frequencies":{str(f):at(f) for f in
                         [20,30,50,100,500,1000,5000,10000,15000,20000]}
    }


def simulate(case,root):
    tag=(f"{case['mode']}_F{case['hz']}_L{case['dbu']}_"
         f"A{case['attack']}_R{case['recovery']}_H{int(case['hold'])}_"
         f"I{int(100*case['input_knob'])}").replace("-","m")
    wd=root/"raw"/tag
    deck=prepare(case,wd)
    try:
        completed=subprocess.run(["ngspice","-b","case.cir"],cwd=wd,timeout=100,
                                 text=True,capture_output=True,check=False)
    except subprocess.TimeoutExpired as exc:
        return {**case,"status":"TIMEOUT","message":str(exc)}
    log=(completed.stdout+"\n"+completed.stderr)
    (wd/"ngspice.log").write_text(log)
    if completed.returncode !=0 or not (wd/"tran.txt").exists():
        return {**case,"status":"FAILED","message":log[-2500:]}
    try:
        table=parse_wrdata(wd/"tran.txt",len(OUTFIELDS))
        t,y=table[:,0],table[:,1]
        h=harmonics(t,y,case["hz"])
        # Every nonlinear tube plate path is retained as a separate distortion
        # fingerprint. These are node voltages, not isolated physical tube THD.
        h["stage_harmonics"]={}
        for node,col in (("6BC8_A_plate",4),("6BC8_B_plate",5),
                         ("6CG7_A_plate",6),("6CG7_B_plate",7)):
            local=harmonics(t,table[:,col],case["hz"])
            h["stage_harmonics"][node]={
                "THD_percent":local["THD_percent"],
                "fundamental_peak_volts":local["fundamental_peak_volts"],
                **{f"K{k}_dBc":local[f"K{k}_dBc"] for k in range(2,21)}
            }
        input_level=np.sqrt(np.mean(table[-max(10,len(table)//4):,2]**2))
        in_dbu=20*np.log10(max(input_level,1e-18)/.775)
        h["measured_input_dBu"]=float(in_dbu)
        h["input_to_output_gain_dB"]=float(h["fundamental_output_dBu"]-in_dbu)
        h["control_voltage_end_V"]=float(table[-1,3])
        h["control_voltage_min_V"]=float(np.min(table[:,3]))
        # Approximate late-time DC component at every tube electrode under GR.
        # Fourier DC average in last 10% of run is NOT a formal .op at compression.
        final=table[-max(25,len(table)//10):]
        h["operating_level_nodes_V"]={
            field:float(np.mean(final[:,idx+1]))
            for idx,field in enumerate(OUTFIELDS[2:],start=2)
        }
        h["op_nodes"]=op_nodes(log)
        h["ac"]=analyze_ac(wd/"ac.txt")
        h["status"]="OK";h["provenance"]="NGSPICE_EXPLORATORY"
        h["model_sha256"]=sha256(LIB)
        h["deck_sha256"]=sha256(deck)
        return {**case,**h}
    except Exception as e:
        return {**case,"status":"ANALYSIS_FAILED","message":str(e)}


def write_results(root,rows,args):
    root.mkdir(parents=True,exist_ok=True)
    manifest={
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "program":"ngspice + Python Fourier pipeline",
        "provenance":"NGSPICE_EXPLORATORY (NOT HISTORICAL REFERENCE)",
        "tube_model_sha256":sha256(LIB),
        "source_netlist_sha256":sha256(BASE_DECK),
        "total":len(rows),"passed":sum(x.get("status")=="OK" for x in rows),
        "failed":sum(x.get("status")!="OK" for x in rows),
        "program_configuration":vars(args)
    }
    (root/"manifest.json").write_text(json.dumps(manifest,indent=2))
    (root/"cases.json").write_text(json.dumps(rows,indent=2))
    keys=["mode","hz","dbu","attack","recovery","hold","input_knob",
          "status","input_to_output_gain_dB","THD_percent","control_voltage_end_V",
          "fundamental_output_dBu"]+[f"K{i}_dBc" for i in range(2,21)]
    with (root/"measurement_table.csv").open("w",newline="") as f:
        w=csv.DictWriter(f,fieldnames=keys,extrasaction="ignore")
        w.writeheader();w.writerows(rows)
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        goods=[r for r in rows if r.get("status")=="OK"]
        fig,ax=plt.subplots(figsize=(10,6))
        for mode in ["A-436B","RS124"]:
            subset=sorted((r for r in goods if r["mode"]==mode and r["hz"]==1000
                           and r["attack"]==0 and r["recovery"]==0
                           and not r["hold"]),key=lambda r:r["dbu"])
            if subset:ax.plot([r["dbu"] for r in subset],
                [r["fundamental_output_dBu"] for r in subset],marker="o",label=mode)
        ax.set(xlabel="Source level (dBu)",ylabel="Measured output (dBu)",
               title="EXPLORATORY SPICE: Input vs output at 1 kHz")
        ax.grid(True);ax.legend();fig.tight_layout()
        fig.savefig(root/"transfer_1k.png",dpi=150);plt.close(fig)
        fig,ax=plt.subplots(figsize=(10,6))
        for mode in ["A-436B","RS124"]:
            subset=sorted((r for r in goods if r["mode"]==mode and r["hz"]==1000
                           and r["attack"]==0 and r["recovery"]==0),key=lambda r:r["dbu"])
            if subset:ax.plot([r["dbu"] for r in subset],
                [r["THD_percent"] for r in subset],marker="o",label=mode)
        ax.set(xlabel="Source level (dBu)",ylabel="THD (%)",
               title="EXPLORATORY nonlinear cells: THD vs 1 kHz level")
        ax.grid(True);ax.legend();fig.tight_layout()
        fig.savefig(root/"thd_1k.png",dpi=150);plt.close(fig)
    except Exception as e:
        (root/"plot_warning.txt").write_text(repr(e))
    print(json.dumps(manifest,indent=2))


def main():
    parser=argparse.ArgumentParser()
    grp=parser.add_mutually_exclusive_group()
    grp.add_argument("--quick",action="store_true")
    grp.add_argument("--broad",action="store_true")
    parser.add_argument("--output",default="output")
    args=parser.parse_args()
    root=Path(args.output).resolve()
    cases=mk_cases(args.broad)
    results=[]
    print(f"Running {len(cases)} genuine ngspice circuit analysis cases",flush=True)
    for i,case in enumerate(cases,1):
        result=simulate(case,root)
        results.append(result)
        print(f"{i:3}/{len(cases)} {case['mode']:7} {case['hz']:5}Hz "
              f"{case['dbu']:+4}dBu : {result['status']}",flush=True)
    write_results(root,results,args)
    if any(r["status"]!="OK" for r in results):
        sys.exit(2)


if __name__=="__main__":main()
