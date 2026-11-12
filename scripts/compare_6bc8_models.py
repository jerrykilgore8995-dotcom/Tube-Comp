#!/usr/bin/env python3
"""Compare actual ngspice DC curves for competing 6BC8 nonlinear models.

Neither is declared calibrated to 436B. Produces per-tube voltages, plate
currents and gm curves, with source hashes.
"""
from pathlib import Path
import csv,hashlib,json,subprocess,shutil
import numpy as np
from run_lab import ROOT,LIB

ALT=ROOT/"spice/models/6bc8_sylvania_curve_candidate.lib"
def sweep(root,key,model,library,vg):
    out=root/key/f"Vg_{vg}".replace("-","m")
    out.mkdir(parents=True,exist_ok=True)
    (out/"models").mkdir(exist_ok=True)
    shutil.copy2(library,out/"models"/Path(library).name)
    circuit=f"""6BC8 candidate plate characteristic - real ngspice
.include "models/{Path(library).name}"
VP plate 0 100
VG grid 0 {vg}
X1 plate grid 0 {model}
.control
set wr_singlescale
set wr_vecnames
dc VP 5 275 5
wrdata dc.txt i(VP)
quit
.endc
.end
"""
    (out/"circuit.cir").write_text(circuit)
    res=subprocess.run(["ngspice","-b","circuit.cir"],cwd=out,text=True,capture_output=True,timeout=40)
    (out/"spice.log").write_text(res.stdout+"\n"+res.stderr)
    if res.returncode:raise RuntimeError(res.stdout+res.stderr)
    arr=np.loadtxt(out/"dc.txt",skiprows=1)
    if arr.ndim!=2 or arr.shape[1]<2:raise RuntimeError("Invalid DC output")
    return [(float(v),float(-i*1e3)) for v,i in arr[:,:2]]

def main():
    root=ROOT/"output/6bc8_model_comparison"
    root.mkdir(parents=True,exist_ok=True)
    rows=[]
    for key,model,library in [
            ("initial","V6BC8",LIB),("sylvania_fit","V6BC8_ALT",ALT)]:
        for vg in [-.5,-1,-2,-3,-4,-5,-6,-7,-8,-10]:
            for plate,current in sweep(root,key,model,library,vg):
                rows.append(dict(model=key,grid_V=vg,plate_V=plate,
                                 current_mA=current))
        print("Completed",key,"real ngspice sweeps",flush=True)
    with (root/"comparison.csv").open("w",newline="") as f:
        w=csv.DictWriter(f,fieldnames=rows[0].keys());w.writeheader();w.writerows(rows)
    summary={"provenance":"NGSPICE_EXPLORATORY","calibration":"NOT CALIBRATED TO HARDWARE",
             "total_points":len(rows),
             "models":[{"name":"initial","sha256":hashlib.sha256(LIB.read_bytes()).hexdigest()},
                       {"name":"sylvania_fit","sha256":hashlib.sha256(ALT.read_bytes()).hexdigest()}]}
    (root/"manifest.json").write_text(json.dumps(summary,indent=2))
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        fig,axes=plt.subplots(1,2,figsize=(13,5),sharey=True)
        for ax,(key,name) in zip(axes,[("initial","Provisional guess"),
                                      ("sylvania_fit","Sylvania Koren parameter candidate")]):
            for vg in [-.5,-1,-2,-3,-4,-5,-6,-7,-8,-10]:
                xs=[r for r in rows if r["model"]==key and r["grid_V"]==vg]
                ax.plot([r["plate_V"] for r in xs],[r["current_mA"] for r in xs],label=f"{vg}V")
            ax.set(xlabel="Vplate (V)",ylabel="Iplate (mA)",
                   title=name+" - not hardware calibrated")
            ax.grid(True)
        axes[1].legend(fontsize=8,ncol=2)
        fig.tight_layout();fig.savefig(root/"comparison.png",dpi=140);plt.close(fig)
    except ImportError:pass
    print(summary)
if __name__=="__main__":main()
