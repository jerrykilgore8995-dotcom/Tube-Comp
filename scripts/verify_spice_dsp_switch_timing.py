#!/usr/bin/env python3
"""Independent timing parity: real 6AL5 ngspice transient vs actual DSP CV.

Uses identical 1kHz source, drive, input control, 1ms gate,
100ms onset and 1.6s off-time. Input files MUST be measured CSV
artifacts (different SPICE and native C++ code paths).
5..8% tolerance on each resolvable 10/63/90% attack/recovery.
HOLD uses a 1% absolute retained-control-fraction limit, with
CV plateau matched at 8% when clearly above 0.1V.
"""
import argparse,csv,json,math
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument("--reference-dir",required=True)
p.add_argument("--dsp-dir",required=True)
p.add_argument("--output",default="output/switch_timing_parity")
p.add_argument("--tolerance",type=float,default=8.)
args=p.parse_args()
source=Path(args.reference_dir);dsp=Path(args.dsp_dir)
dest=Path(args.output);dest.mkdir(parents=True,exist_ok=True)
def load(p):
 with p.open(newline="") as f:return list(csv.DictReader(f))
fields=("attack10_ms","attack63_ms","attack90_ms",
        "recovery63_ms","recovery90_ms")
rows=[];missing=[];failed=[]
for family in ("attack","recovery","hold"):
 ref=load(source/family/"timing.csv")
 actual=load(dsp/family/"measured_dsp_timing.csv")
 def key(x):
  return (x["mode"],int(x["attack"]),int(x["recovery"]),
           str(x["hold"]).lower() in ("1","true"))
 exp={key(x):x for x in ref}
 got={key(x):x for x in actual}
 if len(exp)!=len(ref) or len(got)!=len(actual):
  raise RuntimeError("Duplicated switch setting in "+family)
 if set(exp)!=set(got):
  raise RuntimeError("Missing or extra switch settings "+family+
        " missing "+str(set(exp)-set(got))+
        " extra "+str(set(got)-set(exp)))
 for condition,r in exp.items():
  m=got[condition]
  if r.get("status")!="OK":raise RuntimeError("ngspice case not simulated "+str(r))
  def compare(label,tolerance,args_):
   expected=float(r[label]);rendered=float(m[label])
   if not math.isfinite(expected) or not math.isfinite(rendered):
    if family=="hold" and label in ("recovery63_ms","recovery90_ms"):
     return
    failed.append(dict(family=family,key=condition,metric=label,
        error="NONFINITE_MEASURED_TIME",expected=expected,actual=rendered))
    return
   if args_=="ratio":
    err=100*abs(rendered-expected)/max(.1,abs(expected))
   elif args_=="absolute_hold":
    err=100*abs(rendered-expected)
   else:raise RuntimeError(args_)
   value=dict(family=family,mode=condition[0],attack=condition[1],
    recovery=condition[2],hold=int(condition[3]),metric=label,
    spice=expected,dsp=rendered,error_pct=err,
    passed=err<=tolerance)
   rows.append(value)
   if not value["passed"]:failed.append(value)
  for name in ("attack10_ms","attack63_ms","attack90_ms"):
   compare(name,args.tolerance,"ratio")
  if family!="hold":
   for name in ("recovery63_ms","recovery90_ms"):
    compare(name,args.tolerance,"ratio")
  else:
   compare("hold_fraction",1.,"absolute_hold")
  compare("plateau_cv_V",args.tolerance,"ratio")
cols=["family","mode","attack","recovery","hold","metric",
      "spice","dsp","error_pct","passed"]
with (dest/"timing_by_setting.csv").open("w",newline="") as f:
 w=csv.DictWriter(f,fieldnames=cols);w.writeheader();w.writerows(rows)
summary=dict(calibration="independent SPICE vs realtime DSP",
 cases=sum(len(load(source/f/"timing.csv")) for f in ("attack","recovery","hold")),
 metrics=len(rows),failed_metrics=len(failed),
 worst_error_pct=max((x["error_pct"] for x in rows),default=None),
 tolerance_pct=args.tolerance,failures=sorted(failed,
    key=lambda x:x.get("error_pct",float("inf")),reverse=True)[:45])
(dest/"timing_summary.json").write_text(json.dumps(summary,indent=2))
print("TIMING_PARITY",json.dumps({k:v for k,v in summary.items()
                          if k!="failures"},indent=2),flush=True)
for x in summary["failures"]:print("TIMING_MISMATCH",x,flush=True)
if failed:raise SystemExit(1)
print("PASS ALL MEASURED SWITCH TIMINGS",len(rows))
