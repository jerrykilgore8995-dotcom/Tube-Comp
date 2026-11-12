#!/usr/bin/env python3
"""6BC8/6CG7 independent nonlinear valve plate curves and bias/gm research.

The program uses real ngspice DC sweeps, not measured datasheet points. Curve
comparison to Sylvania and GE datasheets remains a calibration stage.
"""
from pathlib import Path
import argparse,csv,json,subprocess,shutil,math,hashlib
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
LIB=ROOT/"spice/models/tube_cells.lib"

def run_curve(tube,grid,root):
    name=f"{tube}_VG{grid:.1f}".replace("-","m")
    wd=root/name;wd.mkdir(parents=True,exist_ok=True)
    (wd/"models").mkdir(exist_ok=True)
    shutil.copy2(LIB,wd/"models/tube_cells.lib")
    model="V6BC8" if tube=="6BC8" else "V6CG7"
    deck=f"""Tube device DC plate characteristic; provisional / not hardware calibrated
.include "models/tube_cells.lib"
VPL plate 0 100
VGRID grid 0 {grid}
XDEV plate grid 0 {model}
.control
set wr_singlescale
set wr_vecnames
dc VPL 5 275 5
wrdata dc.txt i(VPL)
quit
.endc
.end
"""
    (wd/"tube.cir").write_text(deck)
    r=subprocess.run(["ngspice","-b","tube.cir"],cwd=wd,timeout=30,
                     text=True,capture_output=True)
    (wd/"ngspice.log").write_text(r.stdout+"\n"+r.stderr)
    if r.returncode or not (wd/"dc.txt").exists():
        raise RuntimeError(f"{name}: {(r.stdout+r.stderr)[-1200:]}")
    a=np.loadtxt(wd/"dc.txt",skiprows=1)
    # voltage source i(VPL) is negative for plate current sourced into tube.
    return [(tube,grid,float(v),float(-i*1e3)) for v,i in a[:,:2]]

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--out",default="output/tube_curves")
    args=p.parse_args()
    out=Path(args.out)
    out.mkdir(parents=True,exist_ok=True)
    rows=[]
    for tube in ("6BC8","6CG7"):
        grid_points=([-0.5,-1,-2,-3,-4,-5,-6,-7,-8,-9,-10] if tube=="6BC8"
                     else [0,-2,-4,-6,-8,-10,-12,-16])
        for grid in grid_points:
            x=run_curve(tube,grid,out/"raw")
            rows.extend(x)
            print(f"{tube}, grid {grid} V, {len(x)} actual SPICE points",flush=True)
    with (out/"plate_curves.csv").open("w",newline="") as f:
        w=csv.writer(f)
        w.writerow(["tube","grid_V","plate_V","plate_current_mA"])
        w.writerows(rows)
    # local dIdVg/gm, not generic fixed µ / saturation.
    gm=[]
    for tube in ("6BC8","6CG7"):
        group=[x for x in rows if x[0]==tube]
        for vp in [50,100,150,200,250]:
            slices=sorted((x for x in group if x[2]==vp),key=lambda a:a[1])
            for left,right in zip(slices,slices[1:]):
                slope=(right[3]-left[3])/(right[1]-left[1])
                gm.append((tube,vp,(left[1]+right[1])/2,slope))
    with (out/"transconductance.csv").open("w",newline="") as f:
        w=csv.writer(f);w.writerow(["tube","plate_V","grid_midpoint_V","gm_mA_per_V"])
        w.writerows(gm)
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        for tube in ("6BC8","6CG7"):
            fig,ax=plt.subplots(figsize=(9,6))
            for grid in sorted(set(x[1] for x in rows if x[0]==tube)):
                pts=[x for x in rows if x[0]==tube and x[1]==grid]
                ax.plot([x[2] for x in pts],[x[3] for x in pts],label=f"Vg={grid:g}V")
            ax.set(xlabel="Plate voltage (V)",ylabel="Plate current (mA)",
                   title=f"PROVISIONAL SPICE {tube} plate curves (needs datasheet fitting)")
            ax.grid(True);ax.legend(fontsize=8,ncol=2)
            fig.tight_layout();fig.savefig(out/f"{tube}_plate_curves.png",dpi=140);plt.close(fig)
    except ImportError:pass
    print(f"{len(rows)} provisional DC plate-curve points measured in ngspice.")
if __name__=="__main__":main()
