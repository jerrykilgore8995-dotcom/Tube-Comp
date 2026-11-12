#!/usr/bin/env python3
"""Fit LINEAR output-transformer proxies against factory 436B bandwidth.

The original Peerless 16402 turns ratio 6.2:1 and replacement ~650 ohm
primary winding DCR remain fixed. We vary primary magnetizing inductance and
winding coupling because the earlier 80H/.995 implementation failed
factory ±1.5dB 30Hz-15kHz response. This is a provisional behavioral
parameter fit, not a measured core hysteresis or original winding model.
"""
import csv,json
from pathlib import Path
import numpy as np
import static_cv_ac
from run_lab import BASE_DECK
ROOT=Path("output/output_transformer_fit");ROOT.mkdir(exist_ok=True,parents=True)
original=BASE_DECK.read_text()
rows=[]
for half_h in (20,30,40,50,60):
 for k in (.995,.998,.999,.9995,.9999):
    s=original
    for needle,replace in [
       ("LPRI_A priA p2a 40",f"LPRI_A priA p2a {half_h}"),
       ("LPRI_B p2b priB 40",f"LPRI_B p2b priB {half_h}"),
       ("LSEC secOut 0 4.16",f"LSEC secOut 0 {2.08*half_h/20:.5f}"),
       ("KPP LPRI_A LPRI_B 0.9995",f"KPP LPRI_A LPRI_B {k}"),
       ("KPSA LPRI_A LSEC 0.9995",f"KPSA LPRI_A LSEC {k}"),
       ("KPSB LPRI_B LSEC 0.9995",f"KPSB LPRI_B LSEC {k}"),
    ]:
        if needle not in s:raise RuntimeError("Missing model anchor: "+needle)
        s=s.replace(needle,replace)
    candidate=ROOT/f"transformer_L{half_h}_K{str(k).replace('.','p')}.cir"
    candidate.write_text(s)
    static_cv_ac.BASE_DECK=candidate
    for cv in (0,-15):
        wd,f,g,p,gd,op=static_cv_ac.simulate("A-436B",cv,ROOT/"raw")
        ix=int(np.argmin(abs(f-1000)))
        relative=g-g[ix]
        freqs=[30,100,1000,5000,10000,15000,20000]
        values={}
        for spot in freqs:
            index=int(np.argmin(np.abs(f-spot)))
            values[f"gain_{spot}Hz_dB"]=float(relative[index])
            values[f"phase_{spot}Hz_deg"]=float(p[index])
        limit=(f>=30)&(f<=15000)
        lo=float(min(relative[limit]));hi=float(max(relative[limit]))
        penalty=max(0,-1.5-lo)**2+max(0,hi-1.5)**2
        row=dict(half_primary_H=half_h,total_primary_H=4*half_h,
            secondary_H=2.08*half_h/20,coupling_k=k,cv_V=cv,
            low_30to15k_dB=lo,high_30to15k_dB=hi,
            bandwidth_pass=lo>=-1.5 and hi<=1.5,
            bandwidth_penalty=penalty,
            outputplate_V=op.get("v(p2a)"),**values)
        rows.append(row)
        print(f"OUTPUT_XFMR Lp={half_h*4}H K={k} CV={cv} "
              f"30Hz={values['gain_30Hz_dB']:+.2f}dB "
              f"15kHz={values['gain_15000Hz_dB']:+.2f}dB "
              f"band=[{lo:+.2f},{hi:+.2f}] pass={row['bandwidth_pass']}",
              flush=True)
with (ROOT/"transformer_AC_fit.csv").open("w",newline="") as fh:
    cols=list(rows[0]);w=csv.DictWriter(fh,fieldnames=cols)
    w.writeheader();w.writerows(rows)
ranks={}
for row in rows:
    t=(row["half_primary_H"],row["coupling_k"])
    ranks[t]=ranks.get(t,0)+row["bandwidth_penalty"]
best=sorted(ranks.items(),key=lambda item:item[1])[:10]
(ROOT/"top_transformer_fit.json").write_text(json.dumps([
    dict(primary_total_H=k[0]*4,k=k[1],squared_excess_dB2=penalty)
    for k,penalty in best],indent=2))
print("OUTPUT_XFMR_BEST",best,flush=True)
