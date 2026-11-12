#!/usr/bin/env python3
"""Propose timing coefficients from independent actual ngspice and DSP runs.

This does NOT fabricate a passing result. It changes only the equivalent
digital attack/release one-pole constants used by the feedback-control law.
The CI job must re-render the corrected DSP and pass 10%, 63%, 90% timing,
steady control voltage, and HOLD against real ngspice before it can commit.
"""
import argparse,csv,json,math,re
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument("--reference-dir",required=True)
p.add_argument("--dsp-dir",required=True)
p.add_argument("--engine",default="plugin/TubeCompDSP.h")
p.add_argument("--output",default="output/timing_fitted")
args=p.parse_args()
src=Path(args.reference_dir)
dsp=Path(args.dsp_dir)
dest=Path(args.output);dest.mkdir(parents=True,exist_ok=True)
def csvrows(p):
 with p.open(newline="") as f:return list(csv.DictReader(f))
def keyed(rows):
 def key(r):
  return (r["mode"],int(r["attack"]),int(r["recovery"]),
          str(r["hold"]).lower() in ("true","1"))
 return {key(r):r for r in rows}
a=keyed(csvrows(src/"attack"/"timing.csv"))
d=keyed(csvrows(dsp/"attack"/"measured_dsp_timing.csv"))
r=keyed(csvrows(src/"recovery"/"timing.csv"))
rd=keyed(csvrows(dsp/"recovery"/"measured_dsp_timing.csv"))
oldAttack=[12.,22.,33.,47.,68.,100.,150.,220.,330.]
oldRelease=[180.,330.,680.,1000.,1800.,3300.]
def adjust(old,desired,measured,label):
 if not math.isfinite(desired) or not math.isfinite(measured) or measured<.1:
  raise RuntimeError(f"No valid real SPICE/DSP {label}: {desired} {measured}")
 ratio=desired/measured
 if ratio<.15 or ratio>6.:
  raise RuntimeError(f"Unphysical ratio for {label}: {ratio}")
 return max(.05,old*ratio)
newAttack=[]
for i,nominal in enumerate(oldAttack):
 k=("RS124",i,3,False)
 if k not in a or k not in d:raise RuntimeError(f"Unmeasured attack {k}")
 x=a[k];y=d[k]
 if x["status"]!="OK":raise RuntimeError(f"Unmeasured SPICE attack {k}")
 newAttack.append(adjust(nominal,float(x["attack63_ms"]),
                         float(y["attack63_ms"]),f"attack {i}"))
newRelease=[]
for i,nominal in enumerate(oldRelease):
 k=("RS124",4,i,False)
 if k not in r or k not in rd:raise RuntimeError(f"Unmeasured recovery {k}")
 x=r[k];y=rd[k]
 if x["status"]!="OK":raise RuntimeError(f"Unmeasured SPICE release {k}")
 newRelease.append(adjust(nominal,float(x["recovery63_ms"]),
                          float(y["recovery63_ms"]),f"recovery {i}"))
k=("A-436B",0,3,False)
if k not in a or k not in d:raise RuntimeError("No Altec SPICE reference")
newAltecAttack=adjust(50.,float(a[k]["attack63_ms"]),
                     float(d[k]["attack63_ms"]),"Altec attack")
newAltecRelease=adjust(1000.,float(a[k]["recovery63_ms"]),
                      float(d[k]["recovery63_ms"]),"Altec release")
source=Path(args.engine)
text=source.read_text()
def fmt(x):return f"{x:.6f}f"
def replace_array(name,new):
 global text
 needle=r"(static constexpr std::array<float,\d+>\s+"+name+
        r"\s*=\s*\{\{)(.*?)(\}\};)"
 m=re.search(needle,text,re.S)
 if not m:raise RuntimeError("Cannot locate DSP array "+name)
 if len(re.findall(r"[-+]?(?:\d*\.)?\d+f",m.group(2)))!=len(new):
  raise RuntimeError("Array unexpectedly changed "+name)
 text=text[:m.start()]+m.group(1)+"\n    "+",".join(fmt(x) for x in new)+"\n"+m.group(3)+text[m.end():]
replace_array("rsAttackMs",newAttack)
replace_array("rsReleaseMs",newRelease)
old='p.rs124?rsAttackMs[static_cast<size_t>(p.attack)]:50.f;'
if text.count(old)!=1:raise RuntimeError("Altec attack DSP anchor missing")
text=text.replace(old,
       'p.rs124?rsAttackMs[static_cast<size_t>(p.attack)]:'+fmt(newAltecAttack)+';')
old='rsReleaseMs[static_cast<size_t>(recoveryNumber(p.recovery)-1)]:1000.f;'
if text.count(old)!=1:raise RuntimeError("Altec release DSP anchor missing")
text=text.replace(old,
       'rsReleaseMs[static_cast<size_t>(recoveryNumber(p.recovery)-1)]:'+fmt(newAltecRelease)+';')
source.write_text(text)
proposal=dict(rs_attack_ms=newAttack,rs_release_ms=newRelease,
              altec_attack_ms=newAltecAttack,altec_release_ms=newAltecRelease,
              provenance="independent same-source .tran/SPICE and 44.1k C++ DSP")
(dest/"proposal.json").write_text(json.dumps(proposal,indent=2))
print("TIMING_CALIBRATION_PROPOSAL",json.dumps(proposal),flush=True)
