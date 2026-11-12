#!/usr/bin/env python3
"""Actual .AC circuit linearization at forced compressor control voltages.

Exploratory 436-like topology only: gain-reduction-dependent frequency/phase
and group delay; preserves entire triode network without fake static EQ.
"""
from pathlib import Path
import argparse,csv,json,re,shutil,subprocess
import numpy as np
from run_lab import BASE_DECK,LIB,parse_wrdata,op_nodes

def simulate(mode,cv,root,load_ohms=600):
    tag=f"{mode}_CV{cv:.2f}_Z{load_ohms}".replace("-","m")
    wd=root/tag;wd.mkdir(parents=True,exist_ok=True)
    (wd/"models").mkdir(exist_ok=True)
    shutil.copy2(LIB,wd/"models/tube_cells.lib")
    s=BASE_DECK.read_text()
    if load_ohms not in (200,600):
        raise ValueError("Output load must be 200 or 600ohm")
    old_load="RLOAD secOut 0 600"
    if old_load not in s:
        raise RuntimeError("Missing transformer secondary resistor in netlist")
    s=s.replace(old_load,f"RLOAD secOut 0 {load_ohms}",1)
    # Freeze sidechain charge at a supplied bias. This is not a true compressed
    # transient simulation, but a valid small-signal AC linearization around
    # the corresponding nonlinearly biased tube operating point.
    s=re.sub(r'^R12 detector cv \{ATTACK_R\}$','* forced-bias AC removes R12',s,flags=re.M)
    s=re.sub(r'^C4 cv 0 1u$','* forced-bias AC removes C4',s,flags=re.M)
    s=re.sub(r'^R9 cv 0 \{REC_R\}$','* forced-bias AC removes R9',s,flags=re.M)
    s=s.replace('* Simulations are not hardware-calibrated',
        f'VFORCE cv 0 {cv}\n* Simulations are not hardware-calibrated')
    s=s.replace('BIAS=-0.8',f'BIAS={-0.8 if mode=="A-436B" else -1.0}')
    s=s.replace('OUTPAD=1',f'OUTPAD={1.0 if mode=="A-436B" else 0.65}')
    s=s.replace('ac dec 16 20 20000','ac dec 36 20 20000')
    s=re.sub(r'\ntran 20u 0.20 0 20u\nwrdata tran.txt[^\n]+',
             '',s)
    (wd/"ac.cir").write_text(s)
    p=subprocess.run(["ngspice","-b","ac.cir"],cwd=wd,text=True,
                     capture_output=True,timeout=45)
    (wd/"ngspice.log").write_text(p.stdout+"\n"+p.stderr)
    if p.returncode or not (wd/"ac.txt").exists():
        raise RuntimeError("ngspice failed for "+tag+"\n"+(p.stdout+p.stderr)[-1200:])
    a=parse_wrdata(wd/"ac.txt",2)
    f=a[:,0];z=a[:,1]+1j*a[:,2]
    gain=20*np.log10(np.maximum(abs(z),1.e-30))
    phase=np.unwrap(np.angle(z))
    delay_ms=-np.gradient(phase,2*np.pi*f)*1000
    return wd,f,gain,np.degrees(phase),delay_ms,op_nodes((wd/"ngspice.log").read_text())

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--output",default="output/static_bias_ac")
    p.add_argument("--broad",action="store_true")
    args=p.parse_args();root=Path(args.output);root.mkdir(parents=True,exist_ok=True)
    cvs=np.arange(0,-40.1,-1.0) if args.broad else [0,-1,-3,-5,-8,-12,-16,-20,-25,-30,-35]
    allrows=[];summary=[]
    for mode in ["A-436B","RS124"]:
        curves=[]
        for cv in cvs:
            wd,f,g,ph,gd,op=simulate(mode,float(cv),root/"raw")
            k=int(np.argmin(abs(f-1000)))
            gain1k=float(g[k]);curves.append((float(cv),gain1k))
            for fi,gg,pd,delay in zip(f,g,ph,gd):
                allrows.append((mode,float(cv),float(fi),float(gg),float(pd),float(delay)))
            row=dict(mode=mode,sidechain_control_V=float(cv),
                     one_kHz_gain_dB=gain1k,plate_6BC8_A_V=op.get("v(pa)"),
                     plate_6BC8_B_V=op.get("v(pb)"),
                     plate_6CG7_A_V=op.get("v(p2a)"),
                     plate_6CG7_B_V=op.get("v(p2b)"),
                     cathode_6BC8_V=op.get("v(k1)"),cathode_6CG7_V=op.get("v(k2)"))
            for spot in (20,50,100,1000,5000,10000,20000):
                ix=int(np.argmin(abs(f-spot)))
                row[f"gain_{spot}Hz_dB"]=float(g[ix])
                row[f"phase_{spot}Hz_deg"]=float(ph[ix])
            summary.append(row)
            print(mode,cv,"gain1k",round(gain1k,3),"6BC8plate",op.get("v(pa)"),flush=True)
        zero=curves[0][1]
        for row in summary:
            if row["mode"]==mode:
                row["GR_vs_idle_dB"]=zero-row["one_kHz_gain_dB"]
    with (root/"frequency_phase_vs_control.csv").open("w",newline="") as file:
        w=csv.writer(file)
        w.writerow(["mode","sidechain_control_V","frequency_hz","gain_dB",
                    "phase_deg_unwrapped","group_delay_ms"])
        w.writerows(allrows)
    with (root/"bias_operating_points.json").open("w") as file:
        json.dump(summary,file,indent=2)
    with (root/"bias_operating_points.csv").open("w",newline="") as file:
        w=csv.DictWriter(file,fieldnames=summary[0].keys());w.writeheader();w.writerows(summary)
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        for mode in ["A-436B","RS124"]:
            fig,ax=plt.subplots(figsize=(11,6))
            for cv in cvs:
                rows=[x for x in allrows if x[0]==mode and x[1]==float(cv)]
                if not rows:continue
                base=[x[3] for x in rows if 950<x[2]<1050]
                if not base:continue
                ax.semilogx([x[2] for x in rows],
                            [x[3]-base[0] for x in rows],label=f"CV {cv:.1f} V")
            ax.set(xlabel="Hz",ylabel="dB vs near-1 kHz",
                   title=f"PROVISIONAL {mode}: AC response vs control voltage")
            ax.grid(True);ax.legend(ncol=3,fontsize=8)
            fig.tight_layout()
            fig.savefig(root/f"{mode}_FR_vs_GR.png",dpi=140)
            plt.close(fig)
    except ImportError:pass
if __name__=="__main__":main()
